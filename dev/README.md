# `dev/` — development tooling

Everything in this folder is for **development purposes only**. None of it ships
in a package or is required to build, install, or run MeowMenu from a release.

- `install.sh` — build, install, and hot-reload the plugin in a running
  Xfce session. Run it from the repo root as `./dev/install.sh`.
- `uninstall.sh` — remove a dev install and reset all stored configuration
  so the next install is detected as genuinely fresh. Run it as
  `./dev/uninstall.sh`.
- `docs/` — maintainer notes (architecture, UX, install/build references). This
  subfolder is git-ignored; the scripts and this README are tracked.
