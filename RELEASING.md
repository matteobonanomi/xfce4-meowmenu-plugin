# Releasing MeowMenu

End-to-end checklist for cutting a new release. Follow it top to bottom.

---

## Prerequisites

- **Python ≥ 3.6** — runs helper scripts.
- **rsvg-convert** (preferred) or **inkscape** (fallback) — only needed if you
  re-render icons. Install with `sudo apt install librsvg2-bin`.
- **gh** — GitHub CLI. Install with `sudo apt install gh`, then
  `gh auth login`.
- Write access to `matteobonanomi/xfce4-meowmenu-plugin` on GitHub.

---

## Release steps

### 1. Promote `development` → `release` → `main`

MeowMenu uses a two-hop release flow gated by CI:

1. **`development` → `release`**: open a PR from `development` (or any feature
   branch) into `release`. Heavy CI (`ci.yml`) runs the six-cell distro
   matrix plus `sanitizers`, `translations`, and `no-optional-deps`. All
   nine checks must be green to merge.
2. **`release` → `main`**: open a PR from `release` into `main`. Light verify
   (`ci-main.yml`, the single `verify (ubuntu-26.04)` job) must be green to
   merge.
3. **Tag on `main`**: the release tag must point at a commit reachable from
   `main`. The packaging workflow enforces this invariant and refuses to
   produce artifacts otherwise (see [`docs/ci.md`](docs/ci.md) and
   [`contracts/workflow-jobs.md` §5](.specify/specs/009-ci-foundation/contracts/workflow-jobs.md#5-tag-on-main-invariant-runtime-contract)).

```bash
# After the release PR is merged and main is up to date:
git checkout main
git pull origin main
```

Post-release: load the plugin in an Xfce panel session and confirm it
launches, opens, and runs a search end-to-end. CI does not exercise the
running plugin; this manual UI verification is the only check covering that
surface.

### 2. Edit `NEWS`

Prepend a new top entry:

```text
0.4.0 (2026-06-01)
=====
- one bullet per user-visible change, present tense
```

The header **must** match `X.Y.Z (YYYY-MM-DD)` exactly — `tools/news-version.py`
parses this format. Only edit `NEWS` when you are ready to tag.

### 3. Build and verify locally

```bash
./dev-reload.sh
```

> **What `dev-reload.sh` does (and does not do)**
>
> This script is a **local development tool only**. It reconfigures Meson
> (which re-reads `NEWS` and regenerates `panel-plugin/version.h`), compiles,
> installs, and hot-reloads the plugin in your running Xfce session. It does
> **not** update GitHub, the README badge, or create any release.
>
> Pass `--icons` to also re-render `icons/hi*-app-meowmenu.png` from
> `assets/meowmenu.svg` before building. This requires `rsvg-convert` or
> `inkscape`. Without `--icons`, the committed PNGs are used as-is — you only
> need this flag when the master SVG has changed.
>
> ```bash
> ./dev-reload.sh --icons   # regenerate icons, then build + reload
> ./dev-reload.sh           # build + reload only (normal case)
> ```

Open the plugin's *About* dialog and confirm the version and date are correct.

### 4. Run the test suite

```bash
meson test -C build
```

The `version-consistency` test must pass. If it fails, the `NEWS` top entry
does not match `meson.project_version()`.

### 5. Commit and push

```bash
git add NEWS
git commit -m "release: 0.4.0"
git push origin main
```

### 6. Tag and push the tag

```bash
git tag v0.4.0
git push origin v0.4.0
```

Pushing the tag triggers `.github/workflows/release.yml`, which:

1. Extracts the matching `NEWS` section.
2. Creates a GitHub Release with that content.
3. Automatically updates the `[![Version](...)]` badge in the README — the
   badge is dynamic (shields.io GitHub releases API) and picks up the new
   latest release within a few minutes.

### 7. Verify on GitHub

Open the *Releases* page and confirm the new release exists with the correct
body. The README version badge should update shortly after.

---

## Why the README badge shows an old version

The badge reads the **latest published GitHub Release**, not the `NEWS` file or
any local build. It will only update after you push a tag and the release
workflow completes successfully. Running `dev-reload.sh` has no effect on it.

---

## Tooling reference

### `tools/news-version.py`

Parses the top entry of `NEWS`.

| Flag | Effect |
|------|--------|
| `--version` | print the version string only |
| `--date` | print the release date only |
| `--check EXPECTED` | exit non-zero if NEWS version ≠ `EXPECTED` |
| `--news PATH` | use a non-default NEWS path (auto-discovered otherwise) |

Used by Meson at configure time and by the `version-consistency` test.

### `tools/regen-icons.py`

Renders `assets/meowmenu.svg` to every PNG size in `icons/`. Invoked
automatically by `dev-reload.sh --icons`, or manually:

```bash
python3 tools/regen-icons.py --input assets/meowmenu.svg --output-dir icons/
```

You can also call it via Meson:

```bash
meson compile -C build regen-icons
```

End-user builds do not require a renderer because the PNGs are committed.

---

## Tag format

Tags **must** be `v<version>` (e.g. `v0.4.0`, `v0.4.0-rc1`). The `release.yml`
workflow only triggers on `v*` tags and strips the leading `v` when locating
the matching `NEWS` section.

---

## Branch protection

The two release-track branches (`release` and `main`) are protected on
github.com. The full prose reproduction of the settings lives in
[`docs/ci.md`](docs/ci.md); this section is a deliberate duplicate so the
required-check sets can be reconstructed even if `docs/ci.md` is ever moved
(FR-021).

**Required checks on `release`** (all must be green to merge):

- `build (ubuntu-26.04, debugoptimized)`
- `build (ubuntu-26.04, release)`
- `build (debian-13, debugoptimized)`
- `build (debian-13, release)`
- `build (fedora-44, debugoptimized)`
- `build (fedora-44, release)`
- `sanitizers`
- `translations`
- `no-optional-deps`

`analyze (cpp)` is advisory only — it MUST NOT be added to this list.

**Required checks on `main`** (must be green to merge):

- `verify (ubuntu-26.04)`

**Other protection rules** (both branches): force-push disabled; deletion
disabled; rules apply to administrators. See
[`docs/ci.md`](docs/ci.md) for the full reproduction and the rename protocol
(FR-023).
