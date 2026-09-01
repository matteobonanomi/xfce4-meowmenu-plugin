---
layout: default
title: Support and compatibility
nav_order: 5
---

# Support and compatibility

This page records testing evidence and compatibility boundaries independently
from package availability.

Before final 1.0.0, including every 0.x release and RC, compatibility and
configuration preservation are not guaranteed in any way. From final 1.0.0
onward, configuration preservation is guaranteed. Back up panel configuration
before testing a pre-1.0 release.

## Distro testing

| Distribution/context | Maintainer | Community | CI  |
| ----------------------| :----------:| :---------:| :---:|
| Debian 13            | ✓          | ✓         | ✓   |
| Xubuntu 26.04        | ✓          | ✓         | ✓   |
| Arch Linux           | ✓          | —         | ✓   |
| MX Linux             | —          | ✓         | —   |
| Fedora 44            | —          | —         | ✓   |

- **Maintainer** means a manual result recorded by the maintainer for the
  named environment.
- **Community** means a manual result recorded by a community tester for the
  named environment.
- **CI** means automated build, test, or package-recipe evidence for the named
  distribution context; it is not a live desktop result.
- **—** means no result is currently documented for that source, not a known
  failure or incompatibility.

CI marks describe automation for the named row, not a live desktop result.
Evidence is never transferred to the Maintainer or Community columns.

## Package availability

Project release packages are available for Ubuntu 26.04, Debian 13, and
Fedora 44. Arch Linux uses a separately maintained AUR source recipe. See the
[installation guide](installation) for current artifacts and commands.
The tag workflow builds, tests, installs, and verifies the three native
packages and validates the Arch recipe before publication. Availability,
successful package automation, and recipe publication do not create
maintainer or community testing marks.

## Xfce compatibility

MeowMenu supports Xfce 4.16 through 4.21, with Xfce 4.20 as the primary
quality target.

Routine automated builds cover Ubuntu 26.04, Debian 13, and Fedora 44. Live
validation remains focused on Xfce 4.20 with X11 on `x86_64`/`amd64`; other
supported Xfce versions do not have a documented live result here.

## Sessions and architectures

X11 on `x86_64`/`amd64` is the only officially supported live-quality path.
Wayland is experimentally supported with a graceful positioning fallback but
remains unverified live. Published packages cover `x86_64`/`amd64`; other
architectures have no documented package or live result. Source compilation
does not establish a session, architecture, or live desktop result.

See [known limitations](known-limitations), [translation status](translations),
and the [testing guide](testing) for the exact boundaries. Share a result using
the [compatibility report](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/issues/new?template=compatibility-report.yml).
