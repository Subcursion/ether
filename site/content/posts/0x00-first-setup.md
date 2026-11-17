+++ 
draft = true
date = 2026-11-14T14:50:30-06:00
title = "0x00 - Initial Setup"
description = "What I set up, why, and any other thoughts"
slug = ""
authors = ['subcursion']
tags = []
categories = []
externalLink = ""
series = []
+++

# Why Ether?

Ether is an experimental ray tracer built to
explore computer graphics from first principles.

I have spent a lot of time in Unity and Godot but haven't made full games. I find rendering and the theory a little more interesting instead of getting bogged down in UI, audio, model, or animation development. I'm not an artist (yet, perchance).

## Journal Expectations

Due to the sheer size of code blocks and macros and context behind the code, I will only share snippets of relevant blocks. This is to ensure I am not just shoving a PR your way and doing a code review of it with you reading my thoughts. There are logging and other utilities you will see me use; just know that these aren't pivotal to setting up the ray tracer and were there to make my life easier. Everyone has their own coding style and I took a bit of liberty here to program Ether in a way I felt comfortable with.

You may also see discrepancies between the shared code and the PR. That is because I write these journal entries after programming until I hit a measurable milestone and go back and review the code as I write them up. This undoubtedly brings to light issues or typos or better ways to organize the code but will not change the functionality significantly. There will be a PR associated with the journal which will contain the code fixes possibly referenced in the journal.

## Initial goals

- Understand ray generation and intersection
- Implement a basic GPU renderer using Vulkan in C
- Explore acceleration structures (BVH)
- Efficient structures (intrusive data structures) when sending large amounts of data to GPU

## Development environment

- Garuda Linux
- CMake
- C17
- Vulkan (planned)
- Huge with Coder theme on Github Pages

## Made sure Vulkan development kit was installed

```
sudo pacman -S vulkan-headers
```

## Starting Milestone Roadmap

- Get a window open
- Build a compute pipeline
- Get a sphere intersected
- Handle reflections
- Define materials and object properties
- Add mesh support (targeting .obj and .blend files)
- Create algorithms for triangle intersection
- Build BVH style hierarchy for efficient triangle intersection
- Add Global Illumination (start of at least)

### From there it becomes a matter of research and adding features such as

- Efficient denoising
- Multipass ray tracing for GI and transparency
- UI
- Scene control
- Establish API for interfacing with engine (other language bindings?)
