+++
draft = true
date = 2026-11-16T18:02:30-06:00
title = "Device selection and image-format requirements"
toc = true
tocOpen = false
+++

## When to return here

When adding a format, accepting another GPU, or moving from a color clear to compute output.

## Compatibility before preference

The author started from an example of enumerating GPUs and scoring them. In Ether, the mechanism became both a compatibility filter and a preference system.

`get_device_compatibility` assigns zero on a failed required check. Devices that survive receive preferences based on device type and queue capabilities; `ether_pick_device` selects the highest score. The score expresses application policy, not measured performance.

The current requirements include the targeted API version, graphics and presentation families, feature checks, swapchain support, surface usage flags, a matching surface format, and an acceptable present mode.

A graphics queue and a present queue may belong to different families. Ether records both indices. If they differ, swapchain creation selects concurrent sharing for those families. That addresses image sharing between families; it does not remove the need to synchronize accesses.

## The format choice already assumes a future path

Current configured options:

| Purpose | Choice |
|---|---|
| Planned tracer output | `VK_FORMAT_R16G16B16A16_SFLOAT` |
| Surface format | `VK_FORMAT_B8G8R8A8_SRGB` |
| Surface color space | `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR` |
| Present mode | `VK_PRESENT_MODE_FIFO_KHR` |

The planned tracer image is separate from the swapchain. The author’s intention is to retain floating-point values, then handle scaling and tone mapping before presentation. The first milestone clears swapchain images directly; that future path is not implemented by the format checks.

The tracer format currently requires storage-image, transfer-source, and blit-source feature bits under `optimalTilingFeatures`. The selected surface format is checked for transfer-destination, color-attachment, and blit-destination support.

The mask test is:

```c
(available & required) == required
```

This checks that every required bit is present. Testing whether the intersection is merely nonzero would accept partial support.

## Limits to remember

Format properties are a preliminary capability check, not a complete image-creation guarantee for every combination of usage, dimensions, and flags. When implementing the output image, check the exact intended configuration as well.

A blit and a tone-mapping operation are separate concepts. Selecting an HDR-capable intermediate format does not implement display conversion or an exposure curve.

The version comparison checks the major version first and compares minor versions only when the majors match. A higher major version is not rejected merely because its minor version is lower.

The current milestone selects graphics and presentation families for clearing and presenting images. Compute capability is a separate requirement for the later compute pipeline; queue scoring and queue selection serve different purposes.

## Revisit when

Adding compute dispatch, additional surface formats, a shader-based presentation pass, or multiple candidate GPUs with meaningful performance differences.

## Code anchors

- `ether/src/platform/device.c`
- `ether/inc/ether/rendering/supported_options.h`

Updated against the working tree on November 17, 2026. Source paths above identify the implementation described here.

