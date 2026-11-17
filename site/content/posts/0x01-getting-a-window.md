+++ 
draft = false
date = 2026-11-16T18:02:30-06:00
title = "0x01 - Getting a Window"
description = "Getting a window open was harder than I thought."
slug = ""
authors = ['subcursion']
tags = []
categories = []
externalLink = ""
series = []
toc = true
tocOpen = true
+++

# Let's get started... or so I thought.

I really thought getting a window open wouldn’t be hard. I used OpenGL a long time ago, so my expectation was roughly: ask SDL for a window, get an OpenGL context, tell it what size to render at, and then tell it what to draw. A bit of setup, then onto the interesting stuff.

I was not ready for how asynchronous Vulkan is, or how much I would have to describe explicitly before anything could happen. I had never touched it before, and the amount of setup was overwhelming at first.

That was partly the point of choosing it, though. I knew building a tracer would be challenging, and I wanted to learn a graphics API along the way. I’d already tried OpenGL; this felt like an opportunity to try something new.

For this milestone, I deliberately left scene management and compute shaders alone. I wanted the graphics equivalent of “hello world”: a window with a color in it. Writing this down is part of how I’m making sense of what it took to get there.

The code for this milestone is in [PR #5](https://github.com/Subcursion/ether/pull/5).

## Initializing SDL3

I started by installing SDL3 through pacman:

```
sudo pacman -S sdl3
```

### Adding SDL3 to the build

```cmake
find_package(SDL3 CONFIG REQUIRED)
# ...
target_link_libraries(ether PRIVATE Vulkan::Vulkan SDL3::SDL3)
```

This part followed a familiar pattern: install the library, find it with CMake, and link it to Ether.

### Initializing SDL3

This part wasn’t too bad. I used `SDL_Init` and `SDL_CreateWindow`, then stored the window handle in the application state:

```c
ether_rc ether_window_init(ether_app_t *app, const char *window_title, int width, int height)
{
    ether_rc rc = ETHER_SUCCESS;
    PARAM_CHECK(NULL != app, rc = ETHER_EPARAM, goto EXIT);
    PARAM_CHECK(NULL != window_title, rc = ETHER_EPARAM, goto EXIT);
    PARAM_CHECK(0 < width, rc = ETHER_EPARAM, goto EXIT);
    PARAM_CHECK(0 < height, rc = ETHER_EPARAM, goto EXIT);

    ETHER_ILOG("Initializing SDL3");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD))
    {
        ETHER_ELOGV("Failed to initialize SDL3: %s", SDL_GetError());
        rc = ETHER_ESDL_INIT;
        goto EXIT;
    }

    ETHER_ILOG("Creating SDL3 Window");
    app->sdl_window = SDL_CreateWindow(window_title, width, height,
                                       SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (NULL == app->sdl_window)
    {
        ETHER_ELOGV("Failed to create SDL Window: %s", SDL_GetError());
        rc = ETHER_ESDL_INIT;
        goto EXIT;
    }

EXIT:
    return rc;
}
```

If anything goes wrong, add some logging and return error codes. What's important here is the window has to be initialized for Vulkan using `SDL_WINDOW_VULKAN` which will load additional libraries according to `SDL_CreateWindow` documentation:

> If the window is created with any of the SDL_WINDOW_OPENGL or SDL_WINDOW_VULKAN flags, then the corresponding LoadLibrary function (SDL_GL_LoadLibrary or SDL_Vulkan_LoadLibrary) is called and the corresponding UnloadLibrary function is called by SDL_DestroyWindow().
>
> If SDL_WINDOW_VULKAN is specified and there isn't a working Vulkan driver, SDL_CreateWindow() will fail, because SDL_Vulkan_LoadLibrary() will fail.

And that's pretty much it for initial SDL Window creation.

## Initializing Vulkan

Here we go. The functions get fairly large from this point on, so I’ll use excerpts to focus on the important parts. The PR has the full implementation.

### SDL Compatibility & Vulkan Instance

The very first thing I needed to do was get the extensions that SDL exposes for the Vulkan `Instance`. It was also important to add an extension to this list: `VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME`. This allows us to use the newer API to query surface capabilities that we'll see in use later. We also have the opportunity to put Vulkan validation layers in place so we are warned whenever we make a mistake, lose a resource, or otherwise use the graphics API in a way that is not intended or simply, and more commonly, just plain incorrect. Because of the extra checks this causes, I only wanted these enabled when we were making a debug build (which I was going to be doing plenty).

```c
    uint32_t extensionCount = 0;

    const char *const *sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (NULL == sdl_extensions)
    {
        ETHER_ELOGV("Failed to get SDL Vulkan extensions: %s", SDL_GetError());
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }
    // need to copy to our own version of extensions to enable VK_KHR_get_surface_capabilities2
    const char **extensions = malloc(sizeof(char *) * (extensionCount + 1));
    if (NULL == extensions)
    {
        ETHER_ELOGV("Failed to allocate extension list for instance creation: %s", strerror(errno));
        rc = errno;
        goto EXIT;
    }
    memcpy(extensions, sdl_extensions, extensionCount * sizeof(char *));
    extensions[extensionCount] = VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;

    const char *layers[] = {
#ifndef NODEBUG
        "VK_LAYER_KHRONOS_validation"
#endif
    };
    uint32_t layer_count = 0;
#ifndef NODEBUG
    layer_count = 1;
#endif

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Ether",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "Ether",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = TARGET_VULKAN_VERSION};

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = extensionCount + 1,
        .ppEnabledExtensionNames = extensions,
        .enabledLayerCount = layer_count,
        .ppEnabledLayerNames = layers};

    VkInstance instance = VK_NULL_HANDLE;

    VkResult result = vkCreateInstance(&createInfo, NULL, &instance);
    if (result != VK_SUCCESS)
    {
        ETHER_ELOGV("Failed to create Vulkan instance: %s", string_VkResult(result));
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }
    app->vk_instance = instance;
```

This was my first exposure to how Vulkan structures their API calls: passing structs that contain pointers to other structs to functions. In this case we need to describe to Vulkan the instance we want to create including the aforementioned extensions and layers as well as the application information such as our version, name, and what Vulkan API we are targeting. At this time, I chose the most recent: `1.4.0`.

The instance is the starting point for working with Vulkan. There’s a pattern emerging already: create an object so I can create the next object. There’s going to be a lot of that.

### Vulkan Surface

With the instance, I can now create the surface: the virtual surface in which Vulkan knows how to present images.

```c
    ETHER_ILOG("Initializing Vulkan surface");

    if (!SDL_Vulkan_CreateSurface(app->sdl_window, instance, NULL, &surface))
    {
        ETHER_ELOGV("Failed to create Vulkan surface: %s", SDL_GetError());
        rc = ETHER_ESDL_INIT;
        goto EXIT;
    }
    app->vk_surface = surface;
```

Some of the simplest code I'll write today. Love it!

### GPU Devices

Ready to go down some rabbit holes? Me too. Once we have a surface, we need to ensure that whatever GPU device we are going to talk to actually supports the surface, its capabilities, etc. We need the qualities of the surface to be a subset of the GPU driver's features. Thus, I asked Ether to pick a device for me.

```c
    ether_device_t device = {0};

    // pick the best device
    if (ETHER_SUCCESS != (rc = ether_pick_device(app, &device)))
    {
        goto EXIT;
    }
```

`ether_device_t` and `ether_pick_device` are my own types and functions. I’d seen someone else enumerate the available devices and give each one a score, so I started with a similar approach.

As I worked through it, this became as much a compatibility check as a preference system. First, can the device do what Ether needs? Then, among the devices that pass, which one would I prefer? I expect these checks to grow as I support more image formats. For now, here’s where I landed.

First, take a look at what `ether_device_t` is defined as:

```c
typedef struct ether_device
{
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties properties;

    VkSurfaceCapabilitiesKHR surface_capabilities;
    uint32_t surface_format_count;
    VkSurfaceFormat2KHR *surface_formats;
    uint32_t surface_present_mode_count;
    VkPresentModeKHR *surface_present_modes;

    uint32_t graphics_family;
    uint32_t present_family;

    VkDevice logical_device;

    VkQueue graphics_queue;
    VkQueue present_queue;

    bool supports_swapchain;

    /// @brief The score generated when selecting a device
    uint32_t score;
} ether_device_t;
```

There are a lot of Vulkan objects in there and as we step through how I enumerate devices, these fields get filled in as I discover the features of the graphics device I'm scanning.

#### Enumerating Graphics Devices

Vulkan provides a way to do this. This flow you are about to see is common in Vulkan APIs that return an unknown amount of objects. You call them once with the destination pointer being `NULL` and it will return the number of objects it _would_ have populated into the destination pointer; thus, you can allocate the buffer and call it again to get the actual contents.

```c
    uint32_t deviceCount = 0;
    // ...
    if (VK_SUCCESS != (vk_result = vkEnumeratePhysicalDevices(app->vk_instance, &deviceCount, NULL)))
    {
        ETHER_ELOGV("Failed to enumerate physical devices: %s", string_VkResult(vk_result));
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }

    ETHER_ILOGV("Found %d physical devices", deviceCount);
    if (0 == deviceCount)
    {
        ETHER_ELOG("No GPU devices to use.");
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }

    devices = malloc(sizeof(VkPhysicalDevice) * deviceCount);
    if (NULL == devices)
    {
        ETHER_ELOGV("Physical Device list allocation failed: %s", strerror(errno));
        rc = errno;
        goto EXIT;
    }
    ether_devices = calloc(deviceCount, sizeof(ether_device_t));
    if (NULL == ether_devices)
    {
        ETHER_ELOGV("Ether Device list allocation failed: %s", strerror(errno));
        rc = errno;
        goto EXIT;
    }

    ETHER_ILOG("Enumerating devices for compatability...");

    if (VK_SUCCESS != vkEnumeratePhysicalDevices(app->vk_instance, &deviceCount, devices))
    {
        ETHER_ELOGV("Failed to enumerate physical devices: %s", string_VkResult(vk_result));
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }
```

Now that we have populated the `VkPhysicalDevice` array (and instantiated our own `ether_device_t` array) we can loop over them and see if they're compatible.

```c
    uint32_t di = 0;
    for (; di < deviceCount; di++)
    {
        get_device_compatibility(app, devices[di], ether_devices + di);
    }

    // assign it to the application
    // pick highest scoring device
    for (di = 0; di < deviceCount; di++)
    {
        if (ether_devices[di].score > highest_score)
        {
            best_device = ether_devices + di;
            highest_score = best_device->score;
        }
    }

    if (NULL == best_device)
    {
        ETHER_ELOG("No compatible devices found.");
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }

    ETHER_ILOGV("Picking device: %s", best_device->properties.deviceName);
    ETHER_ILOGV("Graphics Family: %d", best_device->graphics_family);
    ETHER_ILOGV("Present Family: %d", best_device->present_family);
```

The selection itself is simple: pick the highest-scoring compatible device. Most of the work is hiding inside `get_device_compatibility`.

#### Graphics Device Compatibility

Given a `VkPhysicalDevice` I need to ensure that it can actually do the operations I need to do. Let's start with the most obvious and easiest check to make: does the device support the version of Vulkan I am targeting?

##### Version Check

```c
    VkPhysicalDeviceProperties properties = {0};
    // ...
    vkGetPhysicalDeviceProperties(device, &properties);

    // check version
    if (VK_API_VERSION_MAJOR(properties.apiVersion) < VK_API_VERSION_MAJOR(TARGET_VULKAN_VERSION) ||
        VK_API_VERSION_MINOR(properties.apiVersion) < VK_API_VERSION_MINOR(TARGET_VULKAN_VERSION))
    {
        good_version = false;
    }

    ETHER_ENTER_LOG(ETHER_LOG_INFO);
    fprintf(ether_log_stream, "\nPhysical Device:\n\tId: %d\n\tName: %s\n\tType: %s\n\tVulkan: %d.%d.%d (%s)\n",
            properties.deviceID,
            properties.deviceName,
            get_device_type_str(properties.deviceType),
            VK_API_VERSION_MAJOR(properties.apiVersion),
            VK_API_VERSION_MINOR(properties.apiVersion),
            VK_API_VERSION_PATCH(properties.apiVersion),
            good_version ? "PASS" : "FAIL");

    if (!good_version)
    {
        _score = 0;
        goto EXIT;
    }
```

##### Ray Trace Image Format Support

Good so far. Now, for this ray tracer, I am going to render into an image separate from the ones that Vulkan manages for presentation. Why? Mostly so I can do linear color space calculations including HDR then apply resolution scaling, tone-mapping, and any other adjustments before submitting the image for presentation. This allows me to use a lower (or higher) resolution image with extended and predictable capabilities and formats and let the conversion to presentation handle any strange image format nuances.

```c
    /// @brief The ray trace image format we require
    static const VkFormat ray_image_format =
        VK_FORMAT_R16G16B16A16_SFLOAT;
    // ...
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(
        device, ray_image_format, &format_properties);

    // Assuming compute writes followed by a blit to the swapchain.
    VkFormatFeatureFlags ray_image_feature_req =
        VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
        VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
        VK_FORMAT_FEATURE_BLIT_SRC_BIT;

    fprintf(ether_log_stream,
            "\t%s Support: %s\n",
            string_VkFormat(ray_image_format),
            (format_properties.optimalTilingFeatures & ray_image_feature_req) == ray_image_feature_req ? "Yes (PASS)" : "No (FAIL)");

    if ((format_properties.optimalTilingFeatures & ray_image_feature_req) != ray_image_feature_req)
    {
        _score = 0;
        goto EXIT;
    }
```

This really is saying does the device support the format I've passed it with the features to have it be the target for read and write operations (like a compute shader rendering into it), that it can be the source of a transition stage, as well as be the source image in blit operations.

##### Extension Support

Now let's talk extensions. There are a few extensions I decided to require up front (for now, compatibility can be added later): `maintenance1` and `sync2`. The former enables using Fences to gate present notifications in order to secure proper resource cleanup (more on that in the rendering section), while the latter enables better synchronization objects like Barriers which will be important when coordinating work across queues and queue families.

```c
 // check for sync2
    VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR maintenance = {
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR,
        .swapchainMaintenance1 = VK_FALSE};
    VkPhysicalDeviceSynchronization2Features sync2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
        .synchronization2 = VK_FALSE,
        .pNext = &maintenance};
    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &sync2};
    vkGetPhysicalDeviceFeatures2(device, &features);

    fprintf(ether_log_stream, "\tSynchronization2 Support: %s\n",
            sync2.synchronization2
                ? "Yes (PASS)"
                : "No (FAIL)");
    if (!sync2.synchronization2)
    {
        _score = 0;
        goto EXIT;
    }
    fprintf(ether_log_stream, "\tMaintenance1 Support: %s\n",
            maintenance.swapchainMaintenance1 ? "Yes (PASS)" : "No (FAIL)");
    if (!maintenance.swapchainMaintenance1)
    {
        _score = 0;
        goto EXIT;
    }
```

It is worth highlighting using the `.pNext` field here; it is a `void*` pointer designed to allow future expansions to be chained together that the developers couldn't foresee at the time of writing the API calls. This way, the API can be extended without changing the ABI and breaking compatibility. Here, we chain the 2 aforementioned extensions together and set both to `VK_FALSE` so we know for sure they get set to `VK_TRUE` if they are enabled.

##### Device Type Scoring

It's also worth knowing what kind of device we are looking at. Here I assign a point rating for the kind of device it is in order to favor devices more capable and intended to handle the kind of loads I will assign to it

```c
    switch (properties.deviceType)
    {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        _score += 500;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        _score += 100;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        _score += 200;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        _score += 50;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
    case VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM:
    default:
        break;
    }
```

##### Queue Families

Then we look into what kind of queues the graphics device has. My rough mental model is a set of freeways: which vehicles can use each one, and how many lanes are available?

```c
 // check queue families
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, NULL);
    fprintf(ether_log_stream, "\tQueue Families: %d\n", familyCount);

    families = malloc(sizeof(VkQueueFamilyProperties) * familyCount);
    if (NULL == families)
    {
        fprintf(ether_log_stream, "!!! Failed to allocate family properites: %s !!!", strerror(errno));
        _score = 0;
        goto EXIT;
    }

    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families);

    for (uint32_t fi = 0; fi < familyCount; fi++)
    {
        family = &families[fi];

        fprintf(ether_log_stream, "\t\t[%d]\n\t\t\tQueue Count: %d\n", fi, family->queueCount);

        char supportsGraphics = family->queueFlags & VK_QUEUE_GRAPHICS_BIT ? 'G' : '-';
        char supportsCompute = family->queueFlags & VK_QUEUE_COMPUTE_BIT ? 'C' : '-';
        char supportsTransfer = family->queueFlags & VK_QUEUE_TRANSFER_BIT ? 'T' : '-';
        VkBool32 supportsPresent = VK_FALSE;

        result = vkGetPhysicalDeviceSurfaceSupportKHR(device, fi, app->vk_surface, &supportsPresent);
        if (VK_SUCCESS != result)
        {
            supportsPresent = VK_FALSE;
        }

        if (supportsGraphics != '-')
        {
            ether_device->graphics_family = fi;
            _score += 100;
        }
        if (supportsCompute != '-')
        {
            _score += 100;
        }
        if (supportsPresent)
        {
            ether_device->present_family = fi;
            _score += 100;
        }
    // ...
```

As you can see, some of the families will support doing graphics, compute, or transfer operations. They can also support `Present` which is designed to actually render to a surface and, consequently, the screen.

This is also where we find what family we can use for our graphics and present family operations, so we assign them to the device here. The device needs to have at least one of each or one that supports both.

##### Swapchain Support

I also need to ensure the device can do "swapchains". This is what Vulkan refers to when discussing double and triple buffering. Essentially a pool of images to request work for but have specific transition states as you'll see later.

```c
 if (VK_SUCCESS != (result = vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, extensions)))
    {
        fprintf(ether_log_stream, "!!! Failed to get extension properties: %s !!!\n", string_VkResult(result));
        _score = 0; // disable the GPU
        goto EXIT;
    }

    // check for the VK_KHR_SWAPCHAIN_EXTENSION_NAME
    for (uint32_t ei = 0; ei < extension_count; ei++)
    {
        if (strcmp(extensions[ei].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
        {
            ether_device->supports_swapchain = true;
        }
    }
    fprintf(ether_log_stream, "\tSwapchain Support: %s\n", ether_device->supports_swapchain ? "Yes (PASS)" : "No (FAIL)");

    if (!ether_device->supports_swapchain)
    {
        _score = 0;
        goto EXIT;
    }
```

##### Surface Capabilities

We're not done yet. The device needs to support certain surface capabilities (remember `CAPABILITIES_2` from earlier?)

```c
    // now check if the surface capabilities
    VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        .surface = app->vk_surface};
    VkSurfaceCapabilities2KHR _surface_capes = {
        .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR};
    if (VK_SUCCESS != (result = vkGetPhysicalDeviceSurfaceCapabilities2KHR(device, &surfaceInfo, &_surface_capes)))
    {
        fprintf(ether_log_stream, "!!! Failed to get physical device surface capabilities: %s !!!\n", string_VkResult(result));
        _score = 0;
        goto EXIT;
    }
    memcpy(&ether_device->surface_capabilities, &_surface_capes.surfaceCapabilities, sizeof(VkSurfaceCapabilitiesKHR));
    fprintf(ether_log_stream, "\tSurface Capabilities:\n\t\tMinImageCount: %d\n\t\tMaxImageCount: %d\n\t\tMinImageExtent: %dx%d\n\t\tMaxImageExtend: %dx%d\n\t\tMaxImageArrayLayers: %d\n",
            ether_device->surface_capabilities.minImageCount,
            ether_device->surface_capabilities.maxImageCount,
            ether_device->surface_capabilities.minImageExtent.width,
            ether_device->surface_capabilities.minImageExtent.height,
            ether_device->surface_capabilities.maxImageExtent.width,
            ether_device->surface_capabilities.maxImageExtent.height,
            ether_device->surface_capabilities.maxImageArrayLayers);
    fprintf(ether_log_stream, "\t\tColorAttachment Flag: %s\n",
            ether_device->surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ? "Present (PASS)" : "Not Present (FAIL)");
    fprintf(ether_log_stream, "\t\tTransferDst Flag: %s\n",
            ether_device->surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT ? "Present (PASS)" : "Not Present (FAIL)");
    if (!(ether_device->surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) || !(ether_device->surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
    {
        _score = 0;
        goto EXIT;
    }
    fprintf(ether_log_stream, "\t\tAlphaOqaque Flag: %s\n",
            ether_device->surface_capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR ? "Present (PASS)" : "Not Present (FAIL)");
    if (!(ether_device->surface_capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR))
    {
        _score = 0;
        goto EXIT;
    }
```

This is a lot of logging code, honestly. The juicy bit is the last two if checks where we ensure the device can support

- `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT`
- `VK_IMAGE_USAGE_TRANSFER_DST_BIT`
- `VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR`

#### Surface Format Support

Followed by a big chunk of code to match what supported formats we can use against what the device says it can support

```c
    // and formats
    if (VK_SUCCESS != (result = vkGetPhysicalDeviceSurfaceFormats2KHR(device, &surfaceInfo, &ether_device->surface_format_count, NULL)))
    {
        fprintf(ether_log_stream, "!!! Failed to get physical device surface format count: %s !!!\n", string_VkResult(result));
        _score = 0;
        goto EXIT;
    }
    ether_device->surface_formats = malloc(sizeof(VkSurfaceFormat2KHR) * ether_device->surface_format_count);
    if (NULL == ether_device->surface_formats)
    {
        fprintf(ether_log_stream, "!!! Failed to allocate surface format list: %s\n", strerror(errno));
        _score = 0;
        goto EXIT;
    }
    // The caller initializes the header of every extensible output structure.
    for (uint32_t si = 0; si < ether_device->surface_format_count; si++)
    {
        ether_device->surface_formats[si] = (VkSurfaceFormat2KHR){
            .sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR};
    }
    if (VK_SUCCESS != (result = vkGetPhysicalDeviceSurfaceFormats2KHR(device, &surfaceInfo, &ether_device->surface_format_count, ether_device->surface_formats)))
    {
        fprintf(ether_log_stream, "!!! Failed to get physical device surface formats: %s !!!\n", string_VkResult(result));
        _score = 0;
        goto EXIT;
    }

    fprintf(ether_log_stream, "\tSurface Formats [%d]:\n", ether_device->surface_format_count);
    for (uint32_t si = 0; si < ether_device->surface_format_count; si++)
    {
        bool supported_format = false;
        for (uint32_t i = 0; i < sizeof(supported_surface_formats) / sizeof(supported_surface_formats[0]); i++)
        {
            if (memcmp(&ether_device->surface_formats[si].surfaceFormat, &supported_surface_formats[i], sizeof(VkSurfaceFormatKHR)) == 0)
            {

                // ensure it has the right properties
                VkFormatProperties src_format_properties;
                vkGetPhysicalDeviceFormatProperties(
                    device, ether_device->surface_formats[si].surfaceFormat.format, &src_format_properties);

                // Assuming blits and transfer to surface
                VkFormatFeatureFlags src_required_features =
                    VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
                    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                    VK_FORMAT_FEATURE_BLIT_DST_BIT;

                if ((src_format_properties.optimalTilingFeatures & src_required_features) == src_required_features)
                {
                    hasSupportedFormat = true;
                    supported_format = true;
                }
            }
        }
        fprintf(ether_log_stream, "\t\t[%d] (%s)\n\t\t\tFormat: %s\n\t\t\tColor Space: %s\n",
                si, supported_format ? "Supported" : "Unsupported",
                string_VkFormat(ether_device->surface_formats[si].surfaceFormat.format),
                string_VkColorSpaceKHR(ether_device->surface_formats[si].surfaceFormat.colorSpace));
    }
    if (!hasSupportedFormat)
    {
        fprintf(ether_log_stream, "\t\t!!! Missing Supported Format !!!\n");
        _score = 0;
        goto EXIT;
    }
```

##### Surface Present Modes

We're almost there! One more check. We also need to verify that the device can do certain Present modes

```c
 // then present modes
    if (VK_SUCCESS != (result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, app->vk_surface, &ether_device->surface_present_mode_count, NULL)))
    {
        fprintf(ether_log_stream, "!!! Failed to get physical device surface present mode count: %s !!!\n", string_VkResult(result));
        _score = 0;
        goto EXIT;
    }
    ether_device->surface_present_modes = malloc(sizeof(VkPresentModeKHR) * ether_device->surface_present_mode_count);
    if (NULL == ether_device->surface_present_modes)
    {
        fprintf(ether_log_stream, "!!! Failed to allocate surface present mode list: %s\n", strerror(errno));
        _score = 0;
        goto EXIT;
    }
    if (VK_SUCCESS != (result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, app->vk_surface, &ether_device->surface_present_mode_count, ether_device->surface_present_modes)))
    {
        fprintf(ether_log_stream, "!!! Failed to get physical device surface present modes: %s !!!\n", string_VkResult(result));
        _score = 0;
        goto EXIT;
    }

    fprintf(ether_log_stream, "\tPresent Modes [%d]:\n", ether_device->surface_present_mode_count);
    for (uint32_t pmi = 0; pmi < ether_device->surface_present_mode_count; pmi++)
    {
        bool supported = false;
        for (uint32_t i = 0; i < sizeof(supported_present_modes) / sizeof(supported_present_modes[0]); i++)
        {
            if (ether_device->surface_present_modes[pmi] == supported_present_modes[i])
            {
                supported = true;
                hasSupportedPresentMode = true;
            }
        }
        fprintf(ether_log_stream, "\t\t%s (%s)\n", string_VkPresentModeKHR(ether_device->surface_present_modes[pmi]), supported ? "Supported" : "Unsupported");
    }
    if (!hasSupportedPresentMode)
    {
        fprintf(ether_log_stream, "\t\tMissing Supported Present Mode\n");
        _score = 0;
        goto EXIT;
    }
```

This is essentially how the device can perform the render to the surface including options like:

- `VK_PRESENT_MODE_IMMEDIATE_KHR` (overwrite the image immediately without interlacing, slow vertical approach)
- `VK_PRESENT_MODE_MAILBOX_KHR` (maintains a queue of 1 to replace the current image with, if a new image is rendered before presented, it replaces the queued image and the previously waiting image is released back to you)
- `VK_PRESENT_MODE_FIFO_KHR` (one image per vsync in a queue, no tearing but high latency)

##### Putting the checks all together

And that's it. A lot of checks to ensure compatible device behavior but leaves a lot of opportunity to ensure support and not try to render without proper hardware and software. What does this look like? Well I have a NVIDIA 4070 Laptop GPU and here's what it is capable of (based on these checks, not everything was scanned)

```
[I] <1794882933> ./src/platform/device.c:72 | get_device_compatibility >
Physical Device:
        Id: 10336
        Name: NVIDIA GeForce RTX 4070 Laptop GPU
        Type: Discrete GPU (dedicated GPU)
        Vulkan: 1.4.357 (PASS)
        VK_FORMAT_R16G16B16A16_SFLOAT Support: Yes (PASS)
        Synchronization2 Support: Yes (PASS)
        Maintenance1 Support: Yes (PASS)
        Queue Families: 6
                [0]
                        Queue Count: 16
                        Features: [GCTP]
                [1]
                        Queue Count: 2
                        Features: [--T-]
                [2]
                        Queue Count: 8
                        Features: [-CT-]
                [3]
                        Queue Count: 1
                        Features: [--T-]
                [4]
                        Queue Count: 1
                        Features: [--T-]
                [5]
                        Queue Count: 1
                        Features: [--T-]
        Graphics Family Found: True (PASS)
        Present Family Found: True (PASS)
        Swapchain Support: Yes (PASS)
        Surface Capabilities:
                MinImageCount: 2
                MaxImageCount: 8
                MinImageExtent: 1x1
                MaxImageExtend: 16384x16384
                MaxImageArrayLayers: 1
                ColorAttachment Flag: Present (PASS)
                TransferDst Flag: Present (PASS)
                AlphaOqaque Flag: Present (PASS)
        Surface Formats [30]:
                [0] (Unsupported)
                        Format: VK_FORMAT_A2B10G10R10_UNORM_PACK32
                        Color Space: VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
                [1] (Unsupported)
                        Format: VK_FORMAT_A2B10G10R10_UNORM_PACK32
                        Color Space: VK_COLOR_SPACE_PASS_THROUGH_EXT
                [2] (Unsupported)
                        Format: VK_FORMAT_A2B10G10R10_UNORM_PACK32
                        Color Space: VK_COLOR_SPACE_BT709_NONLINEAR_EXT
                [3] (Unsupported)
                        Format: VK_FORMAT_A2B10G10R10_UNORM_PACK32
                        Color Space: VK_COLOR_SPACE_HDR10_ST2084_EXT
                [4] (Unsupported)
                        Format: VK_FORMAT_A2B10G10R10_UNORM_PACK32
                        Color Space: VK_COLOR_SPACE_HDR10_HLG_EXT
                [5] (Unsupported)
                        Format: VK_FORMAT_A2R10G10B10_UNORM_PACK32
                        Color Space: VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
                [6] (Unsupported)
                        Format: VK_FORMAT_A2R10G10B10_UNORM_PACK32
                        Color Space: VK_COLOR_SPACE_PASS_THROUGH_EXT
                [7] (Unsupported)
                        Format: VK_FORMAT_A2R10G10B10_UNORM_PACK32
                        Color Space: VK_COLOR_SPACE_BT709_NONLINEAR_EXT
                [8] (Unsupported)
                        Format: VK_FORMAT_A2R10G10B10_UNORM_PACK32
                        Color Space: VK_COLOR_SPACE_HDR10_ST2084_EXT
                [9] (Unsupported)
                        Format: VK_FORMAT_A2R10G10B10_UNORM_PACK32
                        Color Space: VK_COLOR_SPACE_HDR10_HLG_EXT
                [10] (Supported)
                        Format: VK_FORMAT_B8G8R8A8_SRGB
                        Color Space: VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
                [11] (Unsupported)
                        Format: VK_FORMAT_B8G8R8A8_SRGB
                        Color Space: VK_COLOR_SPACE_PASS_THROUGH_EXT
                [12] (Unsupported)
                        Format: VK_FORMAT_B8G8R8A8_SRGB
                        Color Space: VK_COLOR_SPACE_BT709_NONLINEAR_EXT
                [13] (Unsupported)
                        Format: VK_FORMAT_B8G8R8A8_UNORM
                        Color Space: VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
                [14] (Unsupported)
                        Format: VK_FORMAT_B8G8R8A8_UNORM
                        Color Space: VK_COLOR_SPACE_PASS_THROUGH_EXT
                [15] (Unsupported)
                        Format: VK_FORMAT_B8G8R8A8_UNORM
                        Color Space: VK_COLOR_SPACE_BT709_NONLINEAR_EXT
                [16] (Unsupported)
                        Format: VK_FORMAT_R8G8B8A8_SRGB
                        Color Space: VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
                [17] (Unsupported)
                        Format: VK_FORMAT_R8G8B8A8_SRGB
                        Color Space: VK_COLOR_SPACE_PASS_THROUGH_EXT
                [18] (Unsupported)
                        Format: VK_FORMAT_R8G8B8A8_SRGB
                        Color Space: VK_COLOR_SPACE_BT709_NONLINEAR_EXT
                [19] (Unsupported)
                        Format: VK_FORMAT_R8G8B8A8_UNORM
                        Color Space: VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
                [20] (Unsupported)
                        Format: VK_FORMAT_R8G8B8A8_UNORM
                        Color Space: VK_COLOR_SPACE_PASS_THROUGH_EXT
                [21] (Unsupported)
                        Format: VK_FORMAT_R8G8B8A8_UNORM
                        Color Space: VK_COLOR_SPACE_BT709_NONLINEAR_EXT
                [22] (Unsupported)
                        Format: VK_FORMAT_R16G16B16A16_SFLOAT
                        Color Space: VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
                [23] (Unsupported)
                        Format: VK_FORMAT_R16G16B16A16_SFLOAT
                        Color Space: VK_COLOR_SPACE_PASS_THROUGH_EXT
                [24] (Unsupported)
                        Format: VK_FORMAT_R16G16B16A16_SFLOAT
                        Color Space: VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
                [25] (Unsupported)
                        Format: VK_FORMAT_R16G16B16A16_SFLOAT
                        Color Space: VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT
                [26] (Unsupported)
                        Format: VK_FORMAT_R16G16B16A16_SFLOAT
                        Color Space: VK_COLOR_SPACE_BT709_LINEAR_EXT
                [27] (Unsupported)
                        Format: VK_FORMAT_R16G16B16A16_SFLOAT
                        Color Space: VK_COLOR_SPACE_BT709_NONLINEAR_EXT
                [28] (Unsupported)
                        Format: VK_FORMAT_R16G16B16A16_SFLOAT
                        Color Space: VK_COLOR_SPACE_BT2020_LINEAR_EXT
                [29] (Unsupported)
                        Format: VK_FORMAT_R16G16B16A16_SFLOAT
                        Color Space: VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT
        Present Modes [4]:
                VK_PRESENT_MODE_MAILBOX_KHR (Unsupported)
                VK_PRESENT_MODE_FIFO_LATEST_READY_KHR (Unsupported)
                VK_PRESENT_MODE_FIFO_KHR (Supported)
                VK_PRESENT_MODE_IMMEDIATE_KHR (Unsupported)
        Score: 900
        Compatiable: True
```

Keep in mind "Unsupported" vs "Supported" is not a GPU decision, it's the application saying what it supports compared to what the GPU supports.

#### Create the Logical Device

Now that we know what device we want, we can create the actual handle to interact with it

```c
    // create the logical device and queues
    bool separateFamilies = best_device->graphics_family != best_device->present_family;
    VkDeviceQueueCreateInfo queueInfo[2] = {{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                             .queueFamilyIndex = best_device->graphics_family,
                                             .queueCount = 1,
                                             .pQueuePriorities = queuePriorities},
                                            // add the present queue as if it is a separate family, number of items used will enable/disable it
                                            {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                             .queueFamilyIndex = best_device->present_family,
                                             .queueCount = 1,
                                             .pQueuePriorities = queuePriorities}};

  const char *deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME};
    VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR maint = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR,
        .swapchainMaintenance1 = VK_TRUE};
    VkPhysicalDeviceSynchronization2Features sync2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
        .synchronization2 = VK_TRUE,
        .pNext = &maint};
    VkDeviceCreateInfo deviceInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &sync2,
        .enabledExtensionCount = 3,
        .ppEnabledExtensionNames = deviceExtensions,
        .queueCreateInfoCount = separateFamilies ? 2 : 1,
        .pQueueCreateInfos = queueInfo};

    best_device->logical_device = VK_NULL_HANDLE;
    best_device->graphics_queue = VK_NULL_HANDLE;
    best_device->present_queue = VK_NULL_HANDLE;
    if (VK_SUCCESS != (vk_result = vkCreateDevice(best_device->physical_device, &deviceInfo, NULL, &best_device->logical_device)))
    {
        ETHER_ELOGV("Failed to create logical device: %s", string_VkResult(vk_result));
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }
```

This distinction caught me more than once: checking support and enabling something are separate steps. Several of my validation errors came from checking that an extension or feature was supported, then forgetting to enable it when creating the device. Using the wrong Vulkan constants was another recurring mistake.

The earlier checks told me what was available. This creation step is where I actually request what Ether will use. Obvious once I’d been caught by it a few times, apparently.

#### Get Queue References

Once the device is created we can get references to the queues we created

```c
    // get the queues from it (graphics will be first)
    vkGetDeviceQueue(best_device->logical_device, best_device->graphics_family, 0, &best_device->graphics_queue);
    vkGetDeviceQueue(best_device->logical_device, best_device->present_family, 0, &best_device->present_queue);
```

## Swapchain Shenanigans

I mentioned these earlier. For this milestone, the important part is that the swapchain provides the images I’ll clear and present to the window.

A note before we get into the implementation. Swapchains are tied to the surface capabilities and extents (size). So, the question has to be asked: what happens if you resize the window? Turns out you have to recreate the entire swapchain sequence as well as verify the surface that Vulkan is using is still compatible with what you need.

### Are we still compatible?

Start with a re-query of the surface capabilities

```c
    app->swapchain = VK_NULL_HANDLE;
    app->image_count = 0;
    app->images = NULL;
    app->image_views = NULL;

    // when using separate families, we provide this array for CONCURRENT access
    uint32_t concurrent_families[] = {app->device.graphics_family, app->device.present_family};

    // need to re-query the surface capabilities
    VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        .surface = app->vk_surface};

    VkSurfaceCapabilities2KHR surface_capes = {
        .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR};
    result = vkGetPhysicalDeviceSurfaceCapabilities2KHR(app->device.physical_device, &surfaceInfo, &surface_capes);
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to get physcial surface capabilities: %s", string_VkResult(result));
        rc = ETHER_ERENDER;
        goto EXIT;
    }
    memcpy(&app->device.surface_capabilities, &surface_capes.surfaceCapabilities, sizeof(VkSurfaceCapabilitiesKHR));
```

After clamping the number of images to request between the surface capabilities minimum and maximum (if it has one) number, we have to (again) ensure that the surface has the same capabilities we require as well as refresh the surface formats and, you guessed it, need to re-check if it has supported formats. I will skip over that code because it's nearly identical to how I enumerated these properties during device enumeration.

### Surface Extents

But, after that, we actually need to query the window size and compare that against what Vulkan reports as the surface's extents and pick an image size

```c
    if (!SDL_GetWindowSizeInPixels(app->sdl_window, &sdl_width, &sdl_height))
    {
        ETHER_ELOGV("Failed to query SDL window size: %s", SDL_GetError());
        rc = ETHER_ESDL_INIT;
        goto EXIT;
    }
    if (app->device.surface_capabilities.currentExtent.width != UINT32_MAX)
    {
        size_extents = app->device.surface_capabilities.currentExtent;
    }
    else
    {
        size_extents.width = SDL_clamp((uint32_t)sdl_width,
                                       app->device.surface_capabilities.minImageExtent.width,
                                       app->device.surface_capabilities.maxImageExtent.width);
        size_extents.height = SDL_clamp((uint32_t)sdl_height,
                                        app->device.surface_capabilities.minImageExtent.height,
                                        app->device.surface_capabilities.maxImageExtent.height);
    }

    ETHER_ILOGV("Extents: %dx%d", size_extents.width, size_extents.height);
```

### Swapchain Creation

Then we can create the actual swapchain object

```c
    bool separate_families = app->device.graphics_family != app->device.present_family;
    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = app->vk_surface,
        .minImageCount = image_count,
        .imageFormat = surface_format->format,
        .imageColorSpace = surface_format->colorSpace,
        .imageExtent = size_extents,
        .imageArrayLayers = 1,

        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT,

        .imageSharingMode = separate_families ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = separate_families ? 2 : 0,
        .pQueueFamilyIndices = separate_families ? concurrent_families : NULL,

        .preTransform = app->device.surface_capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, // required in device selection
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,             // required in device selection
        .clipped = VK_TRUE,

        .oldSwapchain = VK_NULL_HANDLE};

    if (VK_SUCCESS != (result = vkCreateSwapchainKHR(app->device.logical_device, &createInfo, NULL, &swapchain)))
    {
        ETHER_ELOGV("Failed to create swapchain: %s", string_VkResult(result));
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }
    app->swapchain = swapchain;
    ETHER_ILOG("Swapchain created");
```

Before we move on, it is worth pointing out the CONCURRENT usage for `.imageSharingMode` if we have separate queue families. This ensures that both families can access the swapchain images without extra synchronization. A small array of what families should have concurrent access was defined earlier.

### Image References

Then we snag the image references

```c
    // get images and imageviews
    do
    {
        if (NULL != _images)
        {
            free(_images);
            _images = NULL;
        }
        created_image_count = 0;
        if (VK_SUCCESS != (result = vkGetSwapchainImagesKHR(app->device.logical_device, swapchain, &created_image_count, NULL)))
        {
            ETHER_ELOGV("Failed to get swapchain image count: %s", string_VkResult(result));
            rc = ETHER_EVK_INIT;
            goto EXIT;
        }
        ETHER_ILOGV("Swapchain has %d images", created_image_count);

        // allocate the images and image views
        _images = calloc(created_image_count, sizeof(VkImage));
        if (NULL == _images)
        {
            ETHER_ELOGV("Failed to allocate image list: %s", strerror(errno));
            rc = errno;
            goto EXIT;
        }

        if (VK_SUCCESS != (result = vkGetSwapchainImagesKHR(app->device.logical_device, swapchain, &created_image_count, _images)))
        {
            if (VK_INCOMPLETE == result)
                continue; // count changed
            ETHER_ELOGV("Failed to get swapchain images: %s", string_VkResult(result));
            rc = ETHER_EVK_INIT;
            goto EXIT;
        }
    } while (VK_INCOMPLETE == result);
    ETHER_ILOG("Image references retrieved");
```

You see we check for `VK_INCOMPLETE` here. This is because the count between the two API calls could have changed and thus Vulkan notifies us that we didn't have enough room for the new count (meaning it grew in that time).

### ImageViews

After that, we need to create `ImageViews`. This is how Vulkan knows how to handle an image, it is the viewport or manner in which the image is treated.

```c
    // now get image views for each one
    ETHER_ILOGV("Creating %d ImageViews", created_image_count);
    _image_views = calloc(created_image_count, sizeof(VkImageView));
    if (NULL == _image_views)
    {
        ETHER_ELOGV("Failed to get image views for swapchain: %s", strerror(errno));
        rc = errno;
        goto EXIT;
    }

    for (uint32_t i = 0; i < created_image_count; i++)
    {
        viewInfo.image = _images[i];
        viewInfo.format = surface_format->format;

        if (VK_SUCCESS != (result = vkCreateImageView(app->device.logical_device, &viewInfo, NULL, _image_views + i)))
        {
            ETHER_ILOGV("Failed to create image view, index %d: %s", i, string_VkResult(result));
            rc = ETHER_EVK_INIT;
            goto EXIT;
        }
    }
    ETHER_ILOG("Image views created");
```

There’s one more piece to create here: a render-finished semaphore for each swapchain image. I keep these alongside the swapchain because the number of semaphores depends on the number of images.

Here’s that allocation:

```c
    // allocate the semaphores pointers for each image here
    ETHER_ILOG("Allocating renders finished semaphore list");

    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    app->renders_finished = calloc(created_image_count, sizeof(VkSemaphore));
    if (NULL == app->renders_finished)
    {
        ETHER_ELOGV("Failed to allocate renders_finished list: %s", strerror(errno));
        rc = errno;
        goto EXIT;
    }
    // allocate the renders_finished for every image in swapchain
    for (uint32_t i = 0; i < created_image_count; i++)
    {
        result = vkCreateSemaphore(app->device.logical_device, &semaphoreInfo, NULL, app->renders_finished + i);
        if (VK_SUCCESS != result)
        {
            ETHER_ELOGV("Failed to create semaphore for image %d: %s", i, string_VkResult(result));
            rc = ETHER_EVK_INIT;
            goto EXIT;
        }
    }
```

That’s the swapchain set up. Now for the objects that let me send work to the GPU.

## Rendering Setup

More initialization. Of course there is.

This is where we start really discussing graphics related primitives that control rendering and the overall engine behavior.

### Command Pool

The command pool setup was pleasantly short:

```c
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = app->device.graphics_family};

    ETHER_ILOG("Creating graphics command pool");
    result = vkCreateCommandPool(
        app->device.logical_device,
        &poolInfo,
        NULL,
        &app->command_pool);
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to create command pool: %s", string_VkResult(result));
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }
```

Just like that we have a `CommandPool` to allocate `CommandBuffers` in that allow us to record rendering commands to send to the GPU.

Pay attention though: this `CommandPool` can only be used with one queue family! If we need to use multiple families for these operations, we'd have to make multiple of them. Eventually, I will need to do this but currently only the graphics family will be used for rendering operations that I'd need a CommandPool for. Present operations don't record GPU actions and thus don't need a `CommandPool`.

### Command Buffer(s)

First, allocate a command buffer from the pool:

```c
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = app->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1};

    ETHER_ILOG("Creating command buffer");
    result = vkAllocateCommandBuffers(
        app->device.logical_device,
        &allocInfo,
        &app->command_buffer);
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to allocate command buffers: %s", string_VkResult(result));
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }
```

This is the object I’ll record GPU commands into. It belongs to the same queue family as its command pool. I’m starting with one command buffer and one frame in flight; this milestone only needs to clear the window to a color.

### Image Available Semaphore

Synchronization was the part where I had to stop and do more reading. At first, I couldn’t imagine already having enough context to know which objects to create, let alone how to put them together. Seeing the declarations wasn’t enough; I needed to understand what each one was waiting for.

My initial expectation had been much closer to “tell it to render, then show the result.” Here, I have to account for work that may still be happening after an API call returns. That’s the shift I wasn’t prepared for.

I think I’m at a decent starting point now. For this implementation, I’m keeping one frame in flight and working through the synchronization objects one at a time.

```c
    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    ETHER_ILOG("Creating image available semaphore");
    result = vkCreateSemaphore(app->device.logical_device, &semaphoreInfo, NULL, &app->image_available);
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to create image available semaphore: %s", string_VkResult(result));
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }
```

This allocates the "image available" semaphore. This is waited on when we need to wait for an image from the swapchain to become available.

### Render Finished Semaphore

I initialized these in the swapchain creation step since it is tied to the number of images in the swapchain itself. While the image available semaphore indicates if an image, any image from the swapchain, is ready for use in rendering, this list of semaphores is used to indicate when the image at a given index is done rendering. We wait for this before transitioning the image into presentation. We'll see how this works in the Rendering section.

### In Flight Fence

A what, now? Fence, you say? I've mentioned them a couple times but I think a "gate" would be a better name for them. The gate can either be open (signaled) or closed (unsignaled). If you walk up to the gate and it's open, you just proceed as if it wasn't there at all, and the gate stays open. If you walk up and the gate is closed, you, of course, can't go through, so you must wait. You also have the option to close the gate (reset the fence).

I use one fence to indicate a frame is "in flight", meaning it's on its way to the GPU and being rendered.

```c
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    ETHER_ILOG("Creating in flight fence");
    result = vkCreateFence(app->device.logical_device, &fenceInfo, NULL, &app->in_flight);
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to create in flight fence: %s", string_VkResult(result));
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }
```

### Present Fence

I use one more Fence to indicate when we are done presenting an image. This is an extension by nature since by default there isn't a way to know when the images are done being presented. There are ways to know when it is done rendering as we just discussed but that doesn't mean the GPU is not busy. Thus, if we were to try to cleanup or destroy objects, Vulkan will be very upset. So we register one more fence to indicate when presenting is done and as a result we can wait on both the in flight fence and present fence to know that nothing is queued up and processing on the GPU or being presented.

```c
    ETHER_ILOG("Creating present fence");
    fenceInfo.flags = 0; // do not set it to be signaled right away
    result = vkCreateFence(app->device.logical_device, &fenceInfo, NULL, &app->present_fence);
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to create present fence: %s", string_VkResult(result));
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }
```

## Rendering (FINALLY)

We made it! All that setup, and I’m finally ready to clear an image to a single color. Spheres will have to wait.

Okay we have a bunch of primitives to synchronize our rendering, what does the actual logic to do this synchronization look like?

### Wait for the In Flight Fence

We talked about this earlier. This indicates that the image we queued to the GPU for rendering is no longer being rendered to. Technically, this isn't a required step since we could enforce that we have, say, 3 images in our swapchain. One could be rendering, one presenting, and one would be ready to get set up to render. As I flesh this renderer out, the in flight fence will evolve to encompass more efficient queueing of images for rendering.

```c
    // rebuild swapchain if need be
    if (app->pending_swapchain_rebuild)
    {

        rc = ether_generate_swapchain(app);
        if (ETHER_SUCCESS != rc)
        {
            goto EXIT;
        }

        app->pending_swapchain_rebuild = false;
    }

    result = vkWaitForFences(app->device.logical_device, 1, &app->in_flight, VK_TRUE, UINT64_MAX);
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to wait for in flight fence: %s", string_VkResult(result));
        rc = ETHER_ERENDER;
        goto EXIT;
    }
```

Notice I included a snippet about regenerating the swapchain. This is important to do at the very beginning of the render function so that we sync our swapchain properties to the surface, if it was changed.

The actual wait for fence call is quite straightforward.

### Get An Image

This is better discussed after seeing the code

```c
    result = vkAcquireNextImageKHR(app->device.logical_device,
                                   app->swapchain,
                                   UINT64_MAX,
                                   app->image_available,
                                   VK_NULL_HANDLE,
                                   &image_index);
    if (VK_SUCCESS != result)
    {
        if (VK_ERROR_OUT_OF_DATE_KHR == result)
        {
            app->pending_swapchain_rebuild = true;
            rc = ETHER_SUCCESS;
            goto EXIT;
        }
        else if (VK_SUBOPTIMAL_KHR != result)
        {
            rc = ETHER_ERENDER;
            goto EXIT;
        }
        else
        {
            app->pending_swapchain_rebuild = true;
        }
    }
```

It's a quite simple call but the return codes require some discussion. `VK_ERROR_OUT_OF_DATE_KHR` is an error code returned by some rendering API calls that indicate that the image properties being used are out of sync with the surface or GPU state. Thus, just trigger a rebuild. If the capabilities are a mismatch, the rebuild will return an error and we'll have to exit.

`VK_SUBOPTIMAL_KHR` is similar in that we need to rebuild the swapchain but different in that an image was actually acquired here and _must_, and I mean _**must**_ be queued into the graphics pipeline or the state of the semaphores and fences we created will get put into deadlock since we never submitted the image. So we check if it wasn't that and is a proper error and return out, otherwise trigger the rebuild and continue with the frame.

### Reset the Command Buffer

Super simple. Before recording commands, reset the command buffer.

```c
    result = vkResetCommandBuffer(app->command_buffer, 0);
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to reset command buffer: %s", string_VkResult(result));
        rc = ETHER_ERENDER;
        goto EXIT;
    }
```

### Recording GPU Commands into a CommandBuffer

This is where the drawing commands will eventually live. For now, it’s two layout transitions with a color clear between them.

```c
    // record commands
    rc = ether_render_record(app, image_index);
    if (ETHER_SUCCESS != rc)
    {
        goto EXIT;
    }
```

Yeah, I made it its own function. Let's take a look.

#### Start Recording

Pretty self explanatory here.

```c
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    result = vkBeginCommandBuffer(app->command_buffer, &beginInfo);
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to begin command buffer: %s", string_VkResult(result));
        rc = ETHER_ERENDER;
        goto EXIT;
    }
```

ONE_TIME_SUBMIT means we are not going to resubmit the command buffer commands and will guarantee that we reset it each time.

#### Get the image into the TRANSFER layout

We need to take the image from wherever it is, called a layout, and put it in the TRANSFER_DST layout so we can execute a clear color command on it.

```c
    result = vkBeginCommandBuffer(app->command_buffer, &beginInfo);
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to begin command buffer: %s", string_VkResult(result));
        rc = ETHER_ERENDER;
        goto EXIT;
    }

    // transition to transfer
    VkImageMemoryBarrier2 toTransfer = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,

        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,

        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

        .image = app->images[image_index],

        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1}};

    VkDependencyInfo dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &toTransfer};

    vkCmdPipelineBarrier2(app->command_buffer, &dependency);
```

A lot of constants here. We are saying it can come from anywhere in the pipeline, and don't care about what the access type was when it was there. We are taking it to the TRANSFER stage and intend to WRITE to it. We also don't care what LAYOUT it was in before and need it in TRANSFER_DST (the destination for color operations). Oh and don't worry about the queue families, we don't need to consider those here either. Then we say what image we are referring to as well as what mip level and array index of the image (in case the image is an array of images, 3D, etc. Ours isn't).

#### Clear its color

It's finally time to do some rendering!

```c
    // clear the color
    VkClearColorValue clearColor = {
        .float32 = {0.02f, 0.05f, 0.12f, 1.0f}};

    VkImageSubresourceRange range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1};

    vkCmdClearColorImage(
        app->command_buffer,
        app->images[image_index],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &clearColor,
        1,
        &range);
```

Yep, that’s it. Hey, we’ve got to start somewhere, right?

#### Transition the Layout to Present

Same as getting the image to the transfer layout but now move it to the PRESENT layout

```c
    // transition to present
    VkImageMemoryBarrier2 toPresent = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,

        .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
        .dstAccessMask = VK_ACCESS_2_NONE,

        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,

        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

        .image = app->images[image_index],
        .subresourceRange = range};

    VkDependencyInfo presentDependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &toPresent};

    vkCmdPipelineBarrier2(app->command_buffer, &presentDependency);
```

Notice we don't care about the destination stage mask or access type.

#### Finish Recording

Aw, we are done, already?

```c
    result = vkEndCommandBuffer(app->command_buffer);
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to end command buffer: %s", string_VkResult(result));
        rc = ETHER_ERENDER;
        goto EXIT;
    }
```

### Submit the CommandBuffer for work

Now, we need to tell Vulkan about all the synchronization primitives we created so it knows what to flag and when. I am going to show the entire block up to submitting the image.

```c
    // wait for queue to process to present
    VkSemaphoreSubmitInfo waitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = app->image_available,
        .stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT};

    VkCommandBufferSubmitInfo commandInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = app->command_buffer};

    VkSemaphoreSubmitInfo signalInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = app->renders_finished[image_index],
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};

    VkSubmitInfo2 submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,

        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &waitInfo,

        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandInfo,

        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo};

    if (app->present_pending)
    {
        // if we are rendering normally and present_pending was triggered, wait on it and reset it
        // so it can be used below
        result = vkWaitForFences(app->device.logical_device, 1, &app->present_fence, VK_TRUE, UINT64_MAX);
        if (VK_SUCCESS != result)
        {
            ETHER_ELOGV("Failed to wait for present fence: %s", string_VkResult(result));
            rc = ETHER_ERENDER;
            goto EXIT;
        }
        result = vkResetFences(app->device.logical_device, 1, &app->present_fence);
        if (VK_SUCCESS != result)
        {
            ETHER_ELOGV("Failed to reset present fence: %s", string_VkResult(result));
            rc = ETHER_ERENDER;
            goto EXIT;
        }
        app->present_pending = false;
    }

    // reset right before (no other error paths should reset this)
    result = vkResetFences(app->device.logical_device, 1, &app->in_flight);
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to reset fense: %s", string_VkResult(result));
        rc = ETHER_ERENDER;
        goto EXIT;
    }

    // submit for processing
    result = vkQueueSubmit2(
        app->device.graphics_queue,
        1,
        &submitInfo,
        app->in_flight);
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to submit GPU work: %s", string_VkResult(result));
        rc = ETHER_ERENDER;
        goto EXIT;
    }
```

About half of that is describing the submission to Vulkan. The rest is error checking and waiting for and resetting the fences before reusing them.

At this point, the in flight fence is cleared and won't be set until rendering is finished. Notice it also waits for the image available semaphore. This will be pivotal to future implementations when we have multiple frames in flight instead of just one. This ensures no rendering is queued to the GPU until an image is available.

### Submit the image for Present

We can also submit the image for Present, even before it has finished rendering due to the list of semaphores we created.

```c
    VkSwapchainPresentFenceInfoKHR presentFenceInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR,
        .swapchainCount = 1,
        .pFences = &app->present_fence,
    };

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,

        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &app->renders_finished[image_index],

        .swapchainCount = 1,
        .pSwapchains = &app->swapchain,
        .pImageIndices = &image_index,

        .pNext = &presentFenceInfo};

    // schedule for presenting when the render is finished
    result = vkQueuePresentKHR(
        app->device.present_queue,
        &presentInfo);
    if (VK_SUCCESS != result)
    {
        if (VK_ERROR_OUT_OF_DATE_KHR == result || VK_SUBOPTIMAL_KHR == result)
        {
            app->pending_swapchain_rebuild = true;
            app->present_pending = true;
            rc = ETHER_SUCCESS;
            goto EXIT;
        }
        ETHER_ELOGV("Failed to queue present image: %s", string_VkResult(result));
        rc = ETHER_ERENDER;
        goto EXIT;
    }
    app->present_pending = true;
```

We tell it about our present fence (which is guaranteed to be reset by this point), and tell it to wait on the rendering semaphore for the image. This way, it won't be queued for present until it is done rendering and will trigger our present fence if the queue to present succeeded.

We have to check those error codes again because they show that it was submitted for present. This way we know to wait on it if need be (either next frame, recreating the swapchain, or shutting down).

That's the end of the rendering steps.

## Engine Loop

Let's switch gears for a minute and talk about what the core engine loop looks like. This is after all resources have been initialized (Vulkan itself, surface, device, swapchain, and render primitives).

```c
ether_rc ether_engine_loop(ether_app_t *app)
{
    ether_rc rc = ETHER_SUCCESS;
    PARAM_CHECK(NULL != app, rc = ETHER_EPARAM, goto EXIT);

    ETHER_ILOG("Entering main engine loop");

    while (!app->should_quit)
    {
        // poll for events
        while (SDL_PollEvent(&app->event))
        {
            switch (app->event.type)
            {
            case SDL_EVENT_QUIT:
                ETHER_ILOG("SDL Quit Event received");
                app->should_quit = true;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                ETHER_ILOG("SDL Resize Event");
                app->pending_swapchain_rebuild = true;
                break;
            default:
                // nothing to do
                break;
            }
        }

        if (app->should_quit)
            break;

        // update scene state

        // render
        rc = ether_render(app);
        if (ETHER_SUCCESS != rc)
        {
            app->should_quit = true;
            break;
        }
    }

    ETHER_ILOG("Main engine loop exited");

EXIT:
    return rc;
}
```

There isn’t much engine logic here yet: poll events, handle a resize or quit request, and render. This gives me somewhere to add scene updates as the tracer grows.

## Cleanup

Cleanup of each stage of resources needs to happen in reverse order. The order is easier to see in the top-level destruction function. Also, note I make it so these functions can be called at any time and won't error if certain objects haven't been created yet, guaranteeing an empty state after doing so.

This is another reason I want validation enabled while developing. There are a lot of object lifetimes and bits of state to keep track of here.

```c
ether_rc ether_vulkan_destroy(ether_app_t *app)
{
    ether_rc rc = ETHER_SUCCESS;

    PARAM_CHECK(NULL != app, rc = ETHER_EPARAM, goto EXIT);

    ether_render_destroy(app);

    ether_swapchain_destroy(app);

    ether_device_destroy(app);

    if (NULL != app->vk_surface)
    {
        ETHER_ILOG("Destroying Vulkan surface");
        vkDestroySurfaceKHR(app->vk_instance, app->vk_surface, NULL);
        app->vk_surface = NULL;
    }
    if (NULL != app->vk_instance)
    {
        ETHER_ILOG("Destroying Vulkan instance");
        vkDestroyInstance(app->vk_instance, NULL);
        app->vk_instance = NULL;
    }

EXIT:
    return rc;
}
```

### Render Primitives

```c
void ether_render_destroy(ether_app_t *app)
{
    PARAM_CHECK(NULL != app, ETHER_ELOG("app null"), return);
    VkResult result = VK_SUCCESS;

    // wait on the in flight for rendering
    if (VK_NULL_HANDLE != app->in_flight)
    {
        ETHER_ILOG("Waiting for in flight fence");
        result = vkWaitForFences(app->device.logical_device, 1, &app->in_flight, VK_TRUE, UINT64_MAX);
        if (VK_SUCCESS != result)
        {
            ETHER_ELOGV("Failed to wait for in flight fence: %s", string_VkResult(result));
        }
        // don't bother resetting it, it's about to be destroyed, also don't quit on error
    }

    // and wait for present to be done
    if (app->present_pending)
    {
        // if we are rendering normally and present_pending was triggered, wait on it and reset it
        // so it can be used below
        ETHER_ILOG("Waiting for present fence");
        result = vkWaitForFences(app->device.logical_device, 1, &app->present_fence, VK_TRUE, UINT64_MAX);
        if (VK_SUCCESS != result)
        {
            ETHER_ELOGV("Failed to wait for present fence: %s", string_VkResult(result));
        }
        /// don't bother resetting it, it's about to be destroyed, also don't quit on error
    }

    // additional safeguard for ensuring the device is idle
    if (VK_NULL_HANDLE != app->device.logical_device)
    {
        ETHER_ILOG("Waiting for idle device");
        if (VK_SUCCESS != (result = vkDeviceWaitIdle(app->device.logical_device)))
        {
            ETHER_ELOGV("Failed to wait for idle device: %s", string_VkResult(result));
            // keep going regardless....
        }
    }

    if (VK_NULL_HANDLE != app->present_fence)
    {
        ETHER_ILOG("Destroying present fence");
        vkDestroyFence(app->device.logical_device, app->present_fence, NULL);
        app->present_fence = VK_NULL_HANDLE;
    }

    if (VK_NULL_HANDLE != app->in_flight)
    {
        ETHER_ILOG("Destroying in flight fence");
        vkDestroyFence(app->device.logical_device, app->in_flight, NULL);
        app->in_flight = VK_NULL_HANDLE;
    }
    if (VK_NULL_HANDLE != app->image_available)
    {
        ETHER_ILOG("Destroying image available semaphore");
        vkDestroySemaphore(app->device.logical_device, app->image_available, NULL);
        app->image_available = VK_NULL_HANDLE;
    }
    if (NULL != app->renders_finished)
    {
        ETHER_ILOG("Destroying renders finished semaphore list");
        for (uint32_t i = 0; i < app->image_count; i++)
        {
            if (VK_NULL_HANDLE != app->renders_finished[i])
            {
                vkDestroySemaphore(app->device.logical_device, app->renders_finished[i], NULL);
                app->renders_finished[i] = VK_NULL_HANDLE;
            }
        }
        free(app->renders_finished);
        app->renders_finished = NULL;
    }
    if (VK_NULL_HANDLE != app->command_pool)
    {
        ETHER_ILOG("Destroying command buffer");
        vkFreeCommandBuffers(app->device.logical_device, app->command_pool, 1, &app->command_buffer);
        app->command_buffer = VK_NULL_HANDLE;
        ETHER_ILOG("Destroying command pool");
        vkDestroyCommandPool(app->device.logical_device, app->command_pool, NULL);
        app->command_pool = VK_NULL_HANDLE;
    }
}
```

See how we wait for the fences here. This guarantees that there aren't images being rendered or presented when we try to destroy them.

### Swapchain

```c

void ether_swapchain_destroy(ether_app_t *app)
{
    PARAM_CHECK(NULL != app, ETHER_ELOG("app null"), return);

    if (NULL != app->renders_finished)
    {
        ETHER_ILOG("Destroying render finished semaphore list");
        for (uint32_t i = 0; i < app->image_count; i++)
        {
            vkDestroySemaphore(app->device.logical_device, app->renders_finished[i], NULL);
        }
        free(app->renders_finished);
        app->renders_finished = NULL;
    }

    if (NULL != app->image_views)
    {
        ETHER_ILOG("Destroying image views");
        for (uint32_t i = 0; i < app->image_count; i++)
        {
            vkDestroyImageView(app->device.logical_device, app->image_views[i], NULL);
        }
        free(app->image_views);
        app->image_views = NULL;
    }

    if (NULL != app->images)
    {
        ETHER_ILOG("Freeing image list");
        free(app->images);
        app->images = NULL;
    }

    if (VK_NULL_HANDLE != app->swapchain)
    {
        ETHER_ILOG("Destroying swapchain");
        vkDestroySwapchainKHR(app->device.logical_device, app->swapchain, NULL);
        app->swapchain = VK_NULL_HANDLE;
    }
}
```

### Graphics Device

```c
void ether_device_destroy(ether_app_t *app)
{
    PARAM_CHECK(NULL != app, ETHER_ELOG("app null"), return);

    // remove the surface_formats and surface_present_modes
    if (NULL != app->device.surface_formats)
    {
        free(app->device.surface_formats);
        app->device.surface_formats = NULL;
    }
    if (NULL != app->device.surface_present_modes)
    {
        free(app->device.surface_present_modes);
        app->device.surface_present_modes = NULL;
    }

    if (VK_NULL_HANDLE != app->device.logical_device)
    {
        ETHER_ILOG("Destroying logical device");
        vkDestroyDevice(app->device.logical_device, NULL);
    }
}
```

### SDL Window

```c
void ether_window_destroy(ether_app_t *app)
{
    PARAM_CHECK(NULL != app, ETHER_ELOG("app null"), return);

    if (NULL != app->sdl_window)
    {
        ETHER_ILOG("Destroying SDL Window");
        SDL_DestroyWindow(app->sdl_window);
        app->sdl_window = NULL;
    }

    SDL_Quit();
}
```

`SDL_Quit` is the last step. We’re finally on our way out.

## Final Result

Hold on to your seat! Here is what those 2.2k lines of code generated for me:

![Getting a Window](/ether/screenshots/get-a-window-result.png)

A single color. After all that, I’ll take it.

The biggest change for me was in how I thought about the work. I came in expecting to set up a context and start drawing. Instead, I spent this milestone describing what the application needs, checking what the device supports, and figuring out how to coordinate work that doesn’t finish just because a function returned.

Next up is the compute pipeline. There’s still no scene to manage and nothing to trace, but at least I have somewhere to put the result now.
