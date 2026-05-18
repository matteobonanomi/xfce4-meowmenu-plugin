# Releasing MeowMenu

This document is the end-to-end checklist for cutting a new MeowMenu release.
A maintainer with no prior context should be able to follow it top to bottom
without having to read any other file.

## 1. Prerequisites

Install on your workstation:

- **Python ≥ 3.6** — runs the build-time helper scripts.
- **rsvg-convert** (preferred) or **inkscape** (fallback) — renders the master
  SVG to PNGs.
  - `sudo apt install librsvg2-bin`  *(preferred)*
  - `sudo apt install inkscape`      *(fallback)*
- **git** — version control.
- **gh** — GitHub CLI, used by the release workflow. Install via
  `sudo apt install gh` and authenticate with `gh auth login`.

You also need write access to the `matteobonanomi/xfce4-meowmenu-plugin`
repository on GitHub.

## 2. Release steps

1. **Decide the new version.** Use semver (`MAJOR.MINOR.PATCH`). Pre-release
   suffixes such as `-rc1` are allowed.

2. **Edit `NEWS`.** Prepend a new top entry on its own line:

   ```text
   0.4.0 (2026-06-01)
   =====
   - one bullet per user-visible change
   - keep the wording short and present tense
   ```

   The date must be `YYYY-MM-DD` and the header line must match
   `X.Y.Z (YYYY-MM-DD)` exactly — `tools/news-version.py` parses it.

3. **Rebuild and reload.** From the repo root:

   ```bash
   ./dev-reload.sh
   ```

   This re-runs `meson setup --reconfigure`, which re-reads `NEWS` via
   `tools/news-version.py` and regenerates `panel-plugin/version.h`. Open the
   plugin's *About* dialog and confirm the new version + date appear.

4. **Run the test suite.**

   ```bash
   meson test -C build
   ```

   The `version-consistency` test must pass. If it fails, your `NEWS` top
   entry does not match `meson.project_version()`.

5. **Commit and push.**

   ```bash
   git add NEWS
   git commit -m "release: <version>"
   git push origin <branch>
   ```

6. **Tag and push the tag.** Tags must be `v<version>`:

   ```bash
   git tag v0.4.0
   git push origin v0.4.0
   ```

   The push triggers `.github/workflows/release.yml`, which extracts the
   matching `NEWS` section and runs `gh release create`. If the workflow
   cannot find a `NEWS` section for the tag, it fails clearly.

7. **Verify on GitHub.** Open the *Releases* page and confirm the new release
   exists with the expected body.

## 3. Tooling reference

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

Renders `assets/meowmenu.svg` to every PNG size in `icons/`.

```bash
python3 tools/regen-icons.py --input assets/meowmenu.svg --output-dir icons/
```

Prefers `rsvg-convert`, falls back to `inkscape`, and fails with a clear
message if neither is installed. The Meson target `regen-icons` invokes the
same script when the maintainer asks for it explicitly:

```bash
meson compile -C build regen-icons
```

End-user builds do not require either renderer because the PNGs are
committed to the repository.

## 4. Tag format

Tags **must** be of the form `v<version>` — for example, `v0.4.0`,
`v0.4.0-rc1`. The `release.yml` workflow only triggers on tags matching
`v*`, and it strips the leading `v` when locating the matching `NEWS`
section.

## 5. PNG determinism note

`rsvg-convert` and `inkscape` produce PNGs that differ byte-for-byte even
when the source SVG is identical, because they embed renderer-specific
colour profile / metadata chunks. This is expected. Reviewers should look at
the rendered images, not the binary diff. If you re-render the PNGs as part
of a release, commit the result without concern about churn.
