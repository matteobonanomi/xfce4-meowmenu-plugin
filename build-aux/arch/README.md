# Arch packaging helpers

Shellcheck-clean shell helpers for the Arch package validation, called by
`.github/workflows/packaging.yml`. Each script runs standalone in an
`archlinux:base-devel` container; the workflow only orchestrates them.
