+++
draft = true
date = 2026-11-16T18:02:30-06:00
title = "SDL and Vulkan: setup and feature activation"
toc = true
tocOpen = false
+++

## When to return here

When a Vulkan command complains about an unavailable feature, or when it is unclear which initialization step owns a piece of setup.

## What each object contributes in Ether

| Object or step | Role in this implementation |
|---|---|
| SDL window | The native window, created with `SDL_WINDOW_VULKAN`. |
| Vulkan instance | Entry point for instance-level operations and physical-device discovery. |
| Surface | Connection to the window’s presentation target. |
| Physical device | A candidate whose capabilities can be queried. |
| Logical device | The device created with requested queues, extensions, and features. |
| Swapchain | Presentation images configured for the surface. |
| Command pool and buffer | Storage for commands to submit to the selected queue family. |

The SDL Vulkan integration supplies the platform-specific instance extensions and surface creation helper. It does not create the whole renderer. See [SDL’s Vulkan overview](https://wiki.libsdl.org/SDL3/CategoryVulkan).

## Supported, requested, and usable are separate checks

The author reported repeatedly querying support without enabling what was needed, as well as using incorrect constants. Keep those failure modes distinct:

1. Query the capability or feature needed by the application.
2. Check extension availability and dependencies where applicable.
3. Request the relevant extensions and enable feature bits at device creation.
4. Use the corresponding commands only under that configuration.

For Synchronization2, Ether queries through `VkPhysicalDeviceFeatures2`, then puts a feature structure with `synchronization2 = VK_TRUE` in the logical-device creation chain. The [feature reference](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceSynchronization2Features.html) documents both uses.

`pNext` is a chain of typed structures, each identified by `sType`. Initializing a pointer chain correctly is separate from requesting extension names. Even extensible output structures such as `VkSurfaceFormat2KHR` require the caller to initialize their headers before querying into them.

## Maintenance and swapchain maintenance are different extensions

`VK_KHR_MAINTENANCE1_EXTENSION_NAME` names the general maintenance extension. The present fences used by Ether belong to swapchain maintenance instead.

Ether requests:

- `VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME` on the logical device, together with the `swapchainMaintenance1` feature.
- `VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME` on the instance, alongside the SDL-required extensions and `VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME`.

Device selection compares the enumerated extension names with `strcmp(...) == 0`; zero means the names match. The [Khronos extension reference](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_swapchain_maintenance1.html) identifies the swapchain-maintenance extension and its dependencies.

The useful lesson is to match the extension name, feature structure, and commands being used. Similar names do not imply the same functionality.

## Validation is development feedback

Ether requests `VK_LAYER_KHRONOS_validation` under `#ifndef NDEBUG`. Debug declares the layer array with a count of one; Release uses a null pointer and zero count. This keeps validation configuration explicit without an empty array initializer.

The API version requested by the application is also separate from enabling optional feature bits. Selecting Vulkan 1.4 does not mean every feature is enabled.

## Code anchors

- `ether/src/rendering/vulkan_state.c`
- `ether/src/platform/device.c`

Updated against the working tree on November 17, 2026. Source paths above identify the implementation described here.

