#ifndef ETHER_DEVICE_H
#define ETHER_DEVICE_H

#include "ether/rc.h"

#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stdbool.h>

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

// include after above so it's already defined. forward declare doesn't work here
#include "ether/app.h"

ether_rc ether_pick_device(ether_app_t *app, ether_device_t *device);

void ether_device_destroy(ether_app_t *app);

#endif