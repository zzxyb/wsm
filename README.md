# wsm
wsm is an pc and Mobile Phone [Wayland] compositor for lychee.

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
    meson build/ --prefix=/usr --buildtype=debug
    sudo ninja -C build/ install
```

## doxygen
set the documentation in meson_options.txt to enabled, reuse meson to compile, and you will see that the documentation has been generated in the build/doc/doxygen/html/wsm directory.

## Configuration


## Running
Run `wsm` from a TTY or in Xorg desktop environment. Some display managers may work but are not supported by wsm (gdm is known to work fairly well).

## How to Contribute
* Contributing just involves sending a merge request.
* Note: rules are made to be broken. Adjust or ignore any/all of these as you see
fit, but be prepared to justify it to your peers.

### Code Style
* wsm is written in C with a style similar to the kernel style, but with a
few notable differences.
* Try to keep your code conforming to C11 and POSIX as much as possible, and do
not use GNU extensions.

### Code Architecture Guidelines
* The code should be simple and easy to understand.
* Add comments to key nodes whether you change or add new code
* Security > Compatibility > Extensibility >= Performance

### Contribution Guideline
* Contribution steps.
    1. First login to your Github account and fork the project
    2. Pull the forked project locally using `git clone`.
    3. Push the new commit to your project using `git push`.
    4. commit your code to the upstream project on Github using the Pull Requese feature.
* commit message specification: use English. Be sure to describe exactly what the commit "does" and "why it was made"
* A commit only does one thing, and the smaller the code changes, the easier it is to accept the commit. For larger code changes, try to split the commit into multiple commits (satisfying the git commit principle as a prerequisite)
* Please do your own testing and code review before committing the code, and submit the PR after confirming that the code is working correctly

## Roadmap
* Support mobile phone pull-down and slide up gestures
* Support window effects, including but not limited to fillet and blur、Minimize 
* Support restricting applications from obtaining sensitive privacy data of user equipment
* Support GL API and hardware layer rendering

[wlroots]: https://gitlab.freedesktop.org/wlroots/wlroots
