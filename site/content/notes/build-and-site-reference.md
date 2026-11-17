+++
draft = true
date = 2026-11-16T18:02:30-06:00
title = "Build, diagnostics, and site reference"
toc = true
tocOpen = false
+++

## When to return here

When adding a source file, checking how diagnostics are generated, or reproducing the project and site builds.

## Commands already provided by the repository

Run from the repository root:

```sh
just configure
just build
just run
```

`just configure` defaults to Debug and runs `cmake -S ether -B build`. To configure a different type explicitly, use `just configure Release`; be aware that `just build` invokes its own default configure dependency again. Use `cmake --build build` after the explicit configuration if you intend to retain it.

The executable is `build/ether`; the program appends diagnostics to `./log.txt` relative to its working directory.

## Choices preserved in CMake

- C17 is required, with C extensions disabled.
- SDL3 and Vulkan are linked through imported targets.
- `pubinc` is public and `inc` is private in the target include configuration. The current target is an executable; this is groundwork, not a delivered library API.
- Recursive source globbing uses `CONFIGURE_DEPENDS`.
- Compiler-specific warnings are treated as errors.
- The cglm headers are included as a system dependency, and `CGLM_FORCE_DEPTH_ZERO_TO_ONE` is defined.

Source globbing is an explicit convenience choice mentioned in the existing CMake notes. Keep track of its tradeoff rather than presenting it as a universal rule; [CMake’s file documentation](https://cmake.org/cmake/help/latest/command/file.html#filesystem) discusses source globbing and `CONFIGURE_DEPENDS`.

The prefix-map option removes the checkout prefix from source paths embedded in logs and debug information. CMake checks compiler support with `CheckCCompilerFlag`, applies the option only to the Ether target, and skips the GCC/Clang option for MSVC. A successful compiler-specific flag check establishes support for that option, not portability of the entire application.

## Diagnostics are part of the investigation record

The logging layer attaches severity, time, file, line, and function. The return-code list uses an X-macro to generate the enum and related mapping logic. These are project utilities, not Vulkan requirements.

Keep the useful part of a debugging session: exact validation identifier or message, the related call, the missing condition, and the fix commit. Thousands of lines of repeated startup output are harder to search than one captured failure and explanation.

Logging uses severity-filtered macros: `ETHER_LOGV` adds the standard header and a newline; `ETHER_LOG_RAWV` writes the supplied text for multiline reports. Filtering is applied to output statements, leaving device compatibility calculations independent of the chosen verbosity.

The stream is configured with `setvbuf(stream, NULL, _IOLBF, 0)` during logger initialization, before any logging. Completed newline-terminated records are flushed for live viewing with `tail -f`. Without line buffering or explicit flushing, a full stdio buffer can be written halfway through a message while the remaining bytes stay in memory. Raw records must provide their own newline. See the [stream buffering reference](https://www.man7.org/linux/man-pages/man3/setvbuf.3.html).

## Site setup evidence

The site is Hugo with the Coder theme. The Pages workflow checks out submodules recursively, builds from `site/`, and uploads `site/public`. It pins Hugo to `0.169.0`; `e245bb3` changed that pin. This note records the repository configuration rather than recommending a current Hugo version.

Installing Dart Sass makes the compiler available; selecting it is a separate template setting. The local `site/layouts/_partials/head/` overrides select `css.Sass` with `transpiler = dartsass` for the theme, dark scheme, RTL, and custom SCSS paths. This removes the LibSass warning without modifying the Coder submodule. The workflow already installs Dart Sass.

The selected compiler accepts the theme’s existing stylesheets. Dart Sass emits its own deprecation warnings for older syntax in the vendored styles; those are distinct from Hugo selecting LibSass. See [Hugo’s Sass options](https://gohugo.io/functions/css/sass/) and the [setup note](../hugo-setup/).

Use `hugo server --buildDrafts` from `site/` to review drafts locally. These reference notes are marked as drafts.

## Code anchors

- `ether/CMakeLists.txt`
- `justfile`
- `ether/src/loggig.c`
- `site/layouts/_partials/head/`
- `ether/inc/ether/logging.h`
- `ether/pubinc/ether/rc.h`
- `.github/workflows/hugo.yml`

Updated against the working tree on November 17, 2026. Source paths above identify the implementation described here.

