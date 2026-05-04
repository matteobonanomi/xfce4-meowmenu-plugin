# Whisker Menu — Mappa architetturale (read-only analysis)

> Bootstrap step §6.1 di `CLAUDE.md`. Documento di sola **analisi**: nessun
> codice modificato, nessuna implementazione proposta.
> Branch: `claude/naughty-mahavira-7ab97a` (worktree).
> Versione progetto: `2.10.0-dev` (vedi `meson.build`).

---

## 1. Vista d'insieme

`xfce4-whiskermenu-plugin` è un **modulo condiviso** (`shared_module`) caricato
da `xfce4-panel`. È scritto in **C++ (gnu++11)** con un piccolo file C che
ospita la macro di registrazione del plugin Xfce.

L'unità di compilazione produce due binari:

| Binario | Tipo | Sorgente principale | Scopo |
|---|---|---|---|
| `libwhiskermenu.so` | `shared_module` | `panel-plugin/*.cpp` + `register-plugin.c` | Plugin caricato dal pannello Xfce |
| `xfce4-popup-whiskermenu` | `executable` | `panel-plugin/xfce4-popup-whiskermenu.cpp` | CLI che chiede al pannello via D-Bus di mostrare il menu |

Architettura riassunta:

```
                        ┌──────────────────────────────┐
                        │      xfce4-panel             │
                        │  (host del plugin)           │
                        └──────────────┬───────────────┘
                                       │ XFCE_PANEL_PLUGIN_REGISTER
                                       ▼
       register-plugin.c ──► whiskermenu_construct(XfcePanelPlugin*)
                                       │
                                       ▼
                              ┌────────────────┐
                              │   Plugin       │  (panel-plugin/plugin.{h,cpp})
                              │   - bottone    │
                              │   - settings   │──► Settings (xfconf)
                              │   - menu win.  │──► Window
                              └───────┬────────┘
                                      │
              ┌───────────────────────┼─────────────────────────────┐
              ▼                       ▼                             ▼
        Window (window.cpp)    SettingsDialog (settings-dialog)    Command/Profile
        ├─ search entry        ├─ tab Appearance                    ├─ session cmds
        ├─ sidebar categorie   ├─ tab Behavior                      └─ AccountsService
        ├─ stack pagine        ├─ tab Commands
        │   ├─ FavoritesPage   └─ tab Search Actions
        │   ├─ RecentPage
        │   ├─ ApplicationsPage  ──► garcon (menu freedesktop)
        │   └─ SearchPage        ──► Query + Match[Launcher|SearchAction|RunAction]
        └─ LauncherView (interfaccia)
            ├─ LauncherTreeView
            └─ LauncherIconView
```

`xfce4-popup-whiskermenu` non interagisce direttamente con il plugin: si limita
a chiamare `org.xfce.Panel.PluginEvent("whiskermenu", "popup", ...)` su D-Bus.
Il pannello consegna l'evento al plugin tramite il segnale `remote-event` —
gestito da `Plugin::remote_event` in [panel-plugin/plugin.cpp](panel-plugin/plugin.cpp:381).

---

## 2. File principali

Tutti i sorgenti vivono in `panel-plugin/` (vedi
[panel-plugin/meson.build](panel-plugin/meson.build:1)). Note: nonostante il
CLAUDE.md citi una directory `src/`, qui la convenzione upstream è
`panel-plugin/`.

### 2.1 Bootstrap e ciclo di vita

| File | Ruolo |
|---|---|
| [register-plugin.c](panel-plugin/register-plugin.c) | Macro `XFCE_PANEL_PLUGIN_REGISTER(whiskermenu_construct)` — l'unico file C; espone il symbol che il pannello cerca. |
| [plugin.h](panel-plugin/plugin.h) / [plugin.cpp](panel-plugin/plugin.cpp) | Classe `Plugin`. Crea `Settings` e `Window`, costruisce il pulsante del pannello, collega tutti i segnali Xfce (`configure-plugin`, `mode-changed`, `remote-event`, `about`, `size-changed`, `free-data`). |
| [xfce4-popup-whiskermenu.cpp](panel-plugin/xfce4-popup-whiskermenu.cpp) | Eseguibile CLI; opzioni `--pointer/--center/--list/--instance/--version`. Usa `xfconf` per enumerare le istanze del plugin nei pannelli e `gdbus` per inviare l'evento `popup`. |

### 2.2 Finestra del menu e layout

| File | Ruolo |
|---|---|
| [window.h](panel-plugin/window.h) / [window.cpp](panel-plugin/window.cpp) | Classe `Window`. Toplevel `GtkWindow`, contiene `m_window_stack` (loading/contents) e `m_contents_stack` (contents/search). Gestisce: posizionamento (PositionAtButton/Cursor/Center), focus-out, key-press (escape, frecce, redirect verso search entry), supporto **gtk-layer-shell** se disponibile (Wayland), composizione visiva (`on_draw_event` con `menu_opacity`), 8 `Resizer` per il ridimensionamento. |
| [resizer.h](panel-plugin/resizer.h) / [resizer.cpp](panel-plugin/resizer.cpp) | Maniglie d'angolo/lato per il resize del menu. |
| [profile.h](panel-plugin/profile.h) / [profile.cpp](panel-plugin/profile.cpp) | Foto utente + nome. Usa **AccountsService** (opzionale, `HAVE_ACCOUNTS_SERVICE`) o fallback su `~/.face`. |

### 2.3 Pagine (concetto core: una `Page` ≈ una tab della sidebar)

| File | Ruolo |
|---|---|
| [page.h](panel-plugin/page.h) / [page.cpp](panel-plugin/page.cpp) | Classe base `Page`: incapsula `LauncherView`, `CategoryButton`, gestione click, drag&drop, menu contestuale (estendibile via `extend_context_menu`), aggiunta a desktop/pannello, edit launcher. |
| [favorites-page.h](panel-plugin/favorites-page.h) / [favorites-page.cpp](panel-plugin/favorites-page.cpp) | `FavoritesPage`: riordino drag&drop, sort asc/desc, persistenza via `Settings::favorites` (`StringList` di desktop-id). |
| [recent-page.h](panel-plugin/recent-page.h) / [recent-page.cpp](panel-plugin/recent-page.cpp) | `RecentPage`: ultimi N lanciati (`recent_items_max`), enforce dei limiti su show. |
| [applications-page.h](panel-plugin/applications-page.h) / [applications-page.cpp](panel-plugin/applications-page.cpp) | `ApplicationsPage`: caricamento asincrono del menu freedesktop tramite **garcon** in un `GTask` su thread separato; due menu uniti — quello principale (o file custom) + `xfce-settings-manager.menu`. Gestisce stato `Invalid → Loading → Done/ReloadRequired`. |
| [search-page.h](panel-plugin/search-page.h) / [search-page.cpp](panel-plugin/search-page.cpp) | `SearchPage`: vedi §4. |

### 2.4 Modello dei dati lanciabili

| File | Ruolo |
|---|---|
| [element.h](panel-plugin/element.h) / [element.cpp](panel-plugin/element.cpp) | Base astratta `Element`: icona + testo + tooltip + sort-key + `run(GdkScreen*)` virtuale + `search(const Query&)` virtuale. Tutti gli oggetti che possono apparire in un risultato di ricerca derivano da qui. |
| [launcher.h](panel-plugin/launcher.h) / [launcher.cpp](panel-plugin/launcher.cpp) | `Launcher` (eredita `Element`): wrapper su `GarconMenuItem`. Contiene stringhe normalizzate per ricerca (`m_search_name`, `m_search_keywords`, `m_search_command`, `m_search_comment`, `m_search_generic_name`) e supporto `DesktopAction`. |
| [search-action.h](panel-plugin/search-action.h) / [search-action.cpp](panel-plugin/search-action.cpp) | `SearchAction` (eredita `Element`): pattern utente (`?`, `!`, `!w`, regex…) → comando shell con placeholder. |
| [run-action.h](panel-plugin/run-action.h) / [run-action.cpp](panel-plugin/run-action.cpp) | `RunAction` (eredita `Element`): voce sintetica "Esegui *query*" sempre presente nei risultati di ricerca. |
| [category.h](panel-plugin/category.h) / [category.cpp](panel-plugin/category.cpp) | `Category` (eredita `Element`): aggrega launcher e sotto-categorie; produce un `GtkTreeModel` (List o Tree a seconda di `view_mode`). |
| [category-button.h](panel-plugin/category-button.h) / [category-button.cpp](panel-plugin/category-button.cpp) | Toggle button della sidebar; raggruppato in radio-group. |

### 2.5 Viste

| File | Ruolo |
|---|---|
| [launcher-view.h](panel-plugin/launcher-view.h) | Interfaccia astratta: `get_widget`, `set_model`, navigazione cursori, drag-source/dest, hover-selection. |
| [launcher-tree-view.h](panel-plugin/launcher-tree-view.h) / [.cpp](panel-plugin/launcher-tree-view.cpp) | `GtkTreeView` — mode List (`ViewAsList`) e Tree (`ViewAsTree`). |
| [launcher-icon-view.h](panel-plugin/launcher-icon-view.h) / [.cpp](panel-plugin/launcher-icon-view.cpp) | `GtkIconView` — mode `ViewAsIcons`. |
| [icon-renderer.h](panel-plugin/icon-renderer.h) / [.cpp](panel-plugin/icon-renderer.cpp) | Rendering icone (scaling, fallback). |
| [icon-size.h](panel-plugin/icon-size.h) / [.cpp](panel-plugin/icon-size.cpp) | Enum tipato per dimensioni icone. |
| [image-menu-item.h](panel-plugin/image-menu-item.h) | Helper per menu item con icona (la GTK3 di alto livello l'ha deprecato). |

### 2.6 Impostazioni

| File | Ruolo |
|---|---|
| [settings.h](panel-plugin/settings.h) / [settings.cpp](panel-plugin/settings.cpp) | Classe `Settings` + wrapper tipati `Boolean`/`Integer`/`String`/`StringList`/`SearchActionList` su **xfconf**. Vedi §5. |
| [settings-dialog.h](panel-plugin/settings-dialog.h) / [settings-dialog.cpp](panel-plugin/settings-dialog.cpp) | Dialog GTK delle preferenze, 5 tab (General, Appearance, Behavior, Commands, Search Actions). |
| [command.h](panel-plugin/command.h) / [command.cpp](panel-plugin/command.cpp) | `Command`: 11 comandi di sessione (Settings, LockScreen, SwitchUser, LogOut/User, Restart, ShutDown, Suspend, Hibernate, LogOut*, MenuEditor, Profile) — bottoni in alto + voci di menu, conferme con countdown. |
| [command-edit.h](panel-plugin/command-edit.h) / [.cpp](panel-plugin/command-edit.cpp) | Riga di edit nel tab Commands. |

### 2.7 Utility

| File | Ruolo |
|---|---|
| [slot.h](panel-plugin/slot.h) | `WhiskerMenu::connect(...)`: helper templatico che permette di passare lambda a `g_signal_connect_data`. È il pattern di binding usato dappertutto. |
| [query.h](panel-plugin/query.h) / [query.cpp](panel-plugin/query.cpp) | `Query`: tokenizzazione UTF-8, normalizzazione (`g_utf8_normalize` + `g_utf8_casefold`), funzioni `match()` e `match_as_characters()` con scoring deterministico. Vedi §4.2. |

### 2.8 Asset e I/O

| Path | Contenuto |
|---|---|
| `icons/` | Icone PNG/SVG del plugin (pulsante pannello, voci menu). |
| `po/` | Traduzioni gettext; aggiornate spesso (vedi git log). |
| `panel-plugin/whiskermenu.desktop.in` | `.desktop` del plugin per il pannello. |
| `panel-plugin/xfce4-popup-whiskermenu.1` | Manpage della CLI. |
| `xfce-revision.h.in` | Template per iniettare lo SHA git via `vcs_tag`. |
| `meson_options.txt` | Opzioni build (accountsservice, gtk-layer-shell). |

---

## 3. Flusso di apertura del menu

Tre vie d'ingresso:

### 3.1 Via click sul pulsante del pannello

In [plugin.cpp:102-121](panel-plugin/plugin.cpp:102):

```
GtkToggleButton (m_button) clicked
        │ button-press-event
        ▼
Plugin::show_menu(Window::PositionAtButton)
        │
        ├─ se m_settings->menu_opacity è cambiato e c'è un toggle 100↔<100
        │  → distrugge e ricrea m_window (per cambio visual RGBA)
        ├─ xfce_panel_plugin_block_autohide(true)
        ├─ toggle_button_set_active(true)
        ▼
Window::show(PositionAtButton)
```

### 3.2 Via D-Bus / scorciatoia da tastiera

```
xfce4-popup-whiskermenu [--pointer|--center|--instance N]
        │  g_dbus_proxy_call_sync("PluginEvent", "whiskermenu", "popup", int)
        ▼
xfce4-panel  ─── segnale "remote-event" ───►  Plugin::remote_event()  [plugin.cpp:381]
        │
        ├─ se il menu si è chiuso da <250 ms → ignora (anti-toggle race)
        ├─ se visibile → m_window->hide()
        └─ altrimenti → show_menu(value)   // 0=button, 1=pointer, 2=center
```

### 3.3 Sequenza di `Window::show(Position)` ([window.cpp:456](panel-plugin/window.cpp:456))

1. `update_view()` su tutte e 4 le pagine (Search/Favorites/Recent/Applications) — applica eventuale cambio `view_mode`.
2. Show/hide tooltip a seconda di `launcher_show_tooltip`.
3. `Profile::reset_tooltip()`.
4. Per ogni `Command`: `check()` (resetta visibilità in base a `which`/path).
5. `RecentPage::enforce_item_count()`.
6. Pulsante "Recent" mostrato solo se `recent_items_max > 0`.
7. **`ApplicationsPage::load()`**: se non già caricato, parte un `GTask` che esegue `garcon_menu_load` su thread; il callback main-thread chiama `set_loaded()` che ferma lo spinner e mostra `contents`. Se già caricato (`Done`), restituisce `true` e si salta lo spinner.
8. `reset_default_button()` + `show_default_page()` — pagina iniziale = `default_category` (Favorites/Recent/All).
9. Reset selezioni + reload icon size su tutti i view.
10. Calcolo `m_geometry` in base a `position`:
    - **PositionAtButton**: aspetta fino a 0.5 s che il pannello auto-hide si stabilizzi (`gtk_window_get_position`), poi `Plugin::get_menu_position` (delegato a `xfce_panel_plugin_position_widget`).
    - **PositionAtCursor**: `gdk_device_get_position` del seat principale.
    - **PositionAtCenter**: `center_window()` sul monitor risolto con `gdk_display_get_monitor_at_point`.
11. Su Wayland (gtk-layer-shell): `gtk_layer_set_monitor` + ancore.
12. `set_size(menu_width, menu_height)` clampato al monitor.
13. Se layout cambiato (LTR/RTL, posizioni alternative, profilo): `update_layout()` riordina vbox/grid completamente.
14. `gtk_window_present()`.
15. Re-fetch della posizione e nuovo `move_window()` per evitare overlap col pannello.

### 3.4 Chiusura

`Window::hide()` ([window.cpp:425](panel-plugin/window.cpp:425)) salva
`favorites` e `recent`, scrolla la sidebar in cima, nasconde i bottoni
comandi (per togliere il bordo "active"), torna alla pagina di default e
notifica `Plugin::menu_hidden()`. Inoltre il segnale `hide` (collegato in
[plugin.cpp:201](panel-plugin/plugin.cpp:201)) registra `m_hide_time` per la
finestra anti-race con `remote-event`.

---

## 4. Gestione della ricerca

### 4.1 Catena dei segnali

`m_search_entry` è una `GtkSearchEntry` ([window.cpp:204](panel-plugin/window.cpp:204)).

```
GtkSearchEntry  signal "changed"
        ▼
Window::search()                                     [window.cpp:1122]
        │  testo vuoto?
        ├─ sì → m_contents_stack -> "contents" (mostra pagina attiva)
        └─ no → m_contents_stack -> "search"   (mostra SearchPage)
        │
        ▼
SearchPage::set_filter(text)                         [search-page.cpp:85]
```

In più, in `SearchPage::SearchPage` ([search-page.cpp:43](panel-plugin/search-page.cpp:43)):
- segnale `activate` (Invio) → `set_filter` + attiva la prima riga della pagina attiva.
- segnale `stop-search` (Esc su `GtkSearchEntry`) → svuota il testo.

`Window::on_key_press_event_after` ([window.cpp:877](panel-plugin/window.cpp:877))
intercetta tasti non gestiti e li **inoltra alla search entry**, così
l'utente può iniziare a scrivere senza prima focalizzarla.

### 4.2 Algoritmo di matching ([query.cpp](panel-plugin/query.cpp))

La query e gli haystack passano per `g_utf8_normalize` + `g_utf8_casefold`,
poi in `Query::set` la query è splittata in token su whitespace.

`Query::match(haystack)` produce un punteggio (più piccolo = migliore):

| Score | Condizione |
|---|---|
| `0x4` | haystack == query |
| `0x8` | haystack inizia con query |
| `0x10` | query trovata su confine di parola |
| `0x20` | tutte le parole trovate in ordine, ognuna a inizio parola |
| `0x40` | tutte le parole trovate in qualunque ordine, a inizio parola |
| `0x80` | substring qualsiasi |
| `UINT_MAX` | nessun match |

`Query::match_as_characters` (usata da `Launcher::search` per le keyword)
cerca i caratteri della query come **sottosequenza**, premiando le
corrispondenze su iniziali di parola (`0x100`) rispetto alle generiche
(`0x200`). È il pezzo che dà la fuzzy-search "abbreviazioni" tipica di
Whisker.

Ogni `Element::search(query)` ritorna un `unsigned int` di rilevanza; più
piccolo è, più il match è forte.

### 4.3 Pipeline di `SearchPage::set_filter`

1. **Reset condizionale**: se la nuova query non è un'estensione della
   precedente (non parte con la stringa precedente), ricostruisce
   `m_matches` da `m_launchers` + `&m_run_action`. Se invece estende,
   filtra solo i `Match` rimasti.
2. **Search actions**: per ogni `SearchAction` di `m_settings->search_actions`
   calcola un match a parte. Se nessun risultato standard è presente, ripiega
   sulle search action **non-regex** anteponendogli il loro pattern (es.
   `?foo` per il web search).
3. **Sort**: `std::stable_sort` per `Match::operator<` (rilevanza
   crescente).
4. **Modello GTK**: costruisce un nuovo `GtkListStore` (4 colonne:
   `COLUMN_ICON`, `COLUMN_TEXT`, `COLUMN_TOOLTIP`, `COLUMN_LAUNCHER`)
   prima con search action, poi con i match standard, e lo passa al
   `LauncherView`.
5. `select_first()` sulla view.

### 4.4 Ordine di base (riusato come tie-break)

`SearchPage::update_search_order` ([search-page.cpp:241](panel-plugin/search-page.cpp:241))
sposta in testa a `m_launchers` prima i recenti poi i preferiti — così, a
parità di rilevanza, quelli usati spesso restano in alto. Il flag
`is_order_unchanged` evita di ripetere il sort inutilmente.

### 4.5 Note importanti per evoluzioni future (Phase 1 "Search Ranking 2.0")

- Il punteggio è **interamente integer-based, deterministico, senza
  apprendimento**. Non c'è nessun modello statistico o tracking d'uso oltre
  alla lista "recent" + ordine "favorites".
- `SearchAction` e `RunAction` sono **fuori** dalla normale pipeline dei
  launcher: vivono in vettori paralleli (`search_action_matches` e
  `m_matches`).
- Non esiste oggi alcun concetto di "provider" pluggable: tutti i risultati
  vengono da `ApplicationsPage::find_all()` (ovvero `garcon`) + le search
  action statiche dell'utente.

---

## 5. Gestione delle impostazioni

### 5.1 Backend

L'unico storage persistente per la configurazione è **xfconf** (canale
`xfce4-panel`, base property impostata con
`xfce_panel_plugin_get_property_base`). Coerente con la regola §3.3 di
`CLAUDE.md` (no parallel config store).

### 5.2 Modello tipato ([settings.h](panel-plugin/settings.h))

Quattro classi wrapper, ognuna parla con `xfconf_channel_*`:

| Classe | Tipo | Conversione |
|---|---|---|
| `Boolean` | `bool` | `xfconf_channel_set/get_bool` |
| `Integer` | `int` (clamp `[min,max]`) | `xfconf_channel_set/get_int` |
| `String` | `std::string` | `xfconf_channel_set/get_string` |
| `StringList` | `std::vector<std::string>` | `xfconf_channel_set_arrayv` (array di GValue stringa) |
| `SearchActionList` | `std::vector<SearchAction*>` | `/search-actions/<id>/(name|pattern|command|regex)` come hash flat |

Ogni wrapper ha:
- un valore di default (rilevato dai file `defaults.rc`),
- `load(XfceRc*, is_default)` per i due vecchi RC files,
- `load(property, GValue*)` per i `property-changed` di xfconf in tempo reale,
- un setter che chiama `begin_property_update` / `end_property_update` per
  evitare il loop di notifica.

### 5.3 Catena di caricamento ([plugin.cpp:64-90](panel-plugin/plugin.cpp:64))

```
1. Settings(plugin)                              // costruisce, registra defaults statici
2. settings->load(defaults.rc, is_default=true)  // override fornito dalla distro
3. settings->load(plugin_lookup_rc_file, true)   // RC per-plugin (legacy)
4. settings->load(plugin_get_property_base)      // ▶ qui si apre il canale xfconf
5. se rc legacy esiste → migrazione + g_remove(rc)
6. prevent_invalid()                             // valida vincoli incrociati
```

I file RC sono retrocompatibilità: la fonte viva è xfconf.

### 5.4 Notifiche e reload

`Settings::property_changed` ([settings.cpp:368](panel-plugin/settings.cpp:368))
smista i cambi:

| Trigger | Conseguenza |
|---|---|
| `favorites` / `recent` / `launcher_show_name` / `launcher_show_description` / `sort_categories` / `view_mode` | `Plugin::reload_menu()` (rebuild lista app) |
| `button_*` | `Plugin::reload_button()` |
| `custom_menu_file`, vari layout, sizes, opacity, search_actions, ecc. | applicato direttamente al prossimo `show()` |
| `command-*` / `show-command-*` | smistato a ognuno dei `Command` |

`prevent_invalid()` rimedia agli stati assurdi (categoria vuota, pulsante
pannello vuoto, default category = recent quando recent disabilitato).

### 5.5 Catalogo proprietà (così come dichiarate al costruttore in [settings.cpp:33](panel-plugin/settings.cpp:33))

| Property | Tipo | Default | Note |
|---|---|---|---|
| `/favorites` | StringList | 4 desktop-id Xfce standard | persistito su `hide()` |
| `/recent` | StringList | vuoto | |
| `/custom-menu-file` | String | "" | path a `.menu` alternativo |
| `/button-title`, `/button-icon` | String | "Applications", `org.xfce.panel.whiskermenu` | |
| `/show-button-title`, `/show-button-icon`, `/button-single-row` | Bool | f, t, f | |
| `/launcher-show-name`, `/launcher-show-description`, `/launcher-show-tooltip` | Bool | t, t, t | |
| `/launcher-icon-size`, `/category-icon-size` | IconSize | Small, Smaller | |
| `/hover-switch-category`, `/category-show-name`, `/sort-categories` | Bool | f, t, t | |
| `/view-mode` | Int enum | List | Icons/List/Tree |
| `/default-category` | Int enum | Favorites | Favorites/Recent/All |
| `/recent-items-max` | Int [0,100] | 10 | 0 = nascondi recent |
| `/favorites-in-recent` | Bool | f | |
| `/position-{profile,search,commands,categories}-alternate` | Bool | f | layout swap |
| `/position-categories-horizontal` | Bool | f | sidebar a barra orizzontale |
| `/stay-on-focus-out` | Bool | f | non si chiude su perdita focus |
| `/profile-shape` | Int enum | Round | Round/Square/Hidden |
| `/confirm-session-command` | Bool | t | dialog di conferma per shutdown ecc. |
| `/menu-{width,height}` | Int | 450, 500 | |
| `/menu-opacity` | Int [0,100] | 100 | trigger di ricreazione `Window` |
| `/command-*`, `/show-command-*` | String + Bool | vedi tabella in [settings.cpp:105-169](panel-plugin/settings.cpp:105) | 11 entry |
| `/search-actions/*` | SearchActionList | 6 default (Man, Web, Files, Wikipedia, Run, Open URI) | comandi differenti pre/post `libxfce4ui 4.21` (`xfce-open` vs `exo-open`) |

### 5.6 SettingsDialog

[settings-dialog.h](panel-plugin/settings-dialog.h) struttura un
`GtkDialog` con cinque tab:

1. `init_general_tab()` — display di default, recent items, favorites in recent.
2. `init_appearance_tab()` — vista (Icons/List/Tree), nomi/descrizioni/tooltip, dim. icone, opacità, layout swap, profilo, dim. menu, button style.
3. `init_behavior_tab()` — hover-switch, stay-on-focus-out, sort categories, conferma sessione.
4. `init_commands_tab()` — 11 `CommandEdit` (ognuno: enable + entry + bottone Run).
5. `init_search_actions_tab()` — `GtkTreeView` con add/remove + form per name/pattern/command/regex.

L'editor di icona apre `xfce_dialog_show_icon_chooser`. Il salvataggio di
`search_actions` avviene a chiusura del dialog (vedi
[plugin.cpp:346-352](panel-plugin/plugin.cpp:346)).

---

## 6. Dipendenze Xfce / GTK / sistema

Dal [meson.build](meson.build:22) (versioni minime in `dependency_versions`):

| Dipendenza | Min | Uso nel codice |
|---|---|---|
| **glib-2.0** ≥ 2.50 | core | `GTask`, `g_utf8_*`, `g_dbus_*`, `GFileMonitor`, `GRegex`, `g_signal_*` |
| **gio-2.0** ≥ 2.50 | core | `GIcon`, `GFile`, D-Bus (per il binario popup) |
| **gtk+-3.0** ≥ 3.22 | UI | tutta l'interfaccia (GtkWindow, GtkStack, GtkSearchEntry, GtkTreeView, GtkIconView, …). **Nota: GTK 4 esplicitamente fuori scope (CLAUDE.md §3.2).** |
| **garcon-1** ≥ 4.16 | menu | `GarconMenu`, `GarconMenuItem`, `GarconMenuDirectory`, `GarconMenuSeparator`, `garcon_menu_new_applications`, `garcon_menu_new_for_path` |
| **libxfce4panel-2.0** ≥ 4.16 | host | `XFCE_PANEL_PLUGIN_REGISTER`, `xfce_panel_plugin_*` (rc file, save location, position widget, autohide, action widget, mode, size, nrows, icon-size, set-small) |
| **libxfce4ui-2** ≥ 4.16 | UI utility | `xfce_dialog_*` (about, icon chooser, conferme), `xfce_textdomain`, `xfce_str_is_empty` |
| **exo-2** ≥ 4.16 | (solo se `libxfce4ui < 4.21`) | comandi default delle search action (`exo-open`) |
| **libxfce4util-1.0** ≥ 4.16 | I/O | `XfceRc` (RC files), `xfce_resource_lookup` |
| **libxfconf-0** ≥ 4.16 | config | l'unica via di persistenza (vedi §5) |
| **accountsservice** ≥ 0.6.45 | opzionale (`HAVE_ACCOUNTS_SERVICE`) | `Profile`: foto + nome utente reale |
| **gtk-layer-shell-0** ≥ 0.7 | opzionale (`HAVE_GTK_LAYER_SHELL`) | finestra menu su Wayland (anchor, layer overlay, keyboard mode) |

CFLAG di feature attivati condizionalmente: `-DHAVE_ACCOUNTS_SERVICE=1`,
`-DHAVE_GTK_LAYER_SHELL=1`. La `meson_options.txt` consente di disattivarli.

`SETTINGS_MENUFILE` viene iniettato dal build come
`<sysconfdir>/xdg/menus/xfce-settings-manager.menu` ed è il fallback per il
menu impostazioni se l'utente non ha override locale (vedi
[applications-page.cpp:271](panel-plugin/applications-page.cpp:271)).

Non ci sono dipendenze runtime non-Xfce nel codice del plugin (nessun
SQLite, nessun cURL, nessun JSON, nessun thread pool custom — solo `GTask`).
Aderisce a CLAUDE.md §3.2.

---

## 7. Punti di estensione plausibili

Non è un'architettura "a plugin" interna: è monolitica all'interno della
`shared_module`. Tuttavia alcuni snodi sono **naturali punti di iniezione**
per le evoluzioni descritte in `docs/whisker-modernization-spec.md` (Phase 1
"Search Ranking 2.0", Phase 3 "Provider search / Runner").

### 7.1 Ricerca: nuovi tipi di risultato

Il modello `Element` con `run()` + `search(Query)` è la **giunzione naturale**.
Oggi le sottoclassi sono `Launcher`, `SearchAction`, `RunAction`, `Category`.
Aggiungere un tipo nuovo (es. risultato da provider esterno) richiederebbe:

- una nuova sottoclasse di `Element`;
- inserire i suoi `Match` nel vettore costruito da
  `SearchPage::set_filter` ([search-page.cpp:85](panel-plugin/search-page.cpp:85))
  — oggi cablato hard su `m_launchers` + `m_run_action` + `m_settings->search_actions`;
- nessuna modifica a `LauncherView` (che parla solo per `COLUMN_LAUNCHER` come
  `gpointer`); l'`activate_path` chiama `Element::run` polimorfico.

### 7.2 Algoritmo di scoring

Tutto il calcolo di rilevanza è in:

- `Query::match` / `Query::match_as_characters` ([query.cpp](panel-plugin/query.cpp))
- `Launcher::search` ([launcher.cpp](panel-plugin/launcher.cpp))
- `SearchAction::search`, `RunAction::search`

Sono i punti su cui agganciare ranking 2.0 (typo-tolerance, history boost,
recency decay, ecc.) senza toccare la pipeline GTK.

### 7.3 Sorgente del menu

`ApplicationsPage::load_garcon_menu` ([applications-page.cpp:238](panel-plugin/applications-page.cpp:238))
è l'unico punto in cui si parla con `garcon`. Il flag
`m_settings->custom_menu_file` permette già di sostituire il file `.menu` —
ma rimane un caricamento garcon-puro. Per un eventuale "provider model"
questo è il punto di estrazione/astrazione (interfaccia tra `Page` e
"sorgente dati"). Il pattern `LoadStatus`
(`Invalid → Loading → ReloadRequired → Done`) è già pronto a gestire
ricariche asincrone.

### 7.4 Pagine aggiuntive nello stack

`Window::m_panels_stack` è un `GtkStack` con tre named children
(`favorites`, `recent`, `applications`). Le `CategoryButton` sono già un
gruppo radio estendibile: `Window::set_categories(vector)` è il punto
attualmente usato da `ApplicationsPage` per iniettare le sue categorie — la
stessa API potrebbe accogliere bottoni-pagina aggiuntivi (con la dovuta
logica di toggle in [window.cpp:686](panel-plugin/window.cpp:686)).

### 7.5 Search actions

Il sistema delle search action è già un mini-DSL utente:
- prefisso letterale (`?`, `!w`) o regex,
- comando con `%s` / `%u` / backref regex `\0`…
- editabile da `SettingsDialog` tab 5,
- persistito in xfconf sotto `/search-actions/<id>/*`.

Estendibile (più placeholder, condizioni, gruppi) modificando
`SearchAction::run` e `match_prefix` / `match_regex`, senza intaccare il
resto.

### 7.6 Layout della finestra

`Window::update_layout` ([window.cpp:1148](panel-plugin/window.cpp:1148)) è il
punto dove vivono *tutte* le permutazioni di posizione (search alternate,
commands alternate, categories alternate, RTL/LTR). Ogni nuova preferenza di
layout va orchestrata qui.

### 7.7 Comandi di sessione

`Settings::Commands` enum + `command[]` array ([settings.h:358](panel-plugin/settings.h:358))
sono dichiarativi. Aggiungere un comando = aggiungere voce all'enum,
costruirlo nel ctor di `Settings` e aggiungere il `CommandEdit` nel tab.

### 7.8 Ciclo di vita finestra

`Window::show`/`hide`/`menu_hidden` + `Plugin::menu_hidden` consentono di
agganciare comportamenti pre/post-apertura senza toccare il widget tree.
È il punto giusto per un eventuale "warm cache" o telemetria locale.

### 7.9 Wayland / X11

Il codice è già pronto a un dual-path: `#ifdef HAVE_GTK_LAYER_SHELL` +
controlli runtime `gtk_layer_is_supported()` in `Window::Window`,
`Window::show`, `Window::move_window`. È il pattern da estendere per ogni
funzionalità che debba essere "X11+Wayland parity" o "X11-first with Wayland
fallback" (CLAUDE.md §3.4).

---

## 8. Note di lettura per future sessioni

- Convention C++: nessuna eccezione, no smart pointer (raw `new`/`delete`),
  signal connection via [slot.h](panel-plugin/slot.h) (lambdas → GClosure).
  Distruzione gestita esplicitamente nei dtor.
- Ownership delle finestre GTK: `g_object_ref_sink` in `Window::Window`
  ([window.cpp:370](panel-plugin/window.cpp:370)) + `g_object_unref` nel dtor.
- Tutti i widget di `Window` sono creati una sola volta nel ctor; `show()`
  rifà solo modello, geometria e layout.
- `xfce_panel_plugin_*`: l'ovvio, ma vale la pena ricordare che il **panel
  fa il free** del plugin via segnale `free-data`
  ([plugin.cpp:157](panel-plugin/plugin.cpp:157)) → `delete this`.
- I file in `po/` cambiano spesso; le PR di traduzione non vanno toccate.
- Per qualsiasi modifica all'UI utente, **§3.3 di CLAUDE.md** richiede:
  proprietà xfconf, GUI, default sicuro, reset, schema migration.

---

## 9. Aderenza a CLAUDE.md

Questo documento è il deliverable di **§6.1 — Architectural read**. Non è
stato modificato alcun file di codice, non è stato eseguito alcun comando di
build, non è stato avviato `meson`. La directory `.specify/` non è stata
toccata. Il prossimo passo previsto dal bootstrap è
`/speckit-constitution` — da invocare solo dopo l'approvazione umana di
questa mappa.
