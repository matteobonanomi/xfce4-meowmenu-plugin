---
layout: default
title: Known limitations
nav_order: 6
---

# Known limitations

These are current technical boundaries and unverified areas, not automatically
confirmed defects.

- **Wayland:** experimental and not live-validated. Interactive
  resizing uses a release-to-apply fallback: the menu stays fixed while the
  primary button is held, then applies the final constrained size once on
  release. Cancelling or hiding during the gesture restores the starting
  geometry and preserves the last completed normal size. This behavior covers
  both the normal GTK toplevel and optional `gtk-layer-shell`; compositor and
  layer-shell combinations without separately recorded results remain
  unvalidated. See the [session boundaries](support#sessions-and-architectures).
  X11 is the primary live-resize quality path.
- **Architecture:** published packages and live checks cover
  `x86_64`/`amd64`. Other architectures are unverified source-build candidates.
- **Xfce versions:** source-stack builds for the 4.16, 4.18, and 4.20 library
  generations are available through an explicit compatibility run, while
  routine distro CI and current live validation remain separate. Live
  validation is on Xfce 4.20 only. On Xfce 4.16, copied desktop launchers can
  be made executable but cannot receive the newer trusted marker; the desktop
  may show its normal trust confirmation before first launch.
- **Optional Wayland positioning:** an unavailable or too-old
  `gtk-layer-shell` disables that integration without blocking the core build.
  MeowMenu then uses its normal session-compatible fallback.
- **Automated versus live coverage:** builds and unit tests do not prove panel
  placement, compositor behavior, or end-to-end desktop use. The
  [support page](support) keeps automated and live evidence separate.
- **Translations:** catalog syntax and completeness are measured
  automatically, but only Italian currently has maintainer linguistic review.
  See [translation status](translations).
- **Calculator:** inline results depend on an available `bc`, `qalc`, or
  `gcalccmd` engine. When no selected engine is available, normal launcher
  search remains available.
- **Visual environments:** dark themes, scaled displays, vertical panels, and
  multi-monitor layouts have not been systematically covered. They are useful
  optional checks, not publication gates by themselves.

Confirmed problems should be filed in the
[issue tracker](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/issues)
with a reproducible environment. When a boundary becomes a confirmed defect,
this page will link its issue.
