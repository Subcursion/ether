+++
draft = true
date = 2026-11-16T18:02:30-06:00
title = "0x01 - Getting a Window (Journal Draft)"
description = "My first Vulkan milestone: a window, a single color, and a lot more setup than I expected."
authors = ['subcursion']
toc = false
+++

I really thought getting a window open wouldn’t be hard.

I used OpenGL a long time ago, so I had a rough idea of what I expected: ask SDL for a window, get a graphics context, tell it what size to render at, and then tell it what to draw. A bit of setup, then onto the interesting stuff.

Here is what roughly 2.2k lines of code got me:

![Getting a Window](/ether/screenshots/get-a-window-result.png)

A single color. Love it. There’s a lot going on behind that very uneventful window.

## The small milestone that wasn’t

Ether is supposed to become a ray tracer, but I deliberately kept scene management and compute shaders out of this milestone. Some preliminary reading suggested I didn’t need either just to put something on screen. I wanted the graphics equivalent of “hello world.”

SDL’s part went pretty smoothly: install the library, link it through CMake, initialize it, and create a window with Vulkan support. That felt familiar enough.

Vulkan was where my expectations fell apart. Before I could clear anything to a color, I was dealing with an instance, a surface, physical and logical devices, queue families, a swapchain, command buffers, and synchronization objects. Each step introduced something else I needed to understand before I could use it.

I had chosen Vulkan partly because I’d already tried OpenGL and wanted to learn something new while building the tracer. So yes, I signed up for this. I just didn’t expect the first window to involve quite this much homework.

## Can this device actually do what I need?

One of the first pieces that grew was device selection. I’d seen someone enumerate the available GPUs, give each one a score, and choose the highest-scoring device. That seemed like a reasonable place to start.

As I developed it, the checks became more about compatibility than preference. Favoring a discrete GPU is one thing; making sure it supports the operations and image formats I intend to use is another. A high score isn’t much use if I can’t render with the device.

That also pulled a few future decisions into this milestone. I plan to render into an image separate from the images used for presentation, so I can work with HDR values and eventually handle scaling and tone mapping before displaying the result. I’m not implementing all of that yet, but I am already checking for the format support I expect to need.

I’m sure the selection logic will change as I support more formats. For now, it gives me a way to see what my GPU supports and why Ether accepts or rejects it.

## Supported doesn’t mean enabled

This was a recurring source of validation errors: I would check that an extension or feature was supported, then forget to actually enable it. I’d established that I _could_ use something and somehow treated that as having asked to use it.

Using the wrong Vulkan constants caught me a few times too. There are a lot of very similar-looking names when all of this is new.

The validation layers were useful here. They gave me something concrete to investigate when I’d missed a step. Checking support and enabling features are separate jobs. Apparently I needed to learn that more than once.

## The part I wasn’t ready for: waiting

The biggest surprise was how asynchronous the whole thing is. I came in expecting to tell the library what to draw and then show the result. Instead, I needed to think about which work might still be happening after a function returned, and what I could safely reuse or destroy.

Synchronization took more reading than the other pieces. Initially, I couldn’t imagine having enough context to know to create these objects, let alone employ them in this way. Seeing a semaphore or fence declaration didn’t tell me why I needed it at that particular point.

What helped was thinking through what each stage was waiting for: an image becoming available, rendering finishing, and presentation reaching the point where associated resources could be reused or cleaned up. Those are different events, even though my original mental model had bundled them together into “draw the frame.”

I’m keeping one frame in flight for now. There’s enough to understand without trying to make several frames overlap immediately. I think I’ve reached a decent starting point, but this is definitely a part I expect to revisit.

## A window is enough for now

The result still makes me laugh a little. All that setup, and the visible achievement is a dark blue window. But I can clear an image, present it, handle a resize, and clean up afterward. That’s the milestone I set out to reach.

The main thing I’m taking away is how much my OpenGL expectations shaped what I thought this would involve. I expected a little setup followed by drawing. I spent much more time describing what the application needs and figuring out how to coordinate the work.

Next up is the compute pipeline. No spheres yet, but at least I have somewhere to put one when I get there.

The implementation for this milestone is in [PR #5](https://github.com/Subcursion/ether/pull/5).
