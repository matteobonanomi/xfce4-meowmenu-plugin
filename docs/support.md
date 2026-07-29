---
layout: default
title: Support and compatibility
nav_order: 5
---

# Support and compatibility

This page records testing evidence and compatibility boundaries independently
from package availability. Upgrades from earlier MeowMenu releases are
intended to preserve configuration.

## Distro testing

| Distribution/context | Maintainer | Community | CI |
|---|:---:|:---:|:---:|
| Debian 13 | ✓ | ✓ | — |
| Debian | — | — | ✓ |
| Xubuntu 26.04 | ✓ | ✓ | — |
| Arch Linux | ✓ | — | ✓ |
| MX Linux | — | ✓ | — |
| Fedora 44 | — | — | ✓ |
| Ubuntu | — | — | ✓ |

- **Maintainer** means a manual result recorded by the maintainer for the
  named environment.
- **Community** means a manual result recorded by a community tester for the
  named environment.
- **CI** means automated build, test, or package-recipe evidence for the named
  distribution context; it is not a live desktop result.
- **—** means no result is currently documented for that source, not a known
  failure or incompatibility.

Ubuntu CI does not count as Xubuntu 26.04 live testing. Distribution-level
Debian CI does not create a Debian 13 CI result. A row can therefore carry
more than one mark, but evidence is never transferred to a related row or
another provenance column.

## Package availability

Project release packages are available for Ubuntu 26.04, Debian 13, and
Fedora 44. Arch Linux uses a separately maintained AUR source recipe. See the
[installation guide](installation) for current artifacts and commands.
Availability, successful package automation, and recipe publication do not
create maintainer or community testing marks.

## Xfce compatibility

MeowMenu supports Xfce 4.16 through 4.21, with Xfce 4.20 as the primary
quality target.

| Compatibility path | Automated evidence | Boundary |
|---|---|---|
| Xfce 4.16 libraries | Source configure, build, and tests with Exo | Supported source stack |
| Xfce 4.18 libraries | Source configure, build, and tests with Exo | Supported source stack |
| Xfce 4.20 libraries | Source configure, build, and tests with Exo | Primary quality target |
| libxfce4ui 4.21 or newer | Successor source cell and staged install without Exo | Dependency-transition boundary only |

The 4.16, 4.18, and 4.20 rows are automated source-stack evidence, not distro
or live desktop results. The libxfce4ui 4.21-or-newer path verifies the
dependency transition without Exo; it is not a separately live-validated Xfce
4.21 desktop and does not claim compatibility with every future library
release.

## Sessions and architectures

X11 on `x86_64`/`amd64` is the primary live-quality path. Wayland is supported
with a graceful positioning fallback but remains unverified live. Published
packages cover `x86_64`/`amd64`; other architectures have no documented
package or live result. Source compilation does not establish a session,
architecture, or live desktop result.

See [known limitations](known-limitations), [translation status](translations),
and the [testing guide](testing) for the exact boundaries. Share a result using
the [compatibility report](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/issues/new?template=compatibility-report.yml).
