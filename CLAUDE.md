# CLAUDE.md

Persistent context for Claude Code working on this repository.
Read this file at the start of every session, before touching any code.

\---

## 1\. Project overview

This repository is a **fork of `xfce4-whiskermenu-plugin`**, the Whisker Menu
launcher for Xfce panels. The official name of the fork is **MeowMenu**. 

* **Target platform:** Xubuntu 26.04 with Xfce 4.20.x
* **Primary session:** X11 (quality path)
* **Secondary session:** Wayland (supported, with documented fallback)
* **Goal of the fork:** evolve Whisker into a modern, more discoverable, deeply
configurable launcher — without turning it into an alternative shell and
without breaking Xfce's principles (lightness, predictability, native
integration, GUI-first configuration).

The guiding product rule:

> \*\*Classic by default, modern when desired, advanced only when requested.\*\*

The full functional and technical specification lives in
`docs/whisker-modernization-spec.md`. That document is the source of truth for
*what* to build. This file (`CLAUDE.md`) is the source of truth for *how* to
work in the repo.

\---

## 2\. Development workflow: Spec-Kit + Claude Code

This project uses **GitHub Spec-Kit** for spec-driven development. The workflow
is non-negotiable: **no production code is written before a constitution, spec,
plan, and tasks exist for the work in question.**

### 2.1 Spec-Kit slash commands

Inside Claude Code, the following slash commands are available (provided by
`specify init`):

|Command|Purpose|
|-|-|
|`/speckit.constitution`|Create or update the project constitution (principles).|
|`/speckit.specify`|Create a spec describing **what** and **why**.|
|`/speckit.clarify`|(Optional) Ask structured clarifying questions on the spec.|
|`/speckit.plan`|Produce a technical plan respecting this CLAUDE.md.|
|`/speckit.tasks`|Break the plan into small, reviewable, ordered tasks.|
|`/speckit.analyze`|(Optional) Cross-check spec, plan, and tasks for drift.|
|`/speckit.implement`|Execute the tasks. **Only after explicit approval.**|

Artifacts produced by these commands live under `.specify/` and the
per-feature folders that Spec-Kit creates. Do not move them, do not rename
them, do not hand-edit them in ways that bypass the slash commands.

### 2.2 Strict workflow order

1. **Constitution** (`/speckit.constitution`) — once for the project. Mirrors
§3 of this file. Update only when project principles genuinely change.
2. **Specify** (`/speckit.specify`) — one per feature/milestone. Focus on
user-visible behavior, requirements, acceptance criteria. **No tech stack
details, no implementation choices.**
3. **Clarify** (`/speckit.clarify`) — if the spec has ambiguities, run this
before planning.
4. **Plan** (`/speckit.plan`) — translates the spec into an architectural
plan, citing real files and modules in this codebase. Must respect the
constraints in §3 and §4 of this file.
5. **Tasks** (`/speckit.tasks`) — produces an ordered, checkable list of small
PR-sized tasks.
6. **Analyze** (`/speckit.analyze`) — optional but recommended sanity check.
7. **Implement** (`/speckit.implement`) — only after a human has explicitly
approved the spec, plan, and tasks. Until that approval lands in chat,
Claude does not write or modify production code.

### 2.3 Default mode: read-only analysis

Unless the user has just run a slash command that authorizes writes, treat
every request as **read-only**. If a user asks for something that would
require writing code or files outside `.specify/`, surface the conflict and
suggest the matching slash command instead.

Concretely, if asked to "just fix this" or "go ahead and implement", reply
with: "This needs a spec → plan → tasks first. Want me to run
`/speckit.specify` for it?" — and stop.

\---

## 3\. Project constitution (hard constraints)

These are the principles the constitution must enforce. Claude must refuse
to violate them even if a user request seems to ask for it; in that case,
stop and confirm.

### 3.1 Process principles

* **Spec-first.** No code without an approved spec, plan, and tasks.
* **Small reviewable patches.** Each task in `tasks.md` should map to one PR.
* **One feature per branch.** Branch naming: `feature/NNN-short-slug`,
matching the Spec-Kit feature folder.

### 3.2 Stack principles

* **Native stack only:** C++, GTK 3, Meson, Xfce native libraries
(`garcon`, `libxfce4panel`, `libxfce4ui`, `libxfce4util`, `xfconf`).
* **No Python rewrites.** Not the plugin, not the prefs dialog, not parts of
it. Helper build scripts in Python are acceptable.
* **No web frontends.** No HTML, no JavaScript, no Electron-style layers.
* **No GTK 4 prerequisite.** A future migration may be planned separately;
it is not in scope here.
* **No mandatory background daemon.** No mandatory desktop indexer.
Optional, lazily-loaded providers are fine.

### 3.3 Source-of-truth principles

* **Applications and categories:** Garcon (freedesktop menu spec). Do not
bypass it with custom `.desktop` scanners.
* **User configuration:** Xfconf. Do not introduce a parallel config store
(no SQLite, no JSON in `\~/.config/whisker-fork/`, no dotfiles).
* Every new user-facing setting must:

  1. live under one of the Xfconf namespaces in spec §13.2;
  2. be reachable from the GUI preferences;
  3. have a safe default;
  4. support reset-to-default;
  5. be covered by the schema migration story (spec §13.3).

### 3.4 Platform principles

* **X11 is the primary quality path.** Every feature must work well here.
* **Wayland is supported with graceful fallback.** Each feature must be
classified in its spec as one of:
`X11+Wayland parity` / `X11-first with Wayland fallback` /
`unavailable on Wayland for now`.
* Optional dependencies (`AccountsService`, `gtk-layer-shell`) stay optional.
Features must degrade cleanly when they are absent.

### 3.5 Out of scope (non-goals)

* Becoming a full-screen GNOME-Shell-style replacement.
* Replacing the Xfce settings system.
* Distro-specific behavior in core (distro hooks must be modular providers).
* Any user-facing capability that requires a terminal to use.

\---

## 4\. Repository layout

```
/                         # upstream Whisker source tree (C++ / Meson)
  meson.build
  panel-plugin/           # Xfce panel plugin glue
  panel-plugin/                    # plugin sources (search, prefs, UI, etc.)
  icons/, po/             # assets and translations
/.specify/                # Spec-Kit workspace (managed by slash commands)
  memory/
    constitution.md       # produced by /speckit.constitution
  specs/
    NNN-feature-slug/
      spec.md             # /speckit.specify
      plan.md             # /speckit.plan
      tasks.md            # /speckit.tasks
/docs/
  whisker-modernization-spec.md   # full product/tech spec (source of truth for WHAT)
  architecture/                   # internal architecture notes
  ux/                             # UX notes and decisions
CLAUDE.md                 # this file (source of truth for HOW)
```

When investigating the codebase, start from `meson.build` and `panel-plugin/`
to understand how the plugin registers with Xfce,  `panel-plugin/` is the actual former whisker menu code base.

\---

## 5\. Build, run, and test

Standard Meson workflow. Always run from the repo root.

```bash
# Configure (only needed once, or after meson.build changes)
meson setup build

# Compile
meson compile -C build

# Run the test suite
meson test -C build

# Clean rebuild
rm -rf build \&\& meson setup build \&\& meson compile -C build
```

Before declaring a task done, Claude must run **all three** of `setup`,
`compile`, and `test` and report the results. If `meson test` reports zero
tests for a touched area, flag it as a finding and propose adding tests in
the relevant `tasks.md`.

For interactive UI testing, a nested Xfce session or a disposable Xubuntu VM
is required. Claude does not launch graphical sessions itself; if a manual
check is needed, write the exact reproduction steps for the human in the PR.

\---

## 6\. First-session bootstrapping

The very first interactions with a freshly-forked repo follow this order.
**Do not skip steps. Do not modify code at any of these steps.**

1. **Architectural read.** Without writing anything, produce a map of the
plugin: main files, menu-open flow, search flow, settings handling,
Xfce/GTK dependencies, and likely extension points. Save the result under
`docs/architecture/` only after the human approves it.
2. **Constitution.** Run `/speckit.constitution` to formalize the principles
in §3 of this file into `.specify/memory/constitution.md`.
3. **First milestone spec.** Run `/speckit.specify` for
*"Search Ranking 2.0 foundation for Whisker Menu on Xubuntu 26.04 /
Xfce 4.20"*. Focus on requirements, not implementation.
4. **Plan.** Run `/speckit.plan`. The plan must cite real files in this
codebase and respect this CLAUDE.md.
5. **Tasks.** Run `/speckit.tasks`. Small, verifiable, independently
reviewable.
6. **Stop.** Wait for human approval before any `/speckit.implement` call.

If any earlier step has not happened yet, the next allowed action is that
step — not an implementation shortcut.

\---

## 7\. Code style and conventions

* Match the existing upstream style. Do not run a wholesale reformatter.
* C++: follow patterns already in `src/` (header guards, naming, indentation).
When in doubt, mimic the closest existing file.
* New translatable strings: wrap with the existing gettext macros and update
`po/` accordingly. Never introduce non-translatable user-facing strings.
* Comments and identifiers: English. Documents under `.specify/` and `docs/`
may be Italian or English — match the surrounding document.
* Commit messages: conventional, present tense, reference the Spec-Kit
feature folder. Example:
`search: add typo-tolerant matcher (.specify/specs/001-search-ranking)`.

\---

## 8\. What to do when uncertain

* If a request conflicts with §3, **stop and surface the conflict.** Do not
improvise a workaround.
* If a spec is silent on a design decision, propose 2–3 options with
trade-offs in `plan.md` and wait for a choice.
* If a build or test fails in a way Claude doesn't fully understand, **do not
paper over it** with a `try/catch`, a disabled test, or a skipped check.
Report the failure verbatim and propose a real fix.
* Never edit `.git/` directly. Never force-push. Never modify files outside
the workspace.
* If the user asks for something that should go through a slash command but
hasn't, recommend the slash command and stop.

\---

## 9\. Quick reference

|Need|Where|
|-|-|
|Product vision \& full spec|`docs/whisker-modernization-spec.md`|
|Active constitution|`.specify/memory/constitution.md`|
|Active feature specs|`.specify/specs/NNN-\*/`|
|Phase 1 features|Spec §9 (Search Ranking 2.0 → Recent Hub 2.0)|
|Phase 2 features|Spec §9.6–9.10 plus parts of §10|
|Phase 3 (signature features)|Spec §10 (Provider search, Runner, Dashboard)|
|Xfconf namespaces|Spec §13.2|
|Test matrix (X11/Wayland)|Spec §17|
|Performance budget|Spec §14|

\---

## 10\. Active Spec-Kit work

The block above is maintained by the `/speckit-plan` skill. Do not hand-edit
its contents outside the markers; the skill rewrites the body each time a new
plan is generated.



