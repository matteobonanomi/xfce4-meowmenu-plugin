---
layout: default
title: Known limitations
nav_order: 6
---

# Known limitations

These are release boundaries and unverified areas, not automatically confirmed
defects.

- **Wayland:** experimental and not live-validated for RC1. Optional
  `gtk-layer-shell` positioning is used when available; otherwise MeowMenu
  keeps its existing fallback behavior. X11 is the primary quality path.
- **Architecture:** published packages and live checks cover
  `x86_64`/`amd64`. Other architectures are unverified source-build candidates.
- **Xfce versions:** the build floor is 4.16, while current live validation is
  on Xfce 4.20 only.
- **Automated versus live coverage:** builds and unit tests do not prove panel
  placement, compositor behavior, or end-to-end desktop use. The
  [support matrix](support) labels live evidence separately.
- **Translations:** catalog syntax and completeness are measured
  automatically, but only Italian currently has maintainer linguistic review.
  See [translation status](translations).
- **Calculator:** inline results depend on an available `bc`, `qalc`, or
  `gcalccmd` engine. When no selected engine is available, normal launcher
  search remains available.
- **Visual environments:** dark themes, scaled displays, vertical panels, and
  multi-monitor layouts have not been systematically covered for RC1. They are
  useful optional checks, not publication gates by themselves.

Confirmed problems should be filed in the
[issue tracker](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/issues)
with a reproducible environment. When a boundary becomes a confirmed defect,
this page will link its issue.
