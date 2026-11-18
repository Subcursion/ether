#include "ether/platform/device.h"

#include "ether/rc.h"
#include "ether/logging.h"
#include "ether/rendering/vulkan_state.h"
#include "ether/rendering/supported_options.h"
#include "ether/profiler.h"

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

static void get_device_compatibility(ether_app_t *app, VkPhysicalDevice device, ether_device_t *ether_device)
{
    VkResult result = VK_SUCCESS;
    VkPhysicalDeviceProperties properties = {0};
    uint32_t _score = 0;
    bool good_version = true;

    uint32_t familyCount = 0;
    VkQueueFamilyProperties *families = NULL;
    VkQueueFamilyProperties *family = NULL;

    uint32_t extension_count = 0;
    VkExtensionProperties *extensions = NULL;

    PARAM_CHECK(NULL != app, ETHER_ELOG("app null"), return);
    PARAM_CHECK(VK_NULL_HANDLE != device, ETHER_ELOG("device null"), return);
    PARAM_CHECK(NULL != ether_device, ETHER_ELOG("ether_device null"), return);

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
        (VK_API_VERSION_MAJOR(properties.apiVersion) == VK_API_VERSION_MAJOR(TARGET_VULKAN_VERSION) &&
         VK_API_VERSION_MINOR(properties.apiVersion) < VK_API_VERSION_MINOR(TARGET_VULKAN_VERSION)))
    {
        good_version = false;
    }

    ETHER_LOG_RAWV(ETHER_LOG_INFO, "\nPhysical Device:\n\tId: %d\n\tName: %s\n\tType: %s\n\tVulkan: %d.%d.%d (%s)\n",
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

    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(
        device, ray_image_format, &format_properties);

    // Assuming compute writes followed by a blit to the swapchain.
    VkFormatFeatureFlags ray_image_feature_req =
        VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
        VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
        VK_FORMAT_FEATURE_BLIT_SRC_BIT;

    ETHER_LOG_RAWV(ETHER_LOG_INFO,
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

    ETHER_LOG_RAWV(ETHER_LOG_INFO, "\tSynchronization2 Support: %s\n",
                   sync2.synchronization2
                       ? "Yes (PASS)"
                       : "No (FAIL)");
    if (!sync2.synchronization2)
    {
        _score = 0;
        goto EXIT;
    }
    ETHER_LOG_RAWV(ETHER_LOG_INFO, "\tMaintenance1 Support: %s\n",
                   maintenance.swapchainMaintenance1 ? "Yes (PASS)" : "No (FAIL)");
    if (!maintenance.swapchainMaintenance1)
    {
        _score = 0;
        goto EXIT;
    }

    // check queue families
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, NULL);
    ETHER_LOG_RAWV(ETHER_LOG_INFO, "\tQueue Families: %d\n", familyCount);

    families = malloc(sizeof(VkQueueFamilyProperties) * familyCount);
    if (NULL == families)
    {
        ETHER_LOG_RAWV(ETHER_LOG_INFO, "!!! Failed to allocate family properites: %s !!!", strerror(errno));
        _score = 0;
        goto EXIT;
    }

    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families);

    for (uint32_t fi = 0; fi < familyCount; fi++)
    {
        family = &families[fi];

        ETHER_LOG_RAWV(ETHER_LOG_INFO, "\t\t[%d]\n\t\t\tQueue Count: %d\n", fi, family->queueCount);

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

        ETHER_LOG_RAWV(ETHER_LOG_INFO, "\t\t\tFeatures: [%c%c%c%c]\n",
                       supportsGraphics,
                       supportsCompute,
                       supportsTransfer,
                       supportsPresent ? 'P' : '-');
    }
    ETHER_LOG_RAWV(ETHER_LOG_INFO, "\tGraphics Family Found: %s\n",
                   ether_device->graphics_family != UINT32_MAX ? "True (PASS)" : "False (FAIL)");
    ETHER_LOG_RAWV(ETHER_LOG_INFO, "\tPresent Family Found: %s\n",
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
    do
    {
        if (NULL != extensions)
        {
            free(extensions);
            extensions = NULL;
        }
        extension_count = 0;
        if (VK_SUCCESS != (result = vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL)))
        {
            ETHER_LOG_RAWV(ETHER_LOG_INFO, "!!! Failed to get extension count: %s !!!\n", string_VkResult(result));
            _score = 0; // disable the GPU
            goto EXIT;
        }

        extensions = malloc(sizeof(VkExtensionProperties) * extension_count);
        if (NULL == extensions)
        {
            ETHER_LOG_RAWV(ETHER_LOG_INFO, "!!! Failed to allocate extension property list: %s !!!", strerror(errno));
            _score = 0; // disable the GPU
            goto EXIT;
        }

        if (VK_SUCCESS != (result = vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, extensions)))
        {
            if (VK_INCOMPLETE == result)
                continue; // count changed
            ETHER_LOG_RAWV(ETHER_LOG_INFO, "!!! Failed to get extension properties: %s !!!\n", string_VkResult(result));
            _score = 0; // disable the GPU
            goto EXIT;
        }
    } while (VK_INCOMPLETE == result);

    // check for the VK_KHR_SWAPCHAIN_EXTENSION_NAME
    bool foundSync2 = false;
    bool foundMaintenance1 = false;
    for (uint32_t ei = 0; ei < extension_count; ei++)
    {
        if (strcmp(extensions[ei].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
        {
            ether_device->supports_swapchain = true;
        }
        if (strcmp(extensions[ei].extensionName, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME) == 0)
        {
            foundSync2 = true;
        }
        if (strcmp(extensions[ei].extensionName, VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME) == 0)
        {
            foundMaintenance1 = true;
        }
    }
    ETHER_LOG_RAWV(ETHER_LOG_INFO, "\tSwapchain Support: %s\n", ether_device->supports_swapchain ? "Yes (PASS)" : "No (FAIL)");
    ETHER_LOG_RAWV(ETHER_LOG_INFO, "\tSync2 Support: %s\n", foundSync2 ? "Yes (PASS)" : "No (FAIL)");
    ETHER_LOG_RAWV(ETHER_LOG_INFO, "\tMaintenance1 Support: %s\n", foundMaintenance1 ? "Yes (PASS)" : "No (FAIL)");

    if (!ether_device->supports_swapchain || !foundMaintenance1 || !foundSync2)
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
        ETHER_LOG_RAWV(ETHER_LOG_INFO, "!!! Failed to get physical device surface capabilities: %s !!!\n", string_VkResult(result));
        _score = 0;
        goto EXIT;
    }
    memcpy(&ether_device->surface_capabilities, &_surface_capes.surfaceCapabilities, sizeof(VkSurfaceCapabilitiesKHR));
    ETHER_LOG_RAWV(ETHER_LOG_INFO, "\tSurface Capabilities:\n\t\tMinImageCount: %d\n\t\tMaxImageCount: %d\n\t\tMinImageExtent: %dx%d\n\t\tMaxImageExtend: %dx%d\n\t\tMaxImageArrayLayers: %d\n",
                   ether_device->surface_capabilities.minImageCount,
                   ether_device->surface_capabilities.maxImageCount,
                   ether_device->surface_capabilities.minImageExtent.width,
                   ether_device->surface_capabilities.minImageExtent.height,
                   ether_device->surface_capabilities.maxImageExtent.width,
                   ether_device->surface_capabilities.maxImageExtent.height,
                   ether_device->surface_capabilities.maxImageArrayLayers);
    ETHER_LOG_RAWV(ETHER_LOG_INFO, "\t\tColorAttachment Flag: %s\n",
                   ether_device->surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ? "Present (PASS)" : "Not Present (FAIL)");
    ETHER_LOG_RAWV(ETHER_LOG_INFO, "\t\tTransferDst Flag: %s\n",
                   ether_device->surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT ? "Present (PASS)" : "Not Present (FAIL)");
    if (!(ether_device->surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) || !(ether_device->surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
    {
        _score = 0;
        goto EXIT;
    }
    ETHER_LOG_RAWV(ETHER_LOG_INFO, "\t\tAlphaOqaque Flag: %s\n",
                   ether_device->surface_capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR ? "Present (PASS)" : "Not Present (FAIL)");
    if (!(ether_device->surface_capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR))
    {
        _score = 0;
        goto EXIT;
    }

    // and formats
    do
    {
        if (NULL != ether_device->surface_formats)
        {
            free(ether_device->surface_formats);
            ether_device->surface_formats = NULL;
        }
        ether_device->surface_format_count = 0;
        if (VK_SUCCESS != (result = vkGetPhysicalDeviceSurfaceFormats2KHR(device, &surfaceInfo, &ether_device->surface_format_count, NULL)))
        {
            ETHER_LOG_RAWV(ETHER_LOG_INFO, "!!! Failed to get physical device surface format count: %s !!!\n", string_VkResult(result));
            _score = 0;
            goto EXIT;
        }
        ether_device->surface_formats = malloc(sizeof(VkSurfaceFormat2KHR) * ether_device->surface_format_count);
        if (NULL == ether_device->surface_formats)
        {
            ETHER_LOG_RAWV(ETHER_LOG_INFO, "!!! Failed to allocate surface format list: %s\n", strerror(errno));
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
            if (VK_INCOMPLETE == result)
                continue; // count changed
            ETHER_LOG_RAWV(ETHER_LOG_INFO, "!!! Failed to get physical device surface formats: %s !!!\n", string_VkResult(result));
            _score = 0;
            goto EXIT;
        }
    } while (VK_INCOMPLETE == result);

    ETHER_LOG_RAWV(ETHER_LOG_INFO, "\tSurface Formats [%d]:\n", ether_device->surface_format_count);
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
        ETHER_LOG_RAWV(ETHER_LOG_INFO, "\t\t[%d] (%s)\n\t\t\tFormat: %s\n\t\t\tColor Space: %s\n",
                       si, supported_format ? "Supported" : "Unsupported",
                       string_VkFormat(ether_device->surface_formats[si].surfaceFormat.format),
                       string_VkColorSpaceKHR(ether_device->surface_formats[si].surfaceFormat.colorSpace));
    }
    if (!hasSupportedFormat)
    {
        ETHER_LOG_RAW(ETHER_LOG_INFO, "\t\t!!! Missing Supported Format !!!\n");
        _score = 0;
        goto EXIT;
    }

    // then present modes
    do
    {
        if (NULL != ether_device->surface_present_modes)
        {
            free(ether_device->surface_present_modes);
            ether_device->surface_present_modes = NULL;
        }
        ether_device->surface_present_mode_count = 0;

        if (VK_SUCCESS != (result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, app->vk_surface, &ether_device->surface_present_mode_count, NULL)))
        {
            ETHER_LOG_RAWV(ETHER_LOG_INFO, "!!! Failed to get physical device surface present mode count: %s !!!\n", string_VkResult(result));
            _score = 0;
            goto EXIT;
        }
        ether_device->surface_present_modes = malloc(sizeof(VkPresentModeKHR) * ether_device->surface_present_mode_count);
        if (NULL == ether_device->surface_present_modes)
        {
            ETHER_LOG_RAWV(ETHER_LOG_INFO, "!!! Failed to allocate surface present mode list: %s\n", strerror(errno));
            _score = 0;
            goto EXIT;
        }
        if (VK_SUCCESS != (result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, app->vk_surface, &ether_device->surface_present_mode_count, ether_device->surface_present_modes)))
        {
            if (VK_INCOMPLETE == result)
                continue; // count changed
            ETHER_LOG_RAWV(ETHER_LOG_INFO, "!!! Failed to get physical device surface present modes: %s !!!\n", string_VkResult(result));
            _score = 0;
            goto EXIT;
        }
    } while (VK_INCOMPLETE == result);

    ETHER_LOG_RAWV(ETHER_LOG_INFO, "\tPresent Modes [%d]:\n", ether_device->surface_present_mode_count);
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
        ETHER_LOG_RAWV(ETHER_LOG_INFO, "\t\t%s (%s)\n", string_VkPresentModeKHR(ether_device->surface_present_modes[pmi]), supported ? "Supported" : "Unsupported");
    }
    if (!hasSupportedPresentMode)
    {
        ETHER_LOG_RAW(ETHER_LOG_INFO, "\t\tMissing Supported Present Mode\n");
        _score = 0;
        goto EXIT;
    }

    ETHER_LOG_RAWV(ETHER_LOG_INFO, "\tScore: %d\n\tCompatiable: %s\n", _score, _score > 0 ? "True" : "False");

EXIT:

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

    VkResult vk_result;
    uint32_t deviceCount = 0;
    VkPhysicalDevice *devices = NULL;
    ether_device_t *ether_devices = NULL;

    uint32_t highest_score = 0;
    ether_device_t *best_device = NULL;

    PARAM_CHECK(NULL != app, rc = ETHER_EPARAM, goto EXIT);
    PARAM_CHECK(NULL != app->vk_instance, rc = ETHER_EPARAM, goto EXIT);
    PARAM_CHECK(NULL != device, rc = ETHER_EPARAM, goto EXIT);

    float queuePriorities[2] = {1.0f, 1.0f}; // 1.0 max allowed

    do
    {
        if (NULL != devices)
        {
            free(devices);
            devices = NULL;
        }
        if (NULL != ether_devices)
        {
            free(ether_devices);
            ether_devices = NULL;
        }
        deviceCount = 0;

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

        if (VK_SUCCESS != (vk_result = vkEnumeratePhysicalDevices(app->vk_instance, &deviceCount, devices)))
        {
            if (VK_INCOMPLETE == vk_result)
                continue; // count changed
            ETHER_ELOGV("Failed to enumerate physical devices: %s", string_VkResult(vk_result));
            rc = ETHER_EVK_INIT;
            goto EXIT;
        }
    } while (VK_INCOMPLETE == vk_result);

    uint32_t di = 0;
    PROFILER_GET_TIMER(device_compatibility);
    for (; di < deviceCount; di++)
    {
        PROFILER_START_TIMER(device_compatibility_timer);
        get_device_compatibility(app, devices[di], ether_devices + di);
        PROFILER_END_TIMER();
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
        VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME};
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
