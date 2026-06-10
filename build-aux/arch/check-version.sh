#!/usr/bin/env bash
#
# check-version.sh: on a release-tag run, fail unless the committed
# dist/arch/PKGBUILD pkgver equals the pushed tag version (refs/tags/vX.Y.Z ->
# X.Y.Z). On any non-tag run (manual dispatch / pre-release validation) the
# check is skipped, because no published release exists yet and the CI-only
# source patch already builds the current checkout.
#
# This is tag-driven, mirroring the .deb/.rpm jobs, which stamp their version
# only at release time rather than from any committed recipe.
#
# NOTE: NEWS is intentionally NOT consulted here. NEWS remains the source of
# truth for the .deb/.rpm `validate-tag` job, but the Arch recipe carries a
# concrete pkgver tied to an already-published tarball + checksum (FR-006).
# Keying this check off the tag ref avoids a spurious failure during the window
# after a NEWS entry is dated but before its release tag is pushed, when the
# committed recipe still legitimately tracks the prior released version.
#
# Trigger context is read from the environment (provided by the workflow step):
#   GITHUB_EVENT_NAME - the Actions event name; "push" for a tag push.
#   GITHUB_REF_NAME   - the short ref name; "vX.Y.Z" on a tag push.
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

event_name="${GITHUB_EVENT_NAME:-}"
ref_name="${GITHUB_REF_NAME:-}"

# Only a tag push carries a published release to validate against. Anything
# else (workflow_dispatch, or a push whose ref is not a vX.Y.Z tag) has no
# release identity yet, so there is nothing to compare the committed pkgver to.
if [ "${event_name}" != "push" ] || [[ "${ref_name}" != v[0-9]* ]]; then
  echo "skipping version-equality check (no release tag; event='${event_name}' ref='${ref_name}')"
  exit 0
fi

# Tag run: the expected version is the tag with its leading 'v' stripped.
expected="${ref_name#v}"

# Source the committed recipe in a subshell to read its pkgver without running
# any build logic. The recipe path is not a constant, so silence shellcheck's
# follow warning.
# shellcheck disable=SC1090,SC1091,SC2154
actual="$(source "${REPO_ROOT}/dist/arch/PKGBUILD"; printf '%s' "${pkgver}")"

if [ "${expected}" != "${actual}" ]; then
  echo "::error::PKGBUILD pkgver '${actual}' does not match release tag version '${expected}'"
  exit 1
fi

echo "pkgver '${actual}' matches release tag version"
