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
on the left, right, horizontal, and disabled, Search at both edges, visible
and hidden Session controls, Applications and Places, docked, centered, and
fullscreen layouts, and both LTR and RTL text directions. At every edge check
internal-first movement, one-move crossings, no-wrap no-ops, Search cursor and
Shift selection, Space during composition, context-menu priority, Escape,
Calculator-first anchoring, asynchronous Places focus, live layout changes,
and focus visuals.

After each live layout change and after opening directly into a search, click
or focus each non-text region and type a printable character. Confirm it is
inserted into Search exactly once and updates Results. Repeat after a transition
that momentarily leaves no child focused, then use every arrow direction from
Search and from the first, middle, and last items in both list and grid views.
Each event must make at most one move and must not stall at a region boundary.

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
clear. Compare selected list rows and grid tiles in each theme: both must use
the theme selection background and foreground with equally clear contrast.
Full Screen must remain one uniform surface in every sidebar state.
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

Repeat with Profile hidden. Search must end exactly at the Results/sidebar
boundary instead of covering the sidebar chrome, and every icon-only category
control must fill the sidebar width with symmetric side space. Mirror the check
between Left and Right. With the sidebar disabled or Horizontal, confirm Search
may again use the complete launcher width when no other primary-row control
needs that space.

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

CodeQL runs independently on a weekly schedule or manual request. It is not a
routine required result.

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

## Optional five-minute extension

- Switch among the built-in presets.
- Test a Calculator expression, then select or simulate a missing engine and
  confirm ordinary search still works.
- Drag an application or Place to Favourites.
- Try the available desktop action for an application or Place.
- If convenient, repeat a quick open/search check with a dark theme, vertical
  panel, or scaled display. Missing optional visual evidence is not a failure.

## Configuration and upgrade check

Before final 1.0.0, compatibility and configuration preservation are not
guaranteed. Keep a backup of the panel configuration, then test the current
release in both a fresh profile and a profile with existing MeowMenu settings.
Confirm that the launcher opens, the selected preset and current Properties
controls work, saved custom presets can be exported and imported, and unrelated
panel items remain available. Record any difference as evidence for the exact
release, distribution, Xfce version, architecture, session, and installation
method above.

From final 1.0.0 onward, repeat the same check as a configuration-preservation
validation. The stable series carries that preservation guarantee.

## Removal and full cleanup

Removing a DEB, RPM, or Arch package removes installed program files but
normally retains the user's Xfconf settings and saved presets. Follow the
[installation guide](installation) when you deliberately want to remove the
package or clear its user data.
