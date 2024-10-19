# wsm
wsm is an pc and Mobile Phone [Wayland] compositor for lychee.

## Contributing to wsm

Please refer to the [contributing document](CONTRIBUTING.md) for everything you need to know to get started contributing to wsm.

## Release Signatures

## Installation

### From Packages

### Compiling from Source
Install dependencies:

build dependencies(for arch linux):
* meson \*
* [wlroots]
* wayland
* wayland-protocols \*
* libdrm
* systemd
* libxcb
* xcb-util-wm
* libxml2
* doxygen
* graphviz
* libxslt
* xmlto

build dependencies(for debian):
* meson 
* libwlroots-dev 
* libwayland-dev
* wayland-protocols
* libsystemd-dev
* libdrm-dev
* libxcb1-dev
* libxcb-icccm4-dev
* libxml2-dev 2.12.6+dfsg-0exp2
* doxygen
* graphviz
* libxslt1-dev xmlto

Run these commands:
```shell
    meson build/ --prefix=/usr --buildtype=debug -Dxwayland=enabled
    sudo ninja -C build/ install
```

## doxygen
set the documentation in meson_options.txt to enabled, reuse meson to compile, and you will see that the documentation has been generated in the build/doc/doxygen/html/wsm directory.

## Running
Run `wsm --xwayland` from a TTY or in Xorg/Wayland desktop environment. Some display managers may work but are not supported by wsm (gdm is known to work fairly well).

## Roadmap
* Support mobile phone pull-down and slide up gestures
* Support window effects, including but not limited to fillet and blur、Minimize 
* Support restricting applications from obtaining sensitive privacy data of user equipment
* Support hardware layer rendering(muti drm-plane)

[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
