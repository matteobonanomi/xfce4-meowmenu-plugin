---
title: FAQ
nav_order: 5
has_children: false
---

# FAQ

### Is this a fork of Whisker Menu? Will it stay in sync with upstream?

Yes, MeowMenu originated as a fork of Whisker Menu by Graeme Gott. No, it
will not track upstream — MeowMenu is now a standalone project with its own
settings schema (`meowmenu.xml`), feature roadmap, and release cycle. All
54 upstream translations are preserved.

### Can I install MeowMenu alongside Whisker Menu?

Yes. They are separate XFCE panel plugins and can coexist. You can have both
buttons on the panel simultaneously.

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

Yes — edit the relevant `po/<lang>.po` file in the repository, run
`msgfmt --check po/<lang>.po` to validate it, and open a Pull Request against
the `development` branch. Only Italian has been validated by a native speaker
so far; all other languages were extended via LLM-assisted translation and
are warmly awaiting review.
