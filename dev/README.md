# `dev/` — development tooling

Everything in this folder is for **development purposes only**. None of it ships
in a package or is required to build, install, or run MeowMenu from a release.

- `install.sh` — build and clean-install the plugin under `/usr/local` after
  removing older development payloads, panel slots, stored configuration,
  presets, and cached usage data. Run it as `./dev/install.sh`, then add a
  fresh Meow Menu item to the panel.
- `uninstall.sh` — completely remove development installs and reset all panel
  slots, stored configuration, presets, and cached usage data. Run it as
  `./dev/uninstall.sh`.
- `docs/` — maintainer notes (architecture, UX, install/build references). This
  subfolder is git-ignored; the scripts and this README are tracked.

Release packages use the same current fallback policy as the public package
recipes. AccountsService and gtk-layer-shell remain source-build options, not
requirements for the core launcher.
