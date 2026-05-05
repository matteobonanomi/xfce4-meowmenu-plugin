# Phase 1 — Data Model: Layout Presets

Mappa concreta tra le entità della spec (`spec.md` §"Key Entities") e l'implementazione Xfconf + C++.

---

## Entità

### LayoutPreset (built-in)

Costante C++ in `panel-plugin/preset.cpp`.

```text
struct LayoutPreset {
    const char*  id;          // "classic" | "modern" | "fullscreen"
    const char*  display_name; // tradotto via _()
    const char*  description;  // tradotto via _()
    bool         is_builtin;   // sempre true per questo array
    PresetValueMap values;     // {property_name → typed_value}
};

static const LayoutPreset BUILTIN_PRESETS[3] = { CLASSIC, MODERN, FULLSCREEN };
```

`PresetValueMap` è un `std::map<std::string, PresetValue>` dove `PresetValue` è una variante (`bool | int | std::string`).

### UserPreset

Salvato in Xfconf sotto `/plugins/whiskermenu-N/presets/<uuid>/`.

| Proprietà sotto-albero | Tipo | Descrizione |
|---|---|---|
| `display-name` | String | Nome mostrato in UI. Univoco tra tutti i preset (built-in + user). |
| `created-by` | String | Versione MeowMenu al momento della creazione (debug only). |
| `<property>` | Boolean / Integer / String | Una proprietà per ogni campo che il preset memorizza. Stessi nomi delle proprietà runtime (vedi `contracts/xfconf-schema.md`). |

`<uuid>` è uno slug stabile (8 byte casuali in base32, lower-case) generato alla creazione. Non cambia mai.

### Configurazione attiva (Settings)

La classe `WhiskerMenu::Settings` ([panel-plugin/settings.h](../../../panel-plugin/settings.h)) viene estesa con i nuovi field. Vedi `contracts/xfconf-schema.md` per l'elenco esaustivo.

### SchemaVersion

Singola proprietà Xfconf `schema_version` (Integer). Default `0` (assente = installazione Whisker upstream o pre-MeowMenu). Valore corrente di questa milestone: `1`.

---

## Mappa proprietà ↔ preset

Tabella: per ogni nuova proprietà, il valore che ciascun built-in assegna. `(=)` significa "non sovrascritto dal preset" (eredita la configurazione attiva o il default Xfconf).

| Proprietà | Tipo | Range | Default Xfconf | Classic | Modern | FullScreen |
|---|---|---|---|---|---|---|
| `schema-version` | Integer | ≥0 | 0 | – | – | – |
| `current-preset-id` | String | – | `""` | `"classic"` | `"modern"` | `"fullscreen"` |
| `corner-radius` | Integer | 0–24 px | 0 | 0 | 12 | 0 |
| `panel-gap` | Integer | 0–50 px | 0 | 0 | 8 | 0 |
| `categories-opacity` | Integer | 0–100 % | 100 | 100 | 100 | 100 |
| `apps-opacity` | Integer | 0–100 % | 100 | 100 | 80 | 100 |
| `sidebar-position` | String | `left` \| `right` \| `hidden` | `left` | `left` | `left` | `hidden` |
| `search-bar-position` | String | `top` \| `bottom` | `top` | `top` | `bottom` | `top` |
| `profile-position` | String | `top` \| `bottom` \| `hidden` | `top` | `top` | `top` | `bottom-right` |
| `grid-columns` | Integer | 2–10 | 4 | (=) | (=) | 6 |
| `grid-rows` | Integer | 1–8 | 3 | (=) | (=) | 3 |
| `grid-density` | String | `low` \| `medium` \| `high` | `medium` | (=) | `medium` | `medium` |
| `layout-mode` | String | `docked` \| `fullscreen` | `docked` | `docked` | `docked` | `fullscreen` |
| `hover-switch-category` | Boolean | – | false | false | true | true |
| `view-mode-default` | String | `icons` \| `list` \| `tree` | `list` | `list` | `icons` | `icons` |
| `commands-position` | String | `bottom-right` \| `top-right` \| `hidden` | `top-right` | (=) | (=) | `bottom-right` |

**Nota**: `view-mode-default` e `hover-switch-category` esistono già in upstream Whisker (vedi `settings.h`); il preset li sovrascrive. Le altre 13 sono nuove proprietà introdotte da questa milestone.

**Nota su `profile-position`**: i tre valori top/bottom/hidden non bastano per FullScreen, che vuole il profilo in basso a destra. Estendiamo il dominio a `bottom-right` come quarto valore. Documentato in `contracts/xfconf-schema.md`.

---

## Schema migration (v0 → v1)

**Trigger**: in `Settings` constructor, dopo `xfconf_channel_get_properties()`, leggi `schema_version`. Se < 1, esegui la migrazione.

**Algoritmo**:

```text
read all_properties from channel under plugin namespace
read schema_version (default 0 if absent)

if schema_version >= 1:
    return  # already migrated

is_fresh_install = (count(all_properties under plugin namespace, excluding xfce4-panel
                          system properties) == 0)

for each new_property in NEW_PROPERTIES_V1:
    if new_property not in all_properties:
        write new_property = DEFAULT_V1[new_property]

if is_fresh_install:
    apply_preset("modern")  # writes the Modern values for all new properties
    set current_preset_id = "modern"
else:
    set current_preset_id = "classic"
    # do NOT overwrite any existing user value — Classic = Whisker upstream
    # defaults, which match what's already in the channel by definition

set schema_version = 1
```

**Idempotenza**: la migrazione è idempotente. Eseguirla due volte produce lo stesso risultato.

**Reversibilità**: nessuna migrazione automatica v1 → v0. Un utente che vuole tornare a Whisker upstream può rimuovere il channel manualmente; le nuove proprietà sono ignorate da upstream.

---

## State transitions: preset corrente

Un preset corrente può trovarsi in uno di tre stati:

```text
[clean]  current_preset_id = X, e tutti i valori live coincidono con BUILTIN_PRESETS[X]
         (o UserPresets[X] se utente).
   |
   | utente modifica una qualunque proprietà governata dal preset
   v
[dirty]  current_preset_id = X, ma almeno una proprietà differisce.
         UI mostra indicatore "Customized".
   |
   |---> utente clicca "Reset preset"  ────────────────> [clean] (riscrive i valori)
   |
   |---> utente seleziona altro preset Y ──────────────> [clean] su Y
   |
   |---> utente clicca "Save as new preset" ──────────> [clean] su nuovo UserPreset
```

Lo stato `dirty` è **calcolato a runtime** comparando i valori live al preset; non è persistito.

---

## Vincoli di integrità

1. `current_preset_id` DEVE essere o vuoto, o uno degli id built-in, o un id presente in `/presets/`.
2. Un preset utente non può avere `display-name` uguale a un built-in né a un altro preset utente.
3. `<uuid>` di un preset utente è immutabile dopo la creazione.
4. Eliminare un preset utente che è anche `current_preset_id` setta `current_preset_id = ""` (la configurazione attiva resta invariata, viene solo "scollegata" dal preset).
5. I valori delle proprietà DEVONO sempre rispettare il range definito in `contracts/xfconf-schema.md`. Le scritture batch (apply preset) DEVONO validare prima di scrivere.

---

## Vincoli di performance

- L'apply di un preset = N scritture Xfconf (N ≤ 16). Va eseguito in batch dentro un `xfconf_channel_set_properties()` se possibile, o sequenzialmente con un solo `reload_menu()` finale (non uno per proprietà).
- Il diff (clean/dirty) viene ricalcolato solo all'apertura della scheda General o al cambio di una proprietà governata: niente polling.
