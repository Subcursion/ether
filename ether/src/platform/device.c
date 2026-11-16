#include "ether/platform/device.h"

#include "ether/rc.h"
#include "ether/logging.h"
#include "ether/rendering/vulkan_state.h"
#include "ether/rendering/supported_options.h"

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static const char *get_device_type_str(VkPhysicalDeviceType type)
{
    switch (type)
    {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "Discrete GPU (dedicated GPU)";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "Integrated GPU (build into CPU)";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return "Virtual GPU (virtualized environment)";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return "CPU (software rendering)";
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
    case VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM:
    default:
        return "Other (Unknown)";
    }
}

static void get_device_compatability(ether_app_t *app, VkPhysicalDevice device, ether_device_t *ether_device)
{
    PARAM_CHECK(NULL != app, ETHER_ELOG("app null"), return);
    PARAM_CHECK(VK_NULL_HANDLE != device, ETHER_ELOG("device null"), return);
    PARAM_CHECK(NULL != ether_device, ETHER_ELOG("ether_device null"), return);
    VkResult result = VK_SUCCESS;
    VkPhysicalDeviceProperties properties = {0};
    uint32_t _score = 0;
    bool good_version = true;

    uint32_t familyCount = 0;
    VkQueueFamilyProperties *families = NULL;
    VkQueueFamilyProperties *family = NULL;

    uint32_t extension_count = 0;
    VkExtensionProperties *extensions = NULL;

    memset(&ether_device->surface_capabilities, 0, sizeof(VkSurfaceCapabilitiesKHR));
    ether_device->surface_format_count = 0;
    ether_device->surface_formats = NULL;
    ether_device->surface_present_mode_count = 0;
    ether_device->surface_present_modes = NULL;

    // canaries for invalid families
    ether_device->graphics_family = UINT32_MAX;
    ether_device->present_family = UINT32_MAX;

    bool hasSupportedFormat = false;
    bool hasSupportedPresentMode = false;

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

        fprintf(ether_log_stream, "\t\t\tFeatures: [%c%c%c%c]\n",
                supportsGraphics,
                supportsCompute,
                supportsTransfer,
                supportsPresent ? 'P' : '-');
    }
    fprintf(ether_log_stream, "\tGraphics Family Found: %s\n",
            ether_device->graphics_family != UINT32_MAX ? "True (PASS)" : "False (FAIL)");
    fprintf(ether_log_stream, "\tPresent Family Found: %s\n",
            ether_device->present_family != UINT32_MAX ? "True (PASS)" : "False (FAIL)");
    if (ether_device->graphics_family == UINT32_MAX)
    {
        _score = 0;
        goto EXIT;
    }
    if (ether_device->present_family == UINT32_MAX)
    {
        _score = 0;
        goto EXIT;
    }

    // now check for extensions
    if (VK_SUCCESS != (result = vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL)))
    {
        fprintf(ether_log_stream, "!!! Failed to get extension count: %s !!!\n", string_VkResult(result));
        _score = 0; // disable the GPU
        goto EXIT;
    }

    extensions = malloc(sizeof(VkExtensionProperties) * extension_count);
    if (NULL == extensions)
    {
        fprintf(ether_log_stream, "!!! Failed to allocate extension property list: %s !!!", strerror(errno));
        _score = 0; // disable the GPU
        goto EXIT;
    }

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

    fprintf(ether_log_stream, "\tScore: %d\n\tCompatiable: %s", _score, _score > 0 ? "True" : "False");

EXIT:
    ETHER_EXIT_LOG();

    if (_score == 0)
    {
        // cleanup unused pointers since it won't be chosen
        if (NULL != ether_device->surface_formats)
        {
            free(ether_device->surface_formats);
            ether_device->surface_formats = NULL;
        }
        if (NULL != ether_device->surface_present_modes)
        {
            free(ether_device->surface_present_modes);
            ether_device->surface_present_modes = NULL;
        }
    }
    if (NULL != extensions)
    {
        free(extensions);
    }
    if (NULL != families)
    {
        free(families);
    }
    // populate ether_device
    ether_device->physical_device = device;
    memcpy(&ether_device->properties, &properties, sizeof(properties));
    ether_device->score = _score;
    return;
}

ether_rc ether_pick_device(ether_app_t *app, ether_device_t *device)
{
    ether_rc rc = ETHER_SUCCESS;
    PARAM_CHECK(NULL != app, rc = ETHER_EPARAM, goto EXIT);
    PARAM_CHECK(NULL != app->vk_instance, rc = ETHER_EPARAM, goto EXIT);
    PARAM_CHECK(NULL != device, rc = ETHER_EPARAM, goto EXIT);

    VkResult vk_result;
    uint32_t deviceCount = 0;
    VkPhysicalDevice *devices = NULL;
    ether_device_t *ether_devices = NULL;

    uint32_t highest_score = 0;
    ether_device_t *best_device = NULL;

    float queuePriorities[2] = {1.0f, 1.0f}; // 1.0 max allowed

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

    uint32_t di = 0;
    for (; di < deviceCount; di++)
    {
        get_device_compatability(app, devices[di], ether_devices + di);
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

    // create the logicl device and queues
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
    // get the queues from it (graphics will be first)
    vkGetDeviceQueue(best_device->logical_device, best_device->graphics_family, 0, &best_device->graphics_queue);
    vkGetDeviceQueue(best_device->logical_device, best_device->present_family, 0, &best_device->present_queue);

    ETHER_ELOG("Device created");

    memcpy(device, best_device, sizeof(ether_device_t));
EXIT:
    if (NULL != devices)
    {
        free(devices);
    }
    if (NULL != ether_devices)
    {
        free(ether_devices);
    }
    return rc;
}

void ether_device_destroy(ether_app_t *app)
{
    PARAM_CHECK(NULL != app, ETHER_ELOG("app null"), goto EXIT);

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
EXIT:
    return;
}
