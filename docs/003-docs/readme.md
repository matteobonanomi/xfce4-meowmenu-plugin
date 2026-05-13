# MeowMenu Properties Refactoring

## A) Main Objective
Create a common dictionary to describe all the customizable components of MeowMenu and review the Properties windows to make them more explainable, simple and well-organized in Macro-areas.

This is my suggested dictionary:
* User/Session header: avatar, user name, session buttons (lock, logout, suspend, shutdown, reboot, etc)
* Search bar: already developed and improved in spec 001
* App grid: central grid / list showing apps that are categorized and/or filtered using the search bar
* Sidebar: category filters

## B) AS-IS
Three panels in the Properties window allow the use to heavily customize MeowMenu
### 1. General
This panel should contain very high level settings, the most relevant is the selection of Presets or save/import/export of custom Presets.

Presets are a bundle of low-level customizations that allow a simple switch between very different configurations, such as:
- Classic Preset: standard Whisker Menu behavior
- Modern Preset: a more eye-candy version, default choice
- FullScreen: a more mobile-oriented solution, similar to GNOME Activities menu

### 2. Appearance

Background opacity that can be configured in Appearence/Behaviour panels is divided in two main regions for Modern and Classic Preset:
- App Grid
- Everything else (user session, search bar, category sidebar)
The distinction is actually well implemented and must be kept as-is
Background opacity for full screen preset is again divided in two sections like explained before, but it does not make sense: it should be a unique opacity value for the whole menu.

### 3. Behaviour
it contains a lot of useful functionalities, yet in a very confused order.

## C) TO-BE
There are just examples of thing I would like to fix, but many more may be found after a careful analysis.
### 1. General
- Presets section must be the first thing in this panel with the title "Preset". Features are already there, maybe a little bit of improvements in UX/UI of the buttons can be made.
- From Appearance/Panel Button section must be moved to the General panel with the title "Panel plugin", to be more consistent and highlight this relevant feature
- From Behaviour/Layout, Panel Gap and Layout Mode must be included in General, as well as Menu width and menu height from Appearance. These 4 settings can be grouped with the title "General menu settings". If one selects "docked" as Layout mode, height and width must be customizable, if one selects "full screen" of course not. Add also the Corner Radius option from Appearance/Customization to this group. 
- If one selects "full screen" from available layout, a new option "full screen opacity" can be customizable. It is has the same UX/UI of the following "App box opaxity" or "Sidebar opacity", but it manager the overal opacity of the whole full-screen menu background, so it's basically the same thing as moving both opacities at the same level at the same time. It "full screen" is not selected, this option is there, but "greyish"
- "stay visible when focus is lost" option from Behaviour/Menu
### 2. Appearance/Behaviour/Advanced Search/Search Actions/Commands
The idea is to drop this definition because is misleading and create new panels the are consistent with menu dictionary elements:
- User/Session
- Search Bar
- App grid
- Sidebar
#### 2.1 User/Session
It will contain at least the following features. Other may be missing and must be considered if useful for consistency. Feel free to group them in sections in a rigoreous yet simple way. They must be easy to understand for every user:
- Profile position and commands position (now in Appearance/Customization)
- Show confirmation dialog from Behaviour/Session commands
- Add in a proper section the whole Commands section, since it basically allows to select which commands must be shown and what they do
#### 2.2 Search Bar
It will contain at least the following features. Other may be missing and must be considered if useful for consistency. Feel free to group them in sections in a rigoreous yet simple way. They must be easy to understand for every user:
- Take from Appearance/Customization move the "search bar position" option 
- Everything from Advanced Search
- Everything from Search Action. To save space you could take the box with del list of actions and the + and - buttons exactly as they are and add a three-horizontal-line button that means £edit" if an action is selected. If clicked, another small window appear with the already-present options: name / pattern / command / regular expression and the ok-cancel options.
#### 2.3 App Grid
It will contain at least the following features. Other may be missing and must be considered if useful for consistency. Feel free to group them in sections in a rigoreous yet simple way. They must be easy to understand for every user:
- the first option must be "Show applications as ..." and then a multi-choice icons / list  / trees instead of the current icons, to save space.
- if Icons is selected from the previous option, Grid density option must be customizable as well as "application icon size".
- from General take "show generic application names" and "show application tooltips" and "show application descriptions" (available only when the "list visualization is selected")
- From Appearance take also "Apps opacity" (to be renamed in "App box opacity" for sake of clarity), selectable only if "docked" layout is selected.
#### 2.4 SideBar
It will contain at least the following features. Other may be missing and must be considered if useful for consistency. Feel free to group them in sections in a rigoreous yet simple way. They must be easy to understand for every user:
- "show category names" from  General
- "category icon size" fro Appearance
- "category opacity" to be renamed in "sidebar opacity", selectable only if "docked" layout" is selected.
- "side bar position" from Appearance/Customization
- from Behaviour/Menu take "switch categories by hovering" and "sort categories"
- from Behaviour take the whole "Default category" section with the three options and the whole "Recently used" section

## D) Important checks
- no double options in different panels of the preferences, be sure there is no repetition/redudance
- no missing options compared to the actual configuration, add the options I forgot in a reasonable panel
- keep the very same default options for the 3 different default Preset

## E) Preset configuration files
To make Preset code more understandable, make sure that every Preset has a simple configuration file with all the default values. If one want to change it via file editor instead of UI, it is possible and just a restard of xfce4 panel will do the job. If one wants to copy pasta another config file, it will be possible if placed in  the right folder.
Add to the readme a basic instration for where to find this configu files and apply new config files so that they are visible from the UI

## F) Presets default standards

### Classic Preset
Exactly how the original Whisker Menu looks like. Docked layout, no radius border, no panel gap, icon+menu name, app list, no hovering. Sidebar on the right, search bar at the top, just like session and profile.
### Modern Preset
Docker layout, with rounded bordrs and panel gap. yes hovering, only icon for the menu in xfce4 panel, icon grid instead of list. Sidebar on the left, search bar at the bottom, action bar and profile at the top
### FullScreen Preset
Full screen layout, no opacity. Side bar on the left. Search bar on top at the center. There is an empty space at the right of the same size of the side bar on the left. Profile icon at the left of the search bar, session buttons at the right of the centered search bar. Scrolling application icon-style grid. app icons are larger and less dense grid than Modern laouyt to be consistent with a full screen view.
### Disclaimer
I could ask you to change these defaults or add more differences.