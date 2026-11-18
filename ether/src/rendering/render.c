#include "ether/rendering/render.h"
#include "ether/rc.h"
#include "ether/app.h"
#include "ether/logging.h"
#include "ether/rendering/swapchain.h"
#include "ether/rendering/supported_options.h"
#include "ether/profiler.h"

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <string.h>
#include <stdlib.h>

ether_rc ether_render_init(ether_app_t *app)
{
    ether_rc rc = ETHER_SUCCESS;
    PARAM_CHECK(NULL != app, rc = ETHER_EPARAM, goto EXIT);
    VkResult result = VK_SUCCESS;
    // ensure it's all null

    app->command_pool = VK_NULL_HANDLE;
    app->command_buffer = VK_NULL_HANDLE;
    app->image_available = VK_NULL_HANDLE;
    app->in_flight = VK_NULL_HANDLE;
    app->present_fence = VK_NULL_HANDLE;
    app->present_pending = false;

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

    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    ETHER_ILOG("Creating image available semaphore");
    result = vkCreateSemaphore(app->device.logical_device, &semaphoreInfo, NULL, &app->image_available);
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to create image available semaphore: %s", string_VkResult(result));
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }

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

    ETHER_ILOG("Creating present fence");
    fenceInfo.flags = 0; // do not set it to be signaled right away
    result = vkCreateFence(app->device.logical_device, &fenceInfo, NULL, &app->present_fence);
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to create present fence: %s", string_VkResult(result));
        rc = ETHER_EVK_INIT;
        goto EXIT;
    }

    // init ray tracing specific resources
    // Raytracer output
    app->ray_image = VK_NULL_HANDLE;
    app->ray_memory = VK_NULL_HANDLE;
    app->ray_view = VK_NULL_HANDLE;
    app->ray_format = ray_image_format;

    // Shader resource bindings
    app->ray_descriptor_layout = VK_NULL_HANDLE;
    app->ray_descriptor_pool = VK_NULL_HANDLE;
    app->ray_descriptor_set = VK_NULL_HANDLE;

    // Compute pipeline
    app->ray_pipeline_layout = VK_NULL_HANDLE;
    app->ray_pipeline = VK_NULL_HANDLE;

    ETHER_ILOG("Render init finished");
EXIT:
    if (ETHER_SUCCESS != rc)
    {
        // cleanup
        ether_render_destroy(app);
    }
    return rc;
}

static ether_rc ether_render_record(ether_app_t *app, uint32_t image_index)
{
    // set up command buffer to transfer from undefined (don't care about contents) to transfer to clear
    ether_rc rc = ETHER_SUCCESS;
    VkResult result = VK_SUCCESS;
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

    result = vkEndCommandBuffer(app->command_buffer);
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to end command buffer: %s", string_VkResult(result));
        rc = ETHER_ERENDER;
        goto EXIT;
    }
EXIT:
    return rc;
}

ether_rc ether_render(ether_app_t *app)
{
    ether_rc rc = ETHER_SUCCESS;
    PARAM_CHECK(NULL != app, rc = ETHER_EPARAM, goto EXIT);
    VkResult result = VK_SUCCESS;
    uint32_t image_index = 0;

    PROFILER_GET_TIMER(ether_generate_swapchain);
    PROFILER_GET_TIMER(vkWaitForFences);
    PROFILER_GET_TIMER(vkAcquireNextImageKHR);
    PROFILER_GET_TIMER(ether_render_record);
    PROFILER_GET_TIMER(submit_frame);

    // rebuild swapchain if need be
    if (app->pending_swapchain_rebuild)
    {
        PROFILER_START_TIMER(ether_generate_swapchain_timer);
        rc = ether_generate_swapchain(app);
        PROFILER_END_TIMER();
        if (ETHER_SUCCESS != rc)
        {
            goto EXIT;
        }

        app->pending_swapchain_rebuild = false;
    }

    PROFILER_START_TIMER(vkWaitForFences_timer);
    result = vkWaitForFences(app->device.logical_device, 1, &app->in_flight, VK_TRUE, UINT64_MAX);
    PROFILER_END_TIMER();
    if (VK_SUCCESS != result)
    {
        ETHER_ELOGV("Failed to wait for in flight fence: %s", string_VkResult(result));
        rc = ETHER_ERENDER;
        goto EXIT;
    }

    PROFILER_START_TIMER(vkAcquireNextImageKHR_timer);
    result = vkAcquireNextImageKHR(app->device.logical_device,
                                   app->swapchain,
                                   UINT64_MAX,
                                   app->image_available,
                                   VK_NULL_HANDLE,
                                   &image_index);
    PROFILER_END_TIMER();
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

    PROFILER_START_TIMER(ether_render_record_timer);
    result = vkResetCommandBuffer(app->command_buffer, 0);
    if (VK_SUCCESS != result)
    {
        PROFILER_END_TIMER();
        ETHER_ELOGV("Failed to reset command buffer: %s", string_VkResult(result));
        rc = ETHER_ERENDER;
        goto EXIT;
    }

    // record commands
    rc = ether_render_record(app, image_index);
    PROFILER_END_TIMER();
    if (ETHER_SUCCESS != rc)
    {
        goto EXIT;
    }

    PROFILER_START_TIMER(submit_frame_timer);

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

EXIT:
    if (submit_frame_timer.started)
    {
        PROFILER_END_TIMER();
    }
    return rc;
}

void ether_render_destroy(ether_app_t *app)
{
    PARAM_CHECK(NULL != app, ETHER_ELOG("app null"), return);
    VkResult result = VK_SUCCESS;

    // and wait for present to be done
    if (app->present_pending)
    {
        // if a present was pending then in flight is also triggered
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