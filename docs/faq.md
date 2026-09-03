---
title: FAQ
nav_order: 10
has_children: false
---

# FAQ

### What is MeowMenu?

MeowMenu is a standalone Xfce panel launcher with presets, Places, flexible
layouts, keyboard navigation, and an optional Calculator. It originated as a
fork of Whisker Menu, but it has its own settings and release cycle.

### Which release should I test?

Before final 1.0.0, 0.x releases are experimental feature releases. An
available RC is the more stable testing channel. Choose the newest 0.x release
when it is newer and you want newer features.

Compatibility and configuration preservation are not guaranteed in any way
before final 1.0.0; configuration preservation is guaranteed from final 1.0.0
onward.

### The plugin doesn't appear in 'Add New Items' after install.

Run `xfce4-panel -r` to restart the panel. If it still doesn't appear,
verify the `.so` file installed correctly by running
`ls /usr/lib/xfce4/panel/plugins/libmeowmenu.so` (or
`ls /usr/local/lib/xfce4/panel/plugins/libmeowmenu.so` for source installs).
If the file is absent, the package may not have installed cleanly. Try
reinstalling.

### My custom preset isn't loading.

Check three things: (1) the file is in `~/.local/share/meowmenu/presets/`;
(2) the filename stem matches the `Id` field inside the file (e.g.,
`my-preset.meowpreset` must contain `Id=my-preset`); (3) you restarted the
panel with `xfce4-panel -r` after placing the file.

### My favourites disappeared after testing a development build. Can they be recovered?

MeowMenu preserves the favourite identifiers that are still stored, including
applications that are temporarily unavailable. It cannot reconstruct choices
that an earlier development build already erased. Restore a saved preset that
contains the missing favourites, or add the applications again manually.

### Can I contribute a translation?

Yes — edit the relevant `po/<lang>.po` file, run
`msgfmt --check --check-format -o /dev/null po/<lang>.po`, and open a pull request.
The status page
separates technical validity, translated text, intentional English fallback,
and fluent review; no language is marked fluent-reviewed before maintainer
review. See [translation status](translations).
