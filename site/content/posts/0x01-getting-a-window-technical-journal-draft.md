+++
draft = true
date = 2026-11-16T18:02:30-06:00
title = "0x01 - Getting a Window (Technical Journal Draft)"
description = "A Vulkan hello world: device selection, feature activation, and learning what the renderer is waiting for."
authors = ['subcursion']
toc = true
tocOpen = false
+++

I really thought getting a window open wouldn’t be hard.

I used OpenGL a long time ago, so my expectation was roughly: ask SDL for a window, get a graphics context, tell it what size to render at, then tell it what to draw. A bit of setup, then onto the interesting stuff.

I was not ready for how asynchronous Vulkan is, or how much I would need to describe before anything could happen. Here is what roughly 2.2k lines of code got me:

![Getting a Window](/ether/screenshots/get-a-window-result.png)

A single color. Love it.

This is Ether’s first rendering milestone. I deliberately left scene management and compute shaders out of it; I wanted the graphics equivalent of “hello world.” I chose Vulkan because I’d already tried OpenGL and wanted to learn something new while building the tracer. I just didn’t expect this first step to involve quite so much homework.

The implementation is in [PR #5](https://github.com/Subcursion/ether/pull/5). I’ll focus here on the decisions and gotchas that made this more than a window-creation exercise. The code excerpts are shortened to show those parts, rather than complete functions.

## SDL was the familiar part

Installing SDL3, finding it with CMake, and creating the window went pretty smoothly. The important flag in my window creation call was `SDL_WINDOW_VULKAN`:

```c
app->sdl_window = SDL_CreateWindow(
    window_title, width, height,
    SDL_WINDOW_VULKAN |
    SDL_WINDOW_RESIZABLE |
    SDL_WINDOW_HIGH_PIXEL_DENSITY);
```

That got me the SDL window. On the Vulkan side, I still needed an instance, a surface associated with that window, a device, and the resources used to submit and present work.

This was my first adjustment: creating the window and having a renderer ready to use it were very different amounts of work.

## Device selection became a compatibility check

I’d seen someone enumerate the available GPUs, give each one a score, and choose the highest-scoring device. That seemed like a reasonable starting point, so I did something similar.

As I developed it, the checks became as important as the scoring. Ether currently favors a discrete GPU, but it first needs a device that supports the Vulkan version, queue capabilities, surface usage, and image formats the implementation expects. In my selection code, failing a required check gives the device a score of zero.

One example is the image I eventually want the tracer to write into. I’m planning to keep that separate from the swapchain images used for presentation. That gives me room to work with floating-point color values and later add scaling and tone mapping before displaying the result.

The format I chose is `VK_FORMAT_R16G16B16A16_SFLOAT`. In `get_device_compatibility`, I query its format properties and check the operations I’m planning to use:

```c
VkFormatProperties format_properties;
vkGetPhysicalDeviceFormatProperties(
    device, ray_image_format, &format_properties);

VkFormatFeatureFlags ray_image_feature_req =
    VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
    VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
    VK_FORMAT_FEATURE_BLIT_SRC_BIT;

if ((format_properties.optimalTilingFeatures & ray_image_feature_req)
    != ray_image_feature_req)
{
    _score = 0;
    goto EXIT;
}
```

The storage-image requirement comes from the planned compute writes. The transfer and blit requirements reflect the image-transfer path I’m anticipating. This check is only one part of device compatibility; it doesn’t implement that path or make a blit a replacement for tone mapping.

That’s an assumption I’m building into selection before the tracer exists. It narrows the devices Ether will accept, and I expect to revisit it as I support more formats or change how the final image reaches the swapchain. For now, logging each check gives me a useful picture of what my GPU supports and what my application requires.

## Checking support wasn’t enabling it

Several of my validation errors came from checking that something was supported, then forgetting to actually enable it. I had established that I *could* use it and somehow treated that as having asked to use it.

Synchronization2 is one example. During device selection, I put its feature structure into the query chain and inspect the result. Later, during logical-device creation, I request it by setting `synchronization2` to `VK_TRUE` and including that structure in the device’s `pNext` chain.

Here are the relevant pieces, with the other chained feature structures omitted:

```c
// Query support during device selection.
VkPhysicalDeviceSynchronization2Features sync2 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES};
VkPhysicalDeviceFeatures2 features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    .pNext = &sync2};
vkGetPhysicalDeviceFeatures2(device, &features);

// Later, request the feature during logical-device creation.
VkPhysicalDeviceSynchronization2Features requested_sync2 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
    .synchronization2 = VK_TRUE};
// Included in VkDeviceCreateInfo's pNext chain.
```

The [Synchronization2 feature reference](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceSynchronization2Features.html) describes the query and enable steps. There are also extension names to request where required. Feature structures and extension names are related pieces of setup, but neither is a substitute for checking that I’ve requested the right thing.

Using the wrong Vulkan constants caught me a few times too. There are a lot of similar-looking names when all of this is new. Validation gave me something concrete to investigate instead of just staring at a window that wasn’t doing what I wanted.

## What is this frame waiting for?

Synchronization was the part where I had to stop and do more reading. Initially, I couldn’t imagine already having enough context to know which objects to create, let alone how to put them together.

The distinction I needed to make was between a function returning and the work actually finishing. My implementation currently has one frame in flight, but even that frame has several different points of completion:

- The swapchain image becomes available for the submitted work to use.
- The command-buffer submission finishes, allowing the CPU to reuse its resources.
- Presentation releases associated resources so they can be safely reused or cleaned up.

I use an image-available semaphore for acquisition, a render-finished semaphore for presentation to wait on, and an in-flight fence for the CPU to wait on before reusing the command buffer. I also use a present fence through swapchain maintenance functionality to track presentation resource lifetime.

The submission in `ether_render` waits on acquisition at the transfer stage, because the first operation I perform on the image is a transfer clear:

```c
VkSemaphoreSubmitInfo waitInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .semaphore = app->image_available,
    .stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT};

VkSemaphoreSubmitInfo signalInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .semaphore = app->renders_finished[image_index],
    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};
```

These structures go into the submission alongside the command buffer. Presentation then waits on `renders_finished[image_index]`.

That index matters. I allocate a render-finished semaphore per swapchain image, rather than assuming that my single in-flight fence makes every presentation semaphore safe to reuse. Completion of the rendering submission doesn’t, by itself, prove that presentation has finished using its wait semaphore. The [Vulkan guide on swapchain semaphore reuse](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html) explains this distinction.

Likewise, I’m treating the present fence as a resource-lifetime signal, rather than a notification that a particular pixel has physically appeared on the monitor. The [present-fence reference](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainPresentFenceInfoKHR.html) spells out the guarantees.

I think I’ve reached a decent starting point, but I expect to revisit this when I add multiple frames in flight. For now, one frame gives me enough to reason about.

## Even clearing a color has a sequence

The actual recorded work is tiny compared to the setup. In `ether_render_record`, I transition the acquired swapchain image to `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`, clear it, then transition it to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`.

For the first transition I use `VK_IMAGE_LAYOUT_UNDEFINED` as the old layout because I’m replacing the whole image and don’t need its previous contents. That’s a decision specific to this clear; preserving image contents would require a different approach.

And then, finally, the rendering command:

```c
VkClearColorValue clearColor = {
    .float32 = {0.02f, 0.05f, 0.12f, 1.0f}};

vkCmdClearColorImage(
    app->command_buffer,
    app->images[image_index],
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    &clearColor,
    1,
    &range);
```

Yep. That’s the entire visible achievement for now. No graphics pipeline or compute shader needed for this clear, which let me keep the milestone focused on getting work submitted and presented.

## Resizing reaches into the renderer

A resizable SDL window also meant dealing with swapchain recreation. The window size affects the images I need to present, so I can’t just update a width and height in my application state and carry on.

The event loop marks the swapchain for rebuilding on resize. The renderer also checks acquisition and presentation results, and recreates the swapchain and dependent resources when needed.

One detail worth keeping separate is `VK_ERROR_OUT_OF_DATE_KHR` versus `VK_SUBOPTIMAL_KHR` from image acquisition. With the former, my code schedules a rebuild and returns without submitting a frame. With the latter, an image was still acquired, so my code schedules the rebuild but continues through the current frame.

That difference affects synchronization as well as image handling: successful acquisition has an associated semaphore signal to account for. Treating both results as “return now and try again” would skip that part of my normal submission path. The [acquisition reference](https://docs.vulkan.org/refpages/latest/refpages/source/vkAcquireNextImageKHR.html) documents the result codes and semaphore behavior.

I didn’t set out to spend this much of the first milestone thinking about state, but resizing makes it hard to avoid.

## What I have, and what I don’t

I have a window that clears to a color, a device-selection path, swapchain recreation, and the synchronization and cleanup needed for this implementation. There’s still no scene, compute pipeline, or ray tracing output. A dark blue window is a small visible result, but getting here changed how I think about what the renderer has to coordinate.

My biggest surprise was how much of the job happens around the drawing command. Describing requirements, enabling features, and working out when resources are safe to reuse took far more thought than choosing the color.

Next up is the compute pipeline. I expect some of these early assumptions to change once the tracer is doing real work. At least I now have somewhere to put the result.
