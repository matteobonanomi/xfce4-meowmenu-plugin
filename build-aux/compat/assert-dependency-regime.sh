#!/usr/bin/env bash
#
# Shared source/package evidence assertions. The script fails closed when the
# selected dependency regime or the Meson test inventory is inconsistent.

set -euo pipefail

usage() {
  echo "usage: $0 --regime legacy|successor [--plugin FILE] [--testlog FILE] [--summary FILE]" >&2
}

regime=
plugin=
testlog=
summary="${GITHUB_STEP_SUMMARY:-}"
while (($#)); do
  case "$1" in
    --regime) regime="${2:-}"; shift 2 ;;
    --plugin) plugin="${2:-}"; shift 2 ;;
    --testlog) testlog="${2:-}"; shift 2 ;;
    --summary) summary="${2:-}"; shift 2 ;;
    *) usage; exit 64 ;;
  esac
done
if [[ "$regime" != legacy && "$regime" != successor ]]; then
  usage
  exit 64
fi

failures=0
record() {
  printf '%s=%s\n' "$1" "$2"
}
expect_present() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "required helper is absent: $1" >&2
    failures=$((failures + 1))
  fi
}
expect_absent() {
  if command -v "$1" >/dev/null 2>&1; then
    echo "forbidden helper is present: $1" >&2
    failures=$((failures + 1))
  fi
}

if [[ "$regime" == legacy ]]; then
  pkg-config --exists exo-2 || failures=$((failures + 1))
  expect_present exo-open
  expect_present exo-desktop-item-edit
else
  if pkg-config --exists exo-2; then
    echo "successor environment exposes exo-2.pc" >&2
    failures=$((failures + 1))
  fi
  expect_absent exo-open
  expect_absent exo-desktop-item-edit
  expect_present xfce-open
  expect_present xfce-desktop-item-edit
fi

if [[ -n "$plugin" ]]; then
  if [[ ! -f "$plugin" ]]; then
    echo "plugin not found: $plugin" >&2
    failures=$((failures + 1))
  elif [[ "$regime" == successor ]]; then
    if readelf -d "$plugin" | grep -Eiq 'NEEDED.*exo'; then
      echo "successor plugin links Exo" >&2
      failures=$((failures + 1))
    fi
    if nm -D --undefined-only "$plugin" | grep -Eiq '(^|[[:space:]])exo_'; then
      echo "successor plugin has unresolved Exo symbols" >&2
      failures=$((failures + 1))
    fi
  fi
fi

configured=0
disabled=0
executed=0
skipped=0
failed=0
timed_out=0
if [[ -n "$testlog" ]]; then
  if [[ ! -f "$testlog" ]]; then
    echo "Meson test log not found: $testlog" >&2
    failures=$((failures + 1))
  else
    passed="$(awk '$1 == "Ok:" {value=$2} END {print value+0}' "$testlog")"
    skipped="$(awk '$1 == "Skipped:" {value=$2} END {print value+0}' "$testlog")"
    failed="$(awk '$1 == "Fail:" {value=$2} END {print value+0}' "$testlog")"
    timed_out="$(awk '$1 == "Timeout:" {value=$2} END {print value+0}' "$testlog")"
    disabled="${MEOWMENU_DISABLED_TESTS:-0}"
    executed=$((passed + skipped + failed + timed_out))
    configured=$((executed + disabled))
    if ((configured == 0)); then
      echo "Meson test summary is missing from $testlog" >&2
      failures=$((failures + 1))
    fi
    if ((failed > 0 || timed_out > 0)); then
      failures=$((failures + 1))
    fi
  fi
fi

evidence="$(
  record regime "$regime"
  record configured "$configured"
  record disabled "$disabled"
  record executed "$executed"
  record skipped "$skipped"
  record failed "$failed"
  record timed_out "$timed_out"
  record outcome "$([[ "$failures" == 0 ]] && echo pass || echo fail)"
)"
printf '%s\n' "$evidence"
if [[ -n "$summary" ]]; then
  {
    printf '### Dependency regime evidence\n\n'
    printf '```text\n%s\n```\n' "$evidence"
  } >>"$summary"
fi
exit "$([[ "$failures" == 0 ]] && echo 0 || echo 1)"
