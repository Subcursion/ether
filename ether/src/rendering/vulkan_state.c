#include "ether/rendering/vulkan_state.h"
#include "ether/app.h"
#include "ether/rc.h"
#include "ether/platform/device.h"
#include "ether/rendering/swapchain.h"
#include "ether/rendering/render.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

ether_rc ether_vulkan_init(ether_app_t *app)
{
    ether_rc rc = ETHER_SUCCESS;

    PARAM_CHECK(NULL != app, rc = ETHER_EPARAM, goto EXIT);

    ETHER_ILOG("Initializing Vulkan instance");

    app->vk_instance = VK_NULL_HANDLE;
    app->vk_surface = VK_NULL_HANDLE;

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

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    ETHER_ILOG("Initializing Vulkan surface");

    if (!SDL_Vulkan_CreateSurface(app->sdl_window, instance, NULL, &surface))
    {
        ETHER_ELOGV("Failed to create Vulkan surface: %s", SDL_GetError());
        rc = ETHER_ESDL_INIT;
        goto EXIT;
    }
    app->vk_surface = surface;

    ether_device_t device = {0};

    // pick the best device
    if (ETHER_SUCCESS != (rc = ether_pick_device(app, &device)))
    {
        goto EXIT;
    }

    memcpy(&app->device, &device, sizeof(device));

    // init swapchain
    rc = ether_generate_swapchain(app);
    if (ETHER_SUCCESS != rc)
    {
        goto EXIT;
    }

    // init renderer
    rc = ether_render_init(app);
    if (ETHER_SUCCESS != rc)
    {
        goto EXIT;
    }

EXIT:
    if (NULL != extensions)
    {
        free(extensions);
        extensions = NULL;
    }
    return rc;
}

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