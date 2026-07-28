---
layout: default
title: Support and compatibility
nav_order: 5
---

# Support and compatibility

MeowMenu 1.0.0-rc1 is the current release candidate. Upgrades from earlier
MeowMenu releases are intended to preserve configuration.

## Evidence tiers

- **Source-build compatible:** configuration, compilation, and every
  applicable automated test pass for the named library generation.
- **Staged-install compatible:** source-build evidence plus installation into
  an isolated root and dependency-sensitive action checks.
- **Published package target:** a clean manifest-derived package build,
  complete tests, artifact installation, linkage/helper inspection, and
  installed-action checks.
- **Maintainer-published source recipe:** the Arch recipe resolves, builds,
  tests, lints, and installs from its own metadata; it is not a prebuilt
  project package.
- **Live validated:** the desktop interactions were separately completed in
  the named session, architecture, versions, and installed artifact.

| Environment | Current evidence requirement | Live evidence |
|---|---|---|
| Xubuntu 26.04, amd64 package | Published package target | Required before publication: Xfce 4.20, X11, amd64 |
| Debian 13, amd64 package | Published package target | Not implied by package automation |
| Fedora 44, x86_64 package | Published package target | Not implied by package automation |
| Current Arch, x86_64 | Maintainer-published source recipe | Not implied by recipe automation |
| Xfce 4.16 libraries | Source-build compatible | Unvalidated live |
| Xfce 4.18 libraries | Source-build compatible | Unvalidated live |
| Xfce 4.20 libraries | Source and package gates | Primary X11 live target |
| libxfce4ui 4.21 or newer | Staged-install compatible with Exo absent | Optional and separately reported |

A lower evidence tier never implies a higher one. In particular, automated
action capture is not a live desktop result, and the replacement library stack
is not described as a separately validated stable “Xfce 4.21” desktop.

## Platform boundary

The build floor remains Xfce 4.16, with continuous source evidence for 4.16,
4.18, and 4.20. X11 on `x86_64`/`amd64` is the primary live-tested path.
Wayland dependency selection is identical but remains unverified live; optional
positioning support degrades gracefully. Published packages cover
`x86_64`/`amd64`; other architectures have no current release evidence.

See [known limitations](known-limitations), [translation status](translations),
and the [testing guide](testing) for the exact boundaries. Share a result using
the [compatibility report](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/issues/new?template=compatibility-report.yml).
