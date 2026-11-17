+++
draft = true
date = 2026-11-16T18:02:30-06:00
title = "Resource ownership, partial initialization, and cleanup"
toc = true
tocOpen = false
+++

## When to return here

When adding a Vulkan object, changing initialization order, or investigating shutdown after an initialization failure.

## Order follows dependencies

The top-level cleanup order is rendering resources, swapchain resources, logical device, surface, instance, then SDL window and SDL shutdown. Dependent objects need their parents alive for destruction, and resources must no longer be in use.

This is more precise than “destroy everything in reverse order”: ownership and outstanding work determine the actual constraints.

## Own the container separately from its contents

| Resource | Cleanup responsibility |
|---|---|
| Swapchain `VkImage` handles returned by Vulkan | Destroy the swapchain; don’t destroy these images individually. |
| Allocated array containing those handles | Free the host allocation. |
| Image views created by Ether | Destroy each created view, then free the array. |
| Semaphores created by Ether | Destroy each created semaphore after its use is finished, then free the array. |
| Future independently created tracer image | Track the image, view, and memory allocation under their own ownership. |

The planned tracer image therefore cannot blindly copy the cleanup treatment of swapchain-owned images.

## Make partial initialization visible to cleanup

The application state starts zeroed. Swapchain creation records its image count and arrays in that state before populating the handles. Image-view and semaphore arrays are zero-initialized, and destruction skips null handles. A failure partway through a creation loop therefore leaves the successful handles reachable by the same cleanup routine used after normal operation.

This connects two separate responsibilities: destroy each created Vulkan object, then free the host array containing its handle. Freeing the array alone does not destroy those objects.

Variables used by cleanup are initialized before any parameter check that jumps to a cleanup label. This matters even when the declaration includes `= NULL`: jumping past the declaration skips that initialization.

## Preserve operation results through shutdown

Cleanup functions continue tearing down resources while logging failures. The main operation’s return code survives that process, and a log-close failure changes it only when no earlier operation failed.

`main` maps the result to `EXIT_SUCCESS` or `EXIT_FAILURE`. Internal error codes remain useful in logs, but returning them directly is unsuitable on Linux: only the low eight bits become the shell exit status, so codes such as 512 can appear as success.

Temporary `ETHER_DELAY` results stay within the engine’s retry loop. They represent a deferred frame, such as waiting for a usable drawable size, rather than an application failure.

See [the synchronization note](../frame-synchronization/) for the relationship between pending work, fences, and safe destruction.

## Revisit when

Adding real tracer image allocations, descriptor pools, pipelines, or recoverable initialization. Resource fields and placeholder source files already exist, but their presence is not evidence that those objects are currently created or used.

## Code anchors

- `ether/src/main.c`
- `ether/src/rendering/vulkan_state.c`
- `ether/src/rendering/swapchain.c`

Updated against the working tree on November 17, 2026. Source paths above identify the implementation described here.

