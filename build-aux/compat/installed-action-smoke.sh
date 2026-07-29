#!/usr/bin/env bash
#
# Exercises the installed dependency-sensitive command contract through capture
# helpers. This is an installed/staged smoke test, not live desktop validation.

set -euo pipefail

usage() {
  echo "usage: $0 --regime legacy|successor [--staged-root DIR] [--plugin FILE]" >&2
}

regime=
staged_root=
plugin=
while (($#)); do
  case "$1" in
    --regime)
      regime="${2:-}"
      shift 2
      ;;
    --staged-root)
      staged_root="${2:-}"
      shift 2
      ;;
    --plugin)
      plugin="${2:-}"
      shift 2
      ;;
    *)
      usage
      exit 64
      ;;
  esac
done

case "$regime" in
  legacy)
    opener=exo-open
    editor=exo-desktop-item-edit
    chooser=exo-icon-chooser
    ;;
  successor)
    opener=xfce-open
    editor=xfce-desktop-item-edit
    chooser=xfce-icon-chooser
    ;;
  *)
    usage
    exit 64
    ;;
esac

if [[ -n "$staged_root" && ! -d "$staged_root" ]]; then
  echo "staged root does not exist: $staged_root" >&2
  exit 66
fi
if [[ -n "$plugin" && ! -f "$plugin" ]]; then
  echo "installed plugin does not exist: $plugin" >&2
  exit 66
fi

work_dir="$(mktemp -d)"
trap 'rm -rf -- "$work_dir"' EXIT
capture_log="$work_dir/capture.log"
helper_dir="$work_dir/helpers"
mkdir -p "$helper_dir"
touch "$capture_log"

capture_script="$(cd "$(dirname "$0")" && pwd)/capture-helper.sh"
for helper in exo-open xfce-open exo-desktop-item-edit xfce-desktop-item-edit; do
  ln -s "$capture_script" "$helper_dir/$helper"
done

export PATH="$helper_dir:$PATH"
export MEOWMENU_CAPTURE_LOG="$capture_log"

hostile_path="$work_dir/Folder '\$value' \`literal\` (test) \\"
mkdir -p "$hostile_path"
launcher_uri="file:///tmp/Meow%20Menu%20%27safe%27.desktop"

# The chooser is an in-process API, so record its regime selection directly.
printf 'interaction=icon-chooser\tfacility=%s\tresult=selected\n' "$chooser"

"$opener" --launch WebBrowser "https://github.com/matteobonanomi/xfce4-meowmenu-plugin"
"$opener" --launch TerminalEmulator man "xfce4-popup-meowmenu"
"$opener" --launch WebBrowser "https://duckduckgo.com/?q=meow%20menu"
"$opener" --launch WebBrowser "https://en.wikipedia.org/wiki/Xfce"
"$opener" --launch TerminalEmulator "printf %s safely"
"$opener" "https://example.invalid/a%20path"
"$editor" "$launcher_uri"
"$opener" --launch FileManager "$hostile_path"
(
  cd "$hostile_path"
  "$opener" --launch TerminalEmulator --working-directory "$hostile_path"
)

expected_blocks=9
actual_blocks="$(grep -c '^BEGIN$' "$capture_log")"
if [[ "$actual_blocks" != "$expected_blocks" ]]; then
  echo "expected $expected_blocks captured helpers, found $actual_blocks" >&2
  exit 1
fi
if [[ "$regime" == successor ]] && grep -Eq '(^|=)exo-(open|desktop-item-edit)' "$capture_log"; then
  echo "successor smoke invoked an Exo helper" >&2
  exit 1
fi
grep -Fq "argv_0=$launcher_uri" "$capture_log"
grep -Fq "cwd=$(printf '%q' "$hostile_path")" "$capture_log"
grep -Fq "argv_3=$(printf '%q' "$hostile_path")" "$capture_log"

printf 'interaction_count=10\n'
printf 'regime=%s\n' "$regime"
printf 'helper_invocations=%s\n' "$actual_blocks"
printf 'result=pass\n'
cat "$capture_log"
