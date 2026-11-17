+++
draft = true
date = 2026-11-16T18:02:30-06:00
title = "Keeping useful development notes"
toc = true
tocOpen = false
+++

These notes preserve things that would be annoying to rediscover. They don’t need to explain every line of Ether or turn each milestone into a tutorial.

## Three places for three kinds of information

| Place | Question it answers | Example from the first window |
|---|---|---|
| Journal | What changed, and how did I get there? | I expected an OpenGL-like setup and underestimated how much asynchronous work I would need to coordinate. |
| Technical note | What will I need to understand or look up again? | Resetting the in-flight fence during recreation can leave the next frame waiting without a submission to signal it. |
| Source and commit | What exactly did I implement or change? | The fence reset was removed from `ether_generate_swapchain` in `3a333cc`. |

A journal can include a technical explanation whenever it matters to the story. A note lets that explanation remain useful after the story moves on. Link between them when useful; there’s no need to repeat the whole explanation.

## What to capture while working

A few rough lines are enough:

- **Expected:** what I thought would happen.
- **Observed:** what actually happened; keep the error or result code if useful.
- **Explanation:** what I understand now, with a source if I checked one.
- **Decision:** what I changed, and what tradeoff I accepted.
- **Revisit when:** what future change might invalidate this decision.

For example:

> Expected: waiting for rendering meant everything associated with the frame was finished.
>
> Explanation: presentation has a separate lifetime for its wait semaphores.
>
> Decision: keep render-finished semaphores per swapchain image and track presentation with a present fence.
>
> Revisit when: adding multiple frames in flight or changing swapchain lifetime management.

This is a suggested way to capture the concept, not a claim that this exact sequence of thoughts happened during development.

## How these notes were compiled

The starting set was compiled from Git history through `3a333cc`, the existing setup notes, and the author’s answers about the first milestone. It was refreshed against the working tree on November 17, 2026, after the cleanup and logging changes. History establishes what changed; it cannot establish every motivation or lesson personally learned.

The author explicitly reported OpenGL-based expectations, surprise at Vulkan’s asynchronous model, extension activation and constant mistakes, and additional synchronization research. Other entries preserve reference material extracted from the implementation and explain when later rendering features will change its assumptions.

| Evidence | What it contributes |
|---|---|
| `2b01a8d`, `1906eb4`, `e245bb3` | Hugo setup, taxonomy changes, and workflow version updates. |
| `6bf7310` | CMake, just recipes, logging, and return-code groundwork. |
| `c296b5d` | SDL/Vulkan window and initial rendering lifecycle. |
| `3a333cc` | Enumeration retries, refreshed surface checks, fence lifetime corrections, and cleanup changes. |
| November 17 working tree | Correct extension requests, build configuration, cleanup ownership, deferred resize handling, exit status, and line-buffered logging. |
| Current ray-tracing files | Scaffolding for the next milestone. |

## Reference map

- [SDL and Vulkan setup](../sdl-vulkan-setup/): which object does what, and support versus activation.
- [Device selection and image formats](../device-selection-and-formats/): requirements, preferences, and future rendering assumptions.
- [Frame synchronization](../frame-synchronization/): resource reuse, fence resets, and presentation lifetime.
- [Swapchain recreation and enumeration](../swapchain-recreation-and-enumeration/): resizing, changing counts, and acquisition results.
- [Resource ownership and cleanup](../resource-ownership-and-cleanup/): partial initialization and dependency order.
- [Build and site reference](../build-and-site-reference/): commands, tooling choices, and existing setup notes.

## Turning a work session into a journal

Pick one change that matters. Start with the visible result or the problem that held it up. Explain the technical mechanism that made the difference, then say what you decided and what remains uncertain. Stop when that change is explained.

Some sessions won’t produce a story worth publishing. A screenshot, a short progress paragraph, and a link to a useful note are enough. Other sessions may deserve a detailed technical post. Consistency of length is less useful than having something specific to say.

For a portfolio, these pages can show how decisions develop and how problems are investigated. They don’t need to sound entertaining on every line. Accurate limits and concrete evidence give the work weight.
