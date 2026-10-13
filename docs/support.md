---
layout: default
title: Support and compatibility
nav_order: 5
---

# Support and compatibility

## Choosing a release

Release candidates are the more stable choice. Regular 0.x.x releases are for
users who want the latest features and improvements. Both are published on the
[GitHub Releases page](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/releases).

## Packages

| System | Installation source |
|--------|---------------------|
| Ubuntu 26.04 | Package attached to each GitHub release |
| Debian 13 | Package attached to each GitHub release |
| Fedora 44 | Package attached to each GitHub release |
| Arch Linux and derivatives | Maintainer-published AUR package |

See [Installation](installation) for commands and source-build instructions.

## Supported systems

| Area | Current support |
|------|-----------------|
| Xfce | Versions 4.16 through 4.21; 4.20 is the primary target |
| Session | X11 is supported; Wayland is experimental |
| Architecture | Published packages are available for `x86_64`/`amd64` |

Wayland remains usable when optional layer-shell support is unavailable, but
menu positioning can differ between compositors.

Other architectures may work when built from source, but they do not currently
have published packages or regular testing.

## Tested distributions

| Distribution | Maintainer tested | Community tested | Automated build |
|--------------|:-----------------:|:----------------:|:---------------:|
| Debian 13 | ✓ | ✓ | ✓ |
| Xubuntu 26.04 | ✓ | ✓ | ✓ |
| Arch Linux | ✓ | — | ✓ |
| MX Linux | — | ✓ | — |
| Fedora 44 | — | — | ✓ |

An automated build confirms that MeowMenu compiles and passes its test suite.
It does not replace testing in a running Xfce desktop.

See [Known limitations](known-limitations) for current caveats. If you try
MeowMenu on another setup, you can share the result with a
[compatibility report](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/issues/new?template=compatibility-report.yml).
