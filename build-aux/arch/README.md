# Arch packaging helpers

Shellcheck-clean shell helpers for the Arch package validation, called by
`.github/workflows/packaging.yml`. Each script runs standalone in an
`archlinux:base-devel` container; the workflow only orchestrates them.

The package recipe disables AccountsService and gtk-layer-shell so the release
artifact uses the core fallback. Source builds may enable those integrations
through Meson when their development packages are available.
