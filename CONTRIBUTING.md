# Contributing to MeowMenu

Thanks for helping improve MeowMenu. Contributions of any size are welcome.

For a small fix, you can open a pull request directly. Please open an issue
first if the change affects how MeowMenu looks or behaves in a significant way.

## Build and test

Install the dependencies from the
[source-build guide](docs/installation.md#build-from-source), then run:

```bash
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

For interface changes, also open MeowMenu and check the behavior yourself. The
[short testing guide](docs/testing.md) provides a quick checklist.

## Keep changes easy to review

- Focus on one problem at a time.
- Follow the style of the surrounding code.
- Avoid unrelated formatting changes.
- Update the user documentation when visible behavior changes.
- Use the existing gettext macros for new text shown in the interface.

You do not need to test every supported distribution or desktop setup. Tell us
what you tested, including your distribution, Xfce version, and session type if
they are relevant.

## Pull requests

In the pull request, briefly explain:

- what changed;
- why it is useful;
- how you tested it;
- anything that still needs checking.

Translation-only contributions are also welcome. See the
[translation page](docs/translations.md) for the required validation command.
