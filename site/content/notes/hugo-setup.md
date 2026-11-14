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

# What wrong wrong
Which works but I still get warnings from Hugo
```
WARN  deprecated: css.Sass: libsass was deprecated in Hugo v0.153.0 and will be removed in a future release. Use dartsass instead. See https://gohugo.io/functions/css/sass/#dart-sass
```

Otherwise, install a theme via git submodule (I used Coder) worked great and the server updated immediately with my new content.