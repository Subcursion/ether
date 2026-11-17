+++
date = '2026-11-14T15:29:03-06:00'
draft = true
title = 'Hugo Setup'
+++

# What went great
Installing on Garuda was great
```bash
sudo pacman -S hugo
```
And I installed dart-sass like this (pardon the invalid characters)
```bash
subcursion@sublegion in repo: ether/site on  main (718ff84) +12/- [x!+?]
 󰛓 ❯ ll -R ~/Programs/sass
drwxr-xr-x   - subcursion 14 Nov 15:00 src
.rwxr-xr-x 592 subcursion 10 Nov 19:48 sass

/home/subcursion/Programs/sass/src:
.rwxr-xr-x 5.7M subcursion 10 Nov 19:48 dart
.rw-r--r--  84k subcursion 10 Nov 19:48 LICENSE
.rw-r--r-- 5.2M subcursion 10 Nov 19:48 sass.snapshot

subcursion@sublegion in repo: ether/site on  main (718ff84) +12/- [x!+?]
 󰛓 ❯ ll ~/.local/bin/sass
lrwxrwxrwx - subcursion 14 Nov 15:04 /home/subcursion/.local/bin/sass -> /home/subcursion/Programs/sass/sass
```

# Selecting the Sass compiler

Installing Dart Sass did not initially stop Hugo from warning about LibSass. The theme was still using the default transpiler in its `toCSS` calls.

The fix was to override the theme’s style partials locally under `site/layouts/_partials/head/` and explicitly select Dart Sass:

```go-html-template
{{ $cssOpts := dict "transpiler" "dartsass" "targetPath" "css/coder.css" }}
{{ $styles := resources.Get "scss/coder.scss" | css.Sass $cssOpts }}
```

The actual overrides preserve template execution, source maps, minification, and fingerprints where the original theme uses them. Theme, dark, RTL, and custom SCSS paths select the same compiler. The theme submodule remains unchanged.

The useful distinction: installing a compiler and configuring the build to use it are separate steps. The LibSass warning is resolved. Dart Sass also reports deprecations in the theme’s existing Sass syntax; these originate from the stylesheets, not from selecting the wrong compiler.

Otherwise, install a theme via git submodule (I used Coder) worked great and the server updated immediately with my new content.