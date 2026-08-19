---
layout: default
title: Testing
nav_order: 7
---

# Testing

Use a disposable profile or VM for upgrade tests. Do not publish private
contact details in evidence.

## Test context

Record these six fields before starting:

- **MeowMenu version/revision:** the exact release, prerelease, commit, or
  development revision under test.
- **Distribution/version:** the distribution name and version.
- **Xfce version:** the exact desktop version reported by the tester.
- **Architecture:** for example, `x86_64` or `amd64`.
- **Session type:** X11 or Wayland.
- **Installation method/artifact:** the package format, source build, or exact
  artifact identity.

Every result applies only to this recorded version and environment. It does
not establish results for another distribution, Xfce version, architecture,
session, or installation method.

## Five-minute core check

1. Install the version under test. Compare the installed version with the
   version shown in **About**, then restart the panel.
2. Add **MeowMenu** through **Add New Items**. On a fresh profile, confirm the
   Modern preset is selected.
3. Open the menu, search for a known application, and launch it.
4. Press Tab to switch Applications/Places, confirm Ctrl+Tab is a harmless
   no-op, use ordinary arrows to navigate, and press Enter to activate.
5. Close with Escape, reopen MeowMenu, and repeat an open/search/launch cycle.
6. Log out and in, then confirm the panel item, search, keyboard flow, and
   another startup still work.

A pass is scoped to the six recorded context fields above. File a
[compatibility report](https://github.com/matteobonanomi/xfce4-meowmenu-plugin/issues/new?template=compatibility-report.yml);
prefer a durable public issue or release note over a short-lived workflow link.

## Directional keyboard matrix

For release-quality manual coverage, use X11 with Xfce 4.20 on `x86_64` and
record the six context fields above. Exercise list and grid Results, Sidebar
on the left, right, top, bottom, and hidden, Search at both edges, visible and
hidden Session controls, Applications and Places, docked, centered, and
fullscreen layouts, and both LTR and RTL text directions. At every edge check
internal-first movement, one-move crossings, no-wrap no-ops, Search cursor and
Shift selection, Space during composition, context-menu priority, Escape,
Calculator-first anchoring, asynchronous Places focus, live layout changes,
and focus visuals.

Repeat a representative subset on Wayland after the X11 pass. Record focus
and geometry differences separately from compositor-delivered global shortcut
behavior; this subset is experimental evidence and does not establish X11
parity.

For every Horizontal-sidebar case whose categories fit, verify equal remaining
space on both sides of the category group within one device pixel. In Docked
and Centered, measure against the complete menu width; in Full Screen, measure
against the Results-width strip. Repeat with enough categories to overflow and
confirm that scrolling stays inside the same strip without widening or
displacing the menu. Apps/Places must remain in its derived non-strip home.

## Launcher surface and selector matrix

For release-quality visual evidence, test one compact, one ordinary, and one
spacious GTK theme in light and dark variants, plus a high-contrast theme.
Record display scale, text scale, compositor state, layout, primary-row edge,
sidebar position, Places state, and available Session actions with the six
context fields above.

In Docked and Centered, verify that Search and Results share the content
surface while visible category navigation and secondary controls share the
chrome surface. Check Left, Right, Horizontal, and disabled sidebars with the
primary row at both Top and Bottom. Every outer inset and major-region gap
must use one consistent theme-responsive rhythm within one device pixel;
hidden regions must leave neither an empty patch nor a doubled or orphaned
gap. Results and scrollbar troughs must have no persistent frame, while the
slider, selection, hover, press, disabled, and keyboard-focus states remain
clear. Full Screen must remain one uniform surface in every sidebar state.
For Horizontal navigation with a visible secondary row, verify equal thin
theme-derived boundaries at both strip edges; the boundaries must add no
spacing or change the measured rhythm. Scroll Results far enough to move icons
and labels across every viewport edge using the wheel, keyboard, scrollbar,
and touchpad, and confirm no result pixels appear in the strip, secondary row,
or outer frame.

In every windowed icon-grid layout, drag the menu width rapidly wider and
narrower across several column thresholds without pausing. Results must remain
continuously visible, existing columns must share newly available width instead
of leaving a growing blank area at the trailing edge, and complete columns must
continue appearing or disappearing during motion. The window must follow
without blinking, empty frames, or a delayed correction on release. Repeat
after selecting All Applications and with Places active; an enlarged width
must not prevent an immediate shrink. After restarting the panel/plugin, open
directly to Favourites and All Applications and confirm populated grids paint
without first hovering, scrolling, or switching categories.

Repeat representative cases at normal and high display scale and at 100, 125,
150, 175, and 200 percent text scale. Check the Apps/Places selector with icons
and labels in each of its sidebar, secondary-row, Search-row, and Full Screen
homes. Exactly one mode must be selected; labels must not clip or overlap; icon
sizes must match the host region within one device pixel; and accessible names,
checked state, Tab order, mouse switching, and LTR/RTL behavior must remain
correct.

With Places active in each Left and Right sidebar, compare the first Home row
with the first Applications navigation row in the otherwise identical layout.
Their top edges must match within one device pixel, with unused vertical space
below the Places group. Confirm no divider or reserved divider gap follows
Favourites when no lower group is visible. Compare the Home, History, and
Favourites artwork at normal and high display scale: the star should have a
consistent optical footprint while all three icons and labels remain in the
same columns. Repeat after moving through Horizontal navigation and after a
live theme change to catch stale first-frame spacer or icon state.

For Docked and Centered layouts with an effective secondary row, measure the
control edges in both vertical-sidebar directions. Apps/Places must remain on
the physical sidebar side, while Session actions must occupy the opposite side;
in particular, a Right sidebar puts the selector on the right and Session on
the left. With Profile visible, compare the avatar and username leading edge
with the category icon content edge after theme, display-scale, RTL, and live
sidebar-side changes. Keep Full Screen out of this correction check and verify
that its existing selector and Session arrangement is unchanged.

Hide category names and use a Profile name wider than the category icons. In
Left and Right Docked/Centered layouts, verify the whole avatar/name group fits
with equal space on both sides and shares its centre axis with every category
icon. Then use enough categories to require the vertical scrollbar: record the
launcher width on first map and repeat hover/scroll overflow transitions 20
times. The width must not change when the scrollbar appears.

Repeat 20 Applications-mode first openings, including an asynchronous menu
reload. Before moving the pointer, the category viewport must start at the top,
Favorites and All Applications must be visible, and Recently Used must match
its setting. None of these controls may first appear after hover or reopening.

Open Full Screen 20 times for Left, Right, Horizontal, and disabled sidebars,
then repeat after changing layout, sidebar state, Places, primary edge, text
direction, and available Session actions. The selector must be in its final
parent and order before the first visible frame, with no duplicate, missing,
or post-map-corrected frame.

Test opacity at 0, 60, 80, and 100 percent on composited X11. Background
surfaces must fade uniformly without compounded overlap, while foreground
labels, icons, selection, and focus remain opaque. Repeat without a compositor
and confirm every surface remains solid and readable. Import representative
older presets alongside current fields, save and export again, and confirm
unknown fields stay inert while supported values and unrelated Xfconf settings
are preserved. Run the complete matrix on X11 and a representative subset on
Wayland, recording any platform-specific visual differences.

## Routine CI and timing evidence

Every pull request to `main` and every `main` push runs one full
`debugoptimized` build and test suite on Ubuntu 26.04, Debian 13, and Fedora
44. Three focused checks cover sanitizers, catalogs and repository
consistency, and operation without optional build integrations or Calculator
provider executables. These six results are routine source and test evidence,
not live desktop results or installable package evidence.

The broader Xfce 4.16, 4.18, 4.20, and successor source-stack matrix is run
explicitly when compatibility evidence is needed. CodeQL runs independently
on a weekly schedule or manual request. Neither is a routine required result.

After a CI-shape change, timing is reviewed using the first ten consecutive
completed routine pull-request or `main` runs. Each measurement starts with
the run and ends when its last required result completes; at least nine should
finish within 15 minutes. A run is replaced only if a superseding change
cancelled it or a documented hosted-service incident overlapped it, with the
exclusion and next consecutive replacement recorded.

## Automated dependency evidence

Ubuntu 26.04 and Debian 13 build inputs are resolved from `debian/control`;
Fedora 44 uses `dnf builddep` on the RPM spec; Arch remains authoritative
through `makepkg --syncdeps`. Each positive gate records configured, disabled,
executed, skipped, failed, and timed-out tests, installs its artifact, checks
helper ownership and linkage, and runs the dependency-sensitive action
contract.

Direct repository contracts check the required Exo build and helper
declarations. The release workflow then builds each native package from the
selected tag's canonical source, runs the product suite, installs the package
in its target environment, and verifies its declared dependency closure and
installed actions.

Source cells cover Xfce 4.16, 4.18, and 4.20 with Exo. A separate
libxfce4ui-4.21-or-newer cell proves Exo is absent, checks symbols and dynamic
linkage, installs into an isolated root, and captures all ten interactions
when the explicit compatibility matrix is dispatched. This is staged-install
evidence, not routine distro CI or live desktop evidence.

The ten interactions are the icon chooser, Help, the Man Pages, Web Search,
Wikipedia, Run in Terminal, and Open URI search actions, launcher editing,
Places folder fallback, and Places terminal opening. Upgrade coverage repeats
the five search actions with historical stored defaults and verifies that
custom and persisted command strings remain byte-for-byte unchanged.

## Optional five-minute extension

- Switch among the built-in presets.
- Test a Calculator expression, then select or simulate a missing engine and
  confirm ordinary search still works.
- Drag an application or Place to Favourites.
- Try the available desktop action for an application or Place.
- If convenient, repeat a quick open/search check with a dark theme, vertical
  panel, or scaled display. Missing optional visual evidence is not a failure.

## Destructive pre-1.0 upgrade check

1. Record the exact `<source-version>` already installed and the exact
   `<target-version>` under test. In a separate profile, install the source
   version and add MeowMenu to the panel.
2. Add representative ordered favourites; choose a non-default layout; select
   non-default preferences; select Calculator engine, result size, and
   precision; save a custom preset. Create a second MeowMenu panel instance
   with different values.
3. Record `xfconf-query -c xfce4-panel -lv` and copy any external
   `.meowpreset` files to a comparison directory.
4. Upgrade to `<target-version>` without deleting configuration.
5. Start only the first instance. Confirm its preferences and managed custom
   presets are cleared, Modern is active, and its bare panel registration plus
   the second instance and external files are unchanged.
6. Start the second instance and confirm it resets independently. Change a
   setting in each completed instance, restart the panel twice, and confirm the
   reset does not repeat.
7. Repeat with a release-candidate profile, an initialized profile with no
   recognizable version, an interrupted `pending` lifecycle, a fresh profile,
   and an already-completed profile.
8. Import a current preset containing an unknown non-layout key and confirm its
   supported values apply. Try imports and overwrite attempts containing each
   retired layout key and `sidebar-position=top` or `bottom`; confirm the
   incompatible message appears and no target value changes.

Expected result: every eligible instance resets exactly once to Modern; fresh
and completed instances retain later changes; panel registrations, siblings,
other plugins, and external files remain unchanged.

## Removal and full cleanup

Removing a DEB, RPM, or Arch package removes installed program files but
normally retains the user's Xfconf settings and saved presets. Follow the
[instance cleanup instructions](installation#clean-one-meowmenu-instance) only
when you deliberately want to erase one instance's state.
