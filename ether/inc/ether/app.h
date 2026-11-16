#ifndef ETHER_APP_H
#define ETHER_APP_H

typedef struct ether_app ether_app_t; // forward declare

#include "ether/platform/device.h"

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

typedef struct ether_app
{
    SDL_Window *sdl_window;
    SDL_Event event;
    bool should_quit;

    VkInstance vk_instance;
    VkSurfaceKHR vk_surface;

    ether_device_t device;

    VkSwapchainKHR swapchain;
    uint32_t image_count;
    VkImage *images;
    VkImageView *image_views;

    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkSemaphore image_available;
    VkSemaphore *renders_finished;
    VkFence in_flight;
    bool pending_swapchain_rebuild;

    VkFence present_fence;
    bool present_pending;

    // Raytracer output
    VkImage ray_image;
    VkDeviceMemory ray_memory;
    VkImageView ray_view;
    VkFormat ray_format;

    // Shader resource bindings
    VkDescriptorSetLayout ray_descriptor_layout;
    VkDescriptorPool ray_descriptor_pool;
    VkDescriptorSet ray_descriptor_set;

    // Compute pipeline
    VkPipelineLayout ray_pipeline_layout;
    VkPipeline ray_pipeline;

} ether_app_t;

#endif