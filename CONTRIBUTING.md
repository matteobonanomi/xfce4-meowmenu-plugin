# Contributing to MeowMenu

Thanks for helping improve MeowMenu. Discuss non-trivial behavior changes in an
issue before investing substantial work.

## Build and test

Install the dependencies listed in the
[installation guide](docs/installation.md#build-from-source), then run:

```bash
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

Match the surrounding C++/GTK style and avoid unrelated formatting changes.
New user-visible strings must use the existing gettext macros. Regenerate the
template with `meson compile -C build xfce4-meowmenu-plugin-pot`, then merge
each catalog with `msgmerge --no-fuzzy-matching --backup=none`. Review every
changed translation individually; leave uncertain wording empty so the source
English fallback is used. Remove obsolete and previous-source blocks after
checking that no active translation was lost. Update a catalog's content only
when supplying an actual translation or an intentional fallback decision.

Before opening a translation change, validate the affected catalog with
`msgfmt --check --check-format -o /dev/null po/<locale>.po`. For the complete catalog set,
regenerate and check the status page with
`python3 build-aux/translation-status.py --write`
followed by `python3 build-aux/translation-status.py --check`, then run
`python3 build-aux/localization-audit.py`.

Update public documentation when behavior, installation, or settings change.
Every new setting needs a safe default, a Preferences control,
reset-to-default behavior, and migration coverage. Note Debian, RPM, Arch, and
optional-dependency effects when packaging changes.

X11 is the only officially supported environment and the primary manual test
path. Wayland is experimental; record its documented fallback behavior
separately from X11 results.

## Pull requests

Keep changes focused and explain the user-facing reason. Describe tests run and
any manual X11 or Wayland result without presenting automated tests as desktop
runtime evidence. Link the relevant issue and call out documentation,
translation, packaging, settings/default, and migration impact.
