#ifndef ETHER_SUPPORTED_OPTIONS_H
#define ETHER_SUPPORTED_OPTIONS_H

#include <vulkan/vulkan.h>

/// @brief The supported surface formats we can render to
static const VkSurfaceFormatKHR supported_surface_formats[] = {
    {.format = VK_FORMAT_B8G8R8A8_SRGB,
     .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};

/// @brief The supported present modes for a surface
static const VkPresentModeKHR supported_present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};

/// @brief The ray trace image format we require
static const VkFormat ray_image_format =
    VK_FORMAT_R16G16B16A16_SFLOAT;
#endif