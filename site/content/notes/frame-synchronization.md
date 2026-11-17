+++
draft = true
date = 2026-11-16T18:02:30-06:00
title = "Frame synchronization: what is safe to reuse?"
toc = true
tocOpen = false
+++

## When to return here

When a frame hangs, a semaphore cannot be reused, or command-buffer reuse and presentation lifetime become confused.

## The current frame has several completion points

Ether uses one command buffer and one frame in flight. Swapchain image count is a separate quantity; requesting three swapchain images does not create three independent sets of frame resources.

| Object | Purpose in Ether | Lifetime/indexing |
|---|---|---|
| `image_available` | Submission waits for the acquired image to become available. | One for the current single-frame design. |
| `renders_finished[image_index]` | Presentation waits for the image’s submitted work. | Per swapchain image. |
| `in_flight` | CPU waits before reusing submission resources. | One fence, initially signaled. |
| `present_fence` | Tracks presentation-associated resource lifetime. | One fence, guarded by `present_pending`. |

The acquisition wait is scoped to `VK_PIPELINE_STAGE_2_TRANSFER_BIT` because the first use of the acquired image is a transfer clear. If that first use changes, review the wait scope with it.

## Rendering completion does not settle presentation lifetime

A submission fence alone is not proof that presentation has finished using its wait semaphore. Ether allocates render-finished semaphores by swapchain-image index. See the [Khronos semaphore reuse guide](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html) for the reasoning behind that pattern.

The present fence is about the specified presentation operations and resource-lifetime guarantees. It should not be described as a timestamp saying the image has appeared on the monitor. Its exact guarantees are in the [present-fence reference](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainPresentFenceInfoKHR.html).

## A concrete history lesson: a fence needs a future signaler

Commit `3a333cc` removes an in-flight fence reset from swapchain recreation. The replacement comment explains why: leave it signaled, because the renderer waits on it before the next submission.

The failure mechanism is:

```text
wait for the previous submission
reset its fence during recreation
return to the render loop
wait for that fence before submitting anything
no new submission exists to signal it
```

The same commit moves the normal reset later, immediately before `vkQueueSubmit2`, after the present-fence wait. This reduces paths that can return with a reset fence and no associated submission.

A similar early-return trap is covered in the [Khronos recreation tutorial](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/04_Swap_chain_recreation.html).

The same rule matters during shutdown: a reset fence is not evidence of pending work. Current teardown performs its explicit fence waits when `present_pending` is set, then waits for device idleness before destroying render resources. This avoids unconditionally waiting on the reset submission fence after a failed submission.

## Layouts are part of the recorded work

`ether_render_record` records:

```text
UNDEFINED → TRANSFER_DST_OPTIMAL
clear the whole color image
TRANSFER_DST_OPTIMAL → PRESENT_SRC_KHR
```

Using `UNDEFINED` discards prior contents. That fits this full-image clear. It is not a general answer for preserving accumulated ray-tracing output.

The command pool permits individual command-buffer reset and belongs to the graphics queue family. The CPU waits on `in_flight` before resetting and rerecording its buffer. Recording completion (`vkEndCommandBuffer`) is not GPU execution completion.

## Revisit when

Adding multiple frames in flight, accumulation, compute writes, additional queues, or recovery after a failed submission. Reconsider the indexing and lifetime of every frame resource rather than only increasing the swapchain image count.

## Code anchors

- `ether/src/rendering/render.c`
- `ether/src/rendering/swapchain.c`

Updated against the working tree on November 17, 2026. Source paths above identify the implementation described here.

