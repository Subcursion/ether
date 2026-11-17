+++
draft = true
date = 2026-11-16T18:02:30-06:00
title = "Swapchain recreation and changing enumeration results"
toc = true
tocOpen = false
+++

## When to return here

When resizing fails, an enumeration result is incomplete, or the requested swapchain image count differs from the actual count.

## A surface can change after device selection

The engine loop marks `pending_swapchain_rebuild` on resize. Acquisition and presentation results can request a rebuild too. `ether_generate_swapchain` waits for tracked work, destroys dependent resources, refreshes surface capabilities and formats, and creates the replacement.

Commit `3a333cc` adds capability revalidation and format refresh to recreation. A device passing the initial selection checks does not make every cached surface property permanent.

Current creation policy includes:

- Prefer three images, clamped to the surface limits. A maximum count of zero means there is no specified maximum.
- Use `currentExtent` when the surface fixes it; otherwise clamp the pixel size reported by SDL.
- Size the image, view, and render-finished semaphore arrays from the actual retrieved image count.
- Use concurrent sharing when graphics and presentation families differ.

SDL window coordinates can differ from drawable pixels. Ether uses `SDL_GetWindowSizeInPixels`; [SDL’s window documentation](https://wiki.libsdl.org/SDL3/SDL_CreateWindow) also identifies pixel-size changes as a reason to query drawable size again.

The event loop marks a rebuild on both `SDL_EVENT_WINDOW_RESIZED` and `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`. If SDL reports a nonpositive drawable size, recreation returns `ETHER_DELAY`; the engine sleeps and retries while continuing to poll events. The frame return code resets at the start of each iteration, so a later quit does not retain the temporary delay status.

## Count, allocate, fill — and sometimes retry

Many enumeration calls take a count pointer and an optional destination array. Ether queries the count, allocates, then retrieves the elements.

`3a333cc` adds retry loops on `VK_INCOMPLETE` for device extensions, surface formats, present modes, and swapchain images. The count query is not a reservation of an immutable list. A retry must refresh the count and allocation, not merely call again with the same insufficient buffer.

For extensible output arrays such as `VkSurfaceFormat2KHR`, initialize the `sType` of each element on every allocation before filling it. Plain arrays of handles do not use that structure-header pattern.

## Acquisition: out of date and suboptimal differ

| Result | Current acquisition path |
|---|---|
| `VK_ERROR_OUT_OF_DATE_KHR` | Mark rebuild and return without submitting this frame. |
| `VK_SUBOPTIMAL_KHR` | Mark rebuild and continue with the acquired image. |
| `VK_SUCCESS` | Continue normally. |

Suboptimal acquisition still acquired an image and has associated synchronization to account for. Ether’s chosen approach is to complete that frame normally before rebuilding. It should not be generalized into a claim that the API always requires rendering that image in precisely this way. See the [acquisition reference](https://docs.vulkan.org/refpages/latest/refpages/source/vkAcquireNextImageKHR.html).

Acquisition and presentation have different synchronization effects. For presentation, `VK_ERROR_OUT_OF_DATE_KHR` still leaves the associated queue operations enqueued. Ether therefore retains `present_pending` for that result and waits for the present fence before replacing the swapchain. See the [presentation reference](https://docs.vulkan.org/refpages/latest/refpages/source/vkQueuePresentKHR.html).

## Code anchors

- `ether/src/rendering/swapchain.c`
- `ether/src/platform/device.c`
- `ether/src/runtime/runtime_loop.c`

Updated against the working tree on November 17, 2026. Source paths above identify the implementation described here.

