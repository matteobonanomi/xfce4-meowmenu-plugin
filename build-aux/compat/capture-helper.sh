#!/usr/bin/env bash
#
# Records desktop-helper invocations without opening applications. Tests create
# symlinks with the helper names they want to observe and prepend that directory
# to PATH.

set -euo pipefail

log_file="${MEOWMENU_CAPTURE_LOG:?MEOWMENU_CAPTURE_LOG must name the capture log}"
exit_status="${MEOWMENU_CAPTURE_EXIT_STATUS:-0}"
error_path="${MEOWMENU_CAPTURE_ERROR_PATH:-}"

case "$exit_status" in
  ''|*[!0-9]*)
    echo "MEOWMENU_CAPTURE_EXIT_STATUS must be an integer from 0 to 255" >&2
    exit 64
    ;;
esac
if ((exit_status > 255)); then
  echo "MEOWMENU_CAPTURE_EXIT_STATUS must be an integer from 0 to 255" >&2
  exit 64
fi

# One append-only block keeps concurrent CI diagnostics readable. Shell-escaped
# values preserve every argument boundary without evaluating captured input.
{
  printf 'BEGIN\n'
  printf 'executable=%q\n' "$(basename "$0")"
  printf 'cwd=%q\n' "$PWD"
  printf 'argc=%d\n' "$#"
  index=0
  for argument in "$@"; do
    printf 'argv_%d=%q\n' "$index" "$argument"
    index=$((index + 1))
  done
  printf 'exit_status=%d\n' "$exit_status"
  if [[ -n "$error_path" ]]; then
    printf 'error_path=%q\n' "$error_path"
  fi
  printf 'END\n'
} >>"$log_file"

if [[ -n "$error_path" ]]; then
  printf 'captured helper failure: %s\n' "$error_path" >&2
fi
exit "$exit_status"
