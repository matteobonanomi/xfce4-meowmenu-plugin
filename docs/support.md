---
layout: default
title: Support and compatibility
nav_order: 5
---

# Support and compatibility

MeowMenu 0.9.0-rc1 is a prerelease on the path to 1.0.0. While it is current,
RC1 is the only version receiving best-effort support. Interfaces may still
change before 1.0.0, which will be the first release described as stable.
Upgrades from 0.8.0 are intended to preserve configuration.

## Evidence for 0.9.0-rc1

- **Built** means the exact candidate compiled and its automated tests passed.
- **Installs** means Built plus the produced package installed cleanly and
  reported the expected package-native version.
- **Ran it** means Installs plus the [core desktop checklist](testing) passed
  in a live Xfce session for that exact candidate and environment.

| Environment | Built | Installs | Ran it |
|---|---|---|---|
| Xubuntu 26.04, amd64 package | Required publication gate | Required publication gate | Required before publication: Xfce 4.20, X11, amd64 |
| Debian 13, amd64 package | Required publication gate | Required publication gate | No RC1 record yet; manual testing dates back to 0.5.x |
| Arch Linux, x86_64 recipe | Required publication gate | Required recipe smoke test | No RC1 record yet; manual testing dates back to 0.7.x |
| Fedora 44, x86_64 package | Required publication gate | Required publication gate | No manual record; package CI only |

The public RC1 release is created only after every Built and Installs gate in
this table passes. A public release note or compatibility report records the
primary Xubuntu **Ran it** evidence. Historical use—Xubuntu since 0.1.x,
Debian since 0.5.x, and Arch since 0.7.x—does not count as RC1 evidence.

## Platform boundary

The build requires Xfce 4.16 or newer. Current live release validation targets
Xfce 4.20; other compatible Xfce versions are unverified. X11 on
`x86_64`/`amd64` is the primary tested path. Wayland is experimental and
unverified, with optional positioning support and graceful fallback.
Maintainer-published binaries cover `x86_64`/`amd64`; other architectures may
be source-build candidates but have no current release evidence.

See [known limitations](known-limitations), [translation status](translations),
and the [testing guide](testing) for the exact boundaries. Share a result using
the [compatibility report](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/issues/new?template=compatibility-report.yml).
