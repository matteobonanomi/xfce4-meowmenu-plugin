---
title: FAQ
nav_order: 10
has_children: false
---

# FAQ

### Is this a fork of Whisker Menu? Will it stay in sync with upstream?

Yes, MeowMenu originated as a fork of Whisker Menu by Graeme Gott. No, it
will not track upstream — MeowMenu is now a standalone project with its own
settings schema (`meowmenu.xml`), roadmap, and release cycle. The current
translation inventory contains 56 catalogs.

### Can I install MeowMenu alongside Whisker Menu?

They use separate package names, panel identifiers, and configuration. This is
a factual packaging property, not a separately certified compatibility
contract.

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

### Can I contribute a translation?

Yes — edit the relevant `po/<lang>.po` file, run `msgfmt --check`, and open a
pull request. Italian has maintainer review; reviews for every other language
are especially welcome. See [translation status](translations).
