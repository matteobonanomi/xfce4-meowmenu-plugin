# Contract: Xfconf schema additions for Layout Presets

Tutti i nomi sono **kebab-case** per coerenza con le proprietà esistenti del plugin Whisker.

## Namespace base

`/plugins/whiskermenu-N/` — il `N` è l'id-istanza del plugin in `xfce4-panel` (esistente, non cambia). Tutte le proprietà sotto.

## Nuove proprietà runtime

| Property | Type | Range / Domain | Default | Note |
|---|---|---|---|---|
| `schema-version` | int | ≥ 0 | 0 | Set a 1 dopo la migrazione. |
| `current-preset-id` | string | builtin id \| user uuid \| `""` | `""` | Empty = no preset linked. |
| `corner-radius` | int | 0–24 | 0 | Pixel. Reso via cairo in `on_draw_event`. |
| `panel-gap` | int | 0–50 | 0 | Pixel. Direzione auto da `panel_position`. |
| `categories-opacity` | int | 0–100 | 100 | Percentuale. Applicato alla zona categorie/profilo. |
| `apps-opacity` | int | 0–100 | 100 | Percentuale. Applicato alla zona applicazioni/ricerca. |
| `sidebar-position` | string | `left` \| `right` \| `hidden` | `left` | Posizione della sidebar categorie. |
| `search-bar-position` | string | `top` \| `bottom` | `top` | Posizione della search entry. |
| `profile-position` | string | `top` \| `bottom` \| `bottom-right` \| `hidden` | `top` | `bottom-right` = grid mode FullScreen. |
| `commands-position` | string | `top-right` \| `bottom-right` \| `hidden` | `top-right` | Posizione delle action buttons (logout/lock/shutdown). |
| `grid-columns` | int | 2–10 | 4 | Colonne della GtkIconView in modalità grid. |
| `grid-rows` | int | 1–8 | 3 | Righe target della GtkIconView (FullScreen). |
| `grid-density` | string | `low` \| `medium` \| `high` | `medium` | Modula `item_padding` e `column_spacing`. |
| `layout-mode` | string | `docked` \| `fullscreen` | `docked` | Switch macro: docked = finestra normale, fullscreen = layer-shell o resize a monitor. |

## Validazione

- Stringhe che non rientrano nel dominio enumerato → reset al default + warning a stderr (`g_warning`).
- Integer fuori range → clamp + warning. Pattern già in uso in `settings.cpp` per `Integer::set_min_max()`.
- Tutte le scritture passano dai wrapper `Settings::Boolean/Integer/String`; non è ammesso `xfconf_channel_set_*()` diretto da nuovo codice.

## Sotto-albero preset utente

`/plugins/whiskermenu-N/presets/<uuid>/`

| Property | Type | Note |
|---|---|---|
| `display-name` | string | Univoco. Validato in UI. |
| `created-by` | string | `meowmenu-X.Y.Z` per debug. |
| Una entry per ogni proprietà runtime salvata | bool/int/string | Stessi nomi e tipi della tabella sopra. Solo le proprietà che il preset memorizza; le altre eredita dalla config attiva al momento dell'apply. |

Lista completa dei preset utente: ottenuta enumerando i sotto-namespace di `/presets/` con `xfconf_channel_get_properties()` + filtraggio.

## Eliminazione

- Eliminare un preset utente: `xfconf_channel_reset_property("/plugins/whiskermenu-N/presets/<uuid>", recursive=TRUE)`. Pattern già usato in `settings.cpp:1070, 1222`.
- Eliminare un preset built-in: NON è un'operazione esposta. `apply_preset_by_name("classic" | "modern" | "fullscreen")` è sempre disponibile.

## Backward compatibility

- Tutte le proprietà preesistenti del plugin Whisker upstream restano invariate. Il preset Classic NON ne sovrascrive nessuna sopra il default upstream.
- Una versione precedente di MeowMenu/Whisker che legge un channel con `schema-version=1` e proprietà sconosciute le ignora (comportamento Xfconf nativo). Nessuna corruzione.
