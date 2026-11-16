#include "ether/rendering/swapchain.h"

#include "ether/rc.h"
#include "ether/app.h"
#include "ether/logging.h"
#include "ether/rendering/supported_options.h"

#include <stdint.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

ether_rc ether_generate_swapchain(ether_app_t *app)
{
    ether_rc rc = ETHER_SUCCESS;
    PARAM_CHECK(NULL != app, rc = ETHER_EPARAM, goto EXIT);
    VkResult result = VK_SUCCESS;
    VkSurfaceFormatKHR *surface_format = NULL;
    int sdl_width = 0;
    int sdl_height = 0;
    VkExtent2D size_extents = {0};

    VkSwapchainKHR swapchain = NULL;
    VkImage *_images = NULL;
    VkImageView *_image_views = NULL;

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,

        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1}};

    if (VK_NULL_HANDLE != app->swapchain)
    {
        // wait for in flight
        if (VK_NULL_HANDLE != app->in_flight)
        {
            result = vkWaitForFences(app->device.logical_device, 1, &app->in_flight, VK_TRUE, UINT64_MAX);
            if (VK_SUCCESS != result)
            {
                ETHER_ELOGV("Failed to wait for in flight: %s", string_VkResult(result));
                rc = ETHER_ERENDER;
                goto EXIT;
            }
            result = vkResetFences(app->device.logical_device, 1, &app->in_flight);
            if (VK_SUCCESS != result)
            {
                ETHER_ELOGV("Failed to reset in flight fence: %s", string_VkResult(result));
                rc = ETHER_ERENDER;
                goto EXIT;
            }
        }

        // wait for present
        if (app->present_pending)
        {
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
                ETHER_ELOGV("Failed to reset preset fence: %s", string_VkResult(result));
                rc = ETHER_ERENDER;
                goto EXIT;
            }
            app->present_pending = false;
        }

        // reset command buffer
        if (VK_NULL_HANDLE != app->command_buffer)
        {
            result = vkResetCommandBuffer(app->command_buffer, 0);
            if (VK_SUCCESS != result)
            {
                ETHER_ELOGV("Failed to reset command buffer: %s", string_VkResult(result));
                rc = ETHER_ERENDER;
                goto EXIT;
            }
        }

        // do an attempt at making sure all GPU work is finished
        ETHER_ILOG("Waiting for idle device");
        if (VK_SUCCESS != (result = vkDeviceWaitIdle(app->device.logical_device)))
        {
            ETHER_ELOGV("Failed to wait for idle device: %s", string_VkResult(result));
            // keep going regardless....
        }

        ether_swapchain_destroy(app);
    }

    app->swapchain = VK_NULL_HANDLE;
    app->image_count = 0;
    app->images = NULL;
    app->image_views = NULL;

    // when using separate families, we provide this array for CONCURRENT access
    uint32_t concurrent_families[] = {app->device.graphics_family, app->device.present_family};

    // need to re-query the surfac capabilities
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

    // pick image count
    uint32_t image_count = 3;         // preferred
    uint32_t created_image_count = 0; // actual

    if (image_count < app->device.surface_capabilities.minImageCount)
    {
        ETHER_WLOGV("Preferred image count of %d too small, increasing to %d",
                    image_count,
                    app->device.surface_capabilities.minImageCount);
        image_count = app->device.surface_capabilities.minImageCount;
    }

    if (app->device.surface_capabilities.maxImageCount != 0 &&
        image_count > app->device.surface_capabilities.maxImageCount)
    {
        ETHER_WLOGV("Preferred iamge count of %d too large, decreasing to %d",
                    image_count,
                    app->device.surface_capabilities.maxImageCount);
        image_count = app->device.surface_capabilities.maxImageCount;
    }

    ETHER_ILOGV("Creating swapchain, requesting %d images", image_count);

    // get surface format
    for (uint32_t si = 0; si < app->device.surface_format_count; si++)
    {
        // find one in supported formats
        for (uint32_t i = 0; i < sizeof(supported_surface_formats) / sizeof(supported_surface_formats[0]); i++)
        {
            if (memcmp(&app->device.surface_formats[si].surfaceFormat, supported_surface_formats + i, sizeof(VkSurfaceFormatKHR)) == 0)
            {
                surface_format = &app->device.surface_formats[si].surfaceFormat;
                break;
            }
        }
        if (NULL != surface_format)
        {
            break;
        }
    }
    if (NULL == surface_format)
    {
        ETHER_ELOG("Unable to find any compatible surface formats in device.");
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }

    ETHER_ILOGV("Using format:%s, colorspace:%s", string_VkFormat(surface_format->format), string_VkColorSpaceKHR(surface_format->colorSpace));

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

    // get images and imageviews
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
        ETHER_ELOGV("Failed to get swapchain images: %s", string_VkResult(result));
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }
    ETHER_ILOG("Image references retrieved");

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

    // all good, assign to app
    app->image_count = created_image_count;
    app->images = _images;
    app->image_views = _image_views;

EXIT:
    if (ETHER_SUCCESS != rc)
    {
        ether_swapchain_destroy(app);
        if (NULL != _images)
        {
            free(_images);
            _images = NULL;
        }
        if (NULL != _image_views)
        {
            free(_image_views);
            _image_views = NULL;
        }
    }
    return rc;
}

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