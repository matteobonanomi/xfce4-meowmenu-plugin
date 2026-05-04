<!--
SYNC IMPACT REPORT
==================
Version change: TEMPLATE (uninitialized) → 1.0.0
Bump rationale: First ratified constitution for the Whisker Menu fork. All
  placeholder tokens replaced with concrete content; principles, additional
  constraints, workflow, and governance fully defined.

Modified principles (placeholder → final):
  [PRINCIPLE_1_NAME] → I. Spec-First Development (NON-NEGOTIABLE)
  [PRINCIPLE_2_NAME] → II. Small Reviewable Patches
  [PRINCIPLE_3_NAME] → III. Native Stack Only
  [PRINCIPLE_4_NAME] → IV. Xfconf as Single Source of Truth
  [PRINCIPLE_5_NAME] → V. X11 Primary, Wayland Graceful Fallback
  (added)            → VI. Garcon for Application Discovery
  (added)            → VII. Optional Dependencies Degrade Cleanly

Added sections:
  - Additional Constraints (stack guardrails, scope guardrails)
  - Development Workflow & Quality Gates
  - Governance (amendment, versioning, compliance)
  - Out-of-Scope (non-goals)

Removed sections: none (template skeleton fully replaced).

Templates requiring follow-up alignment:
  ⚠ pending  .specify/templates/plan-template.md
             "Constitution Check" gate is currently a placeholder
             ("[Gates determined based on constitution file]"). Should be
             replaced with concrete checks derived from Principles I–VII
             (e.g. "Spec → Plan → Tasks present", "No Python in production
             paths", "Xfconf is the only persistence", "Wayland fallback
             classified", etc.). Not modified in this pass per user
             instruction "Non implementare nulla".
  ⚠ pending  .specify/templates/spec-template.md
             Add a mandatory "Platform Classification" field per Principle V
             (X11+Wayland parity / X11-first with Wayland fallback /
             unavailable on Wayland for now).
  ⚠ pending  .specify/templates/tasks-template.md
             Confirm task categories surface translation (po/), Xfconf
             schema migration, and X11/Wayland verification when relevant.
  ✅ noted   docs/architecture/architecture-map.md (worktree) reflects the
             same constraints; no changes required.
  ✅ noted   CLAUDE.md §3 mirrors these principles; no drift.

Deferred TODOs: none. RATIFICATION_DATE set to today (2026-05-04) since this
  is the original adoption.
-->

# xfce4-meowmenu-plugin Constitution

## Core Principles

### I. Spec-First Development (NON-NEGOTIABLE)

No production code is written before an approved spec, plan, and task list
exist for the work in question. The mandated pipeline is
`/speckit-constitution` → `/speckit-specify` → (optional `/speckit-clarify`) →
`/speckit-plan` → `/speckit-tasks` → (optional `/speckit-analyze`) →
`/speckit-implement`. Until a human has explicitly approved the spec, plan,
and tasks for a feature, the agent operates read-only on production paths
(`panel-plugin/`, `meson.build`, `po/`, `icons/`).

**Rationale**: This fork must evolve a launcher used in a stable desktop
environment without regressing the upstream Whisker Menu. Spec-driven work
forces clarity on user-visible behavior, prevents speculative refactors, and
keeps every change reviewable. Drive-by edits and "just fix it" shortcuts
are explicitly forbidden because they have historically produced
regressions in this codebase's domain (panel integration, Xfconf schema,
i18n).

### II. Small Reviewable Patches

Each task in `tasks.md` MUST map to a single pull request. Each pull
request MUST be self-contained: it compiles, its tests pass, and it does
not rely on a sibling PR landing first. Branch naming MUST follow
`feature/NNN-short-slug`, where `NNN` matches the Spec-Kit feature folder
under `.specify/specs/`. Mass-reformatting, drive-by renames, and unrelated
refactors MUST NOT be bundled with a behavioral change.

**Rationale**: Whisker is a long-lived plugin loaded into a host process
(`xfce4-panel`); a single bad commit can break every Xfce panel running
the plugin. Small, focused patches make bisecting trivial and let
translators, packagers, and reviewers verify scope at a glance. One
feature per branch keeps the Spec-Kit folder and the git history aligned.

### III. Native Stack Only

The plugin is C++ (gnu++11), GTK 3, Meson, and the Xfce native libraries
(`garcon`, `libxfce4panel`, `libxfce4ui`, `libxfce4util`, `xfconf`). The
following are PROHIBITED in production code:

- Python rewrites of any production module (the plugin shared object, the
  `xfce4-popup-whiskermenu` binary, the preferences dialog, or any subset
  of them). Python is acceptable ONLY for build/release helper scripts.
- Web frontends in any form: HTML rendering, JavaScript runtimes,
  WebKitGTK panes, Electron-style layers, embedded browsers.
- Hard dependency on GTK 4. A future migration may be planned in its own
  spec; it is out of scope for the current fork.
- Mandatory background daemons or always-on indexing services. Optional,
  lazily-loaded providers are permitted only if the plugin remains fully
  functional when they are absent.

**Rationale**: Xfce's identity is lightness, predictability, and native
integration. A Python or web layer would inflate startup, complicate
packaging on Xubuntu 26.04, fragment the bug surface across runtimes, and
break Xfce's principle of GUI-first configuration. GTK 4 is excluded
because it would force every downstream distro to ship a parallel toolkit
just for one panel plugin.

### IV. Xfconf as Single Source of Truth

User-visible configuration MUST live in Xfconf, under namespaces declared
in the active spec (see `docs/whisker-modernization-spec.md` §13.2). It is
forbidden to introduce a parallel configuration store: no SQLite database,
no JSON files in `~/.config/whisker-fork/`, no dotfiles, no environment
variables to carry user state, no XDG `state` blobs. Every new
user-facing setting MUST:

1. Live under one of the documented Xfconf namespaces.
2. Be reachable from the GUI preferences dialog (no terminal-only
   settings).
3. Have a safe default that produces correct behavior on a fresh install.
4. Support reset-to-default from the GUI.
5. Be covered by the schema migration story (spec §13.3): existing users
   MUST end up with equivalent or better defaults after upgrade.

**Rationale**: The upstream Whisker plugin already speaks Xfconf
end-to-end via the typed wrappers in `panel-plugin/settings.{h,cpp}`. A
parallel store would split the configuration surface, break
`xfce4-settings-manager` introspection, and prevent users from migrating
between machines via standard Xfce profile sync. One persistence layer is
also auditable, which is what the schema migration story relies on.

### V. X11 Primary, Wayland Graceful Fallback

X11 is the **quality path**: every feature MUST work fully on Xubuntu
26.04 / Xfce 4.20 X11 sessions before it ships. Wayland is **supported
with graceful fallback**: every feature spec MUST classify itself as one
of:

- `X11+Wayland parity` — feature works identically on both;
- `X11-first with Wayland fallback` — degraded but functional on Wayland;
- `unavailable on Wayland for now` — explicitly disabled, with a clear UI
  hint when the user is on Wayland.

A feature that crashes, hangs, or silently misbehaves on Wayland is a
release blocker, regardless of its classification. The existing dual-path
pattern (`#ifdef HAVE_GTK_LAYER_SHELL` + runtime `gtk_layer_is_supported()`
in `panel-plugin/window.cpp`) is the reference example.

**Rationale**: Xubuntu 26.04 ships X11 by default, but Wayland is the
direction of the broader ecosystem. We commit to never letting Whisker
become the reason a user cannot switch sessions. Explicit classification
in every spec prevents silent regressions and makes review of the
session-mode matrix mechanical.

### VI. Garcon for Application Discovery

The application catalog MUST be sourced from `garcon` (freedesktop menu
specification) and from the user's `xfce-settings-manager.menu`. It is
forbidden to bypass garcon with a custom `.desktop` scanner, an
ad-hoc `xdg-data-dirs` walker, or a hard-coded application list. Custom
menu files are permitted only via the existing `custom_menu_file`
preference, which still flows through `garcon_menu_new_for_path`.

**Rationale**: Garcon is the single integration point that respects
NoDisplay, OnlyShowIn, TryExec, locale fallbacks, and freedesktop semantic
versioning. Reinventing this would silently break enterprise lockdowns,
multi-user systems, kiosk deployments, and the entire Xfce menu-editing
toolchain (`menulibre`, `alacarte`, `xfce4-settings`).

### VII. Optional Dependencies Degrade Cleanly

`accountsservice` and `gtk-layer-shell` are declared optional in
`meson.build` and gated by the `HAVE_ACCOUNTS_SERVICE` and
`HAVE_GTK_LAYER_SHELL` preprocessor flags. Any new optional dependency
MUST follow the same pattern:

1. Declared as optional in `meson_options.txt`.
2. Compiled only when present.
3. Wrapped in a feature flag macro.
4. The plugin MUST remain fully functional with the dependency absent
   (with a documented graceful degradation, e.g. fallback avatar, X11-only
   path).
5. CI MUST exercise both the with-dependency and without-dependency build.

**Rationale**: Distros ship a long tail of Xfce flavors; a hard-required
dependency that some downstream packagers cannot satisfy effectively
forks the plugin out of those distros. Mandatory degradation paths also
make the plugin auditable for the constrained-environment use cases
(Raspberry Pi, kiosks, server boxes with a panel).

## Additional Constraints

### Stack guardrails

- C++: match the existing patterns in `panel-plugin/` (header guards,
  naming, indentation, raw signal binding via `panel-plugin/slot.h`). No
  wholesale reformatter passes.
- Translatable strings: every user-visible string MUST go through the
  existing gettext macros and have a corresponding `po/` update.
  Non-translatable user-facing strings are forbidden.
- Public-facing identifiers and code comments are in English.
  `.specify/` and `docs/` may be Italian or English — match the
  surrounding document's language.
- Commit messages: conventional, present tense, referencing the
  Spec-Kit feature folder. Example:
  `search: add typo-tolerant matcher (.specify/specs/001-search-ranking)`.

### Scope guardrails

The fork MUST NOT drift toward becoming any of the following:

- A full-screen GNOME-Shell-style shell replacement.
- A replacement for the Xfce settings system or `xfce4-settings-manager`.
- A distro-specific launcher: distro-specific behavior MUST be expressed
  as modular providers, never hard-coded into core paths.
- A launcher whose any user-facing capability requires opening a
  terminal to configure or use.

## Development Workflow & Quality Gates

### Workflow order (non-negotiable)

1. **Constitution** (`/speckit-constitution`) — this document. Updated only
   when project principles genuinely change.
2. **Specify** (`/speckit-specify`) — one per feature/milestone.
   User-visible behavior, requirements, acceptance criteria. No
   implementation choices.
3. **Clarify** (`/speckit-clarify`) — when the spec has ambiguities, before
   planning.
4. **Plan** (`/speckit-plan`) — translates the spec into an architectural
   plan, citing real files and modules. MUST respect this constitution.
5. **Tasks** (`/speckit-tasks`) — ordered, checkable, PR-sized tasks.
6. **Analyze** (`/speckit-analyze`) — optional cross-check for drift.
7. **Implement** (`/speckit-implement`) — only after explicit human approval
   of spec, plan, and tasks in chat.

### Build and test gates

Standard Meson workflow (see `CLAUDE.md` §5). A task is "done" only when
all three of the following have run successfully and been reported:

```
meson setup build
meson compile -C build
meson test -C build
```

If `meson test` reports zero tests for a touched area, the task MUST flag
this as a finding and propose a test-addition task in the relevant
`tasks.md`.

For UI changes, the agent MUST NOT claim success without a manual
verification path written into the PR (a nested Xfce session or
disposable Xubuntu VM is required for graphical verification; the agent
does not launch graphical sessions itself).

### Default mode

Outside of an authorized `/speckit-implement` execution, every request is
treated as **read-only** on production paths. Requests of the form "just
fix this" or "go ahead and implement" MUST be answered with: "This needs
a spec → plan → tasks first. Want me to run `/speckit-specify` for it?" —
and stop.

## Out-of-Scope (Non-Goals)

The following are explicit non-goals for this fork. A request that
implies any of them is a Constitution violation and MUST be surfaced and
confirmed before any work proceeds:

- Replacing or shadowing the Xfce panel itself.
- Becoming a desktop shell, application dock, window manager, or
  notification center.
- Mandatory cloud connectivity for any user-facing feature.
- Telemetry that leaves the user's machine.
- A configuration UX that requires editing files by hand.

## Governance

### Authority

This constitution supersedes all other practices, including upstream
conventions, when there is a conflict on this fork. Where the
constitution is silent, upstream Whisker Menu conventions apply.
Where both are silent, follow the closest existing pattern in
`panel-plugin/`.

### Compliance review

Every PR MUST verify compliance with this constitution. The plan
template's "Constitution Check" gate is the canonical enforcement point;
the reviewer MUST cite the principle(s) checked. Any complexity that
appears to violate a principle MUST be justified in the plan's
"Complexity Tracking" section, with a reasoned argument and a sunset
condition.

### Amendment procedure

Amendments are made by re-running `/speckit-constitution`. Each
amendment MUST:

1. Document the change in the Sync Impact Report at the top of this file
   (HTML comment).
2. Bump the version field at the bottom according to semantic versioning:
   - **MAJOR** — backward-incompatible governance or principle removal /
     redefinition.
   - **MINOR** — new principle, new section, or materially expanded
     guidance.
   - **PATCH** — wording, typo, or non-semantic clarification.
3. Update the `Last Amended` date.
4. Propagate any required changes to the Spec-Kit templates
   (`plan-template.md`, `spec-template.md`, `tasks-template.md`) in the
   same change set.

### Runtime guidance

Runtime "how to work in this repo" guidance lives in `CLAUDE.md`. When
`CLAUDE.md` and this constitution disagree, **this constitution wins**
and `CLAUDE.md` MUST be updated to match.

**Version**: 1.0.0 | **Ratified**: 2026-05-04 | **Last Amended**: 2026-05-04
