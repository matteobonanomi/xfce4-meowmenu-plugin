# Implementation Plan: Layout Presets

**Branch**: `chore/spec-002-presets` (spec/plan); l'implementazione userà `feature/002-layout-presets` | **Date**: 2026-05-05 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `.specify/specs/002-layout-presets/spec.md`

## Summary

Introdurre tre preset built-in (Classic, Modern, FullScreen) come **macro** su un insieme di impostazioni individuali — molte delle quali non esistono ancora nel codice upstream — esposte come controlli granulari nelle schede di dettaglio. Modern è il default su installazione fresca. Gli utenti possono salvare la configurazione attiva come preset utente, esportarlo/importarlo via file, riorganizzare il pannello impostazioni intorno alla nuova scheda **General** (gestione preset).

**Approccio tecnico**: i tre preset built-in vivono come **costanti C++** (un array di mappe `{property → value}`) compilate dentro il modulo. I preset utente vivono in **Xfconf** sotto `/presets/<id>/`. L'applicazione di un preset è una scrittura batch sulle proprietà Xfconf esistenti — nessuna logica preset-specifica nel rendering. Il rendering legge solo Xfconf come oggi. Le nuove impostazioni (curvatura angoli, doppia opacità, distacco dal pannello, griglia, posizioni elementi) sono nuovi `Boolean`/`Integer` nella classe `WhiskerMenu::Settings` ([panel-plugin/settings.h](../../../panel-plugin/settings.h)) e nuovi controlli nella `SettingsDialog` ([panel-plugin/settings-dialog.cpp](../../../panel-plugin/settings-dialog.cpp)). Il `Window::on_draw_event()` ([panel-plugin/window.cpp](../../../panel-plugin/window.cpp) ~ riga 934) viene esteso per la curvatura/doppia opacità via cairo; `move_window()` (~ riga 1029) per il distacco; `LauncherIconView::reload_icon_size()` ([panel-plugin/launcher-icon-view.cpp](../../../panel-plugin/launcher-icon-view.cpp) ~ riga 223) per le colonne/densità della griglia.

## Technical Context

**Language/Version**: C++ (gnu++11), come il resto di `panel-plugin/`.
**Primary Dependencies**: GTK 3, cairo (già pulled-in da GTK), Garcon, libxfce4panel, libxfce4ui, libxfce4util, xfconf. Optional: gtk-layer-shell (per Wayland), accountsservice (per profilo). Nessuna nuova dipendenza.
**Storage**: Xfconf (channel `xfce4-panel`, plugin namespace `/plugins/whiskermenu-N/...`). I preset utente vivono in Xfconf sotto un sotto-albero dedicato. Il file di export/import è un formato testuale auto-contenuto (vedi [contracts/preset-file-format.md](contracts/preset-file-format.md)).
**Testing**: Meson (`meson test -C build`). La codebase upstream non ha test C++ unitari nel `panel-plugin/`. Vedi sezione "Test strategy" più sotto: questa milestone introdurrà i primi test unitari per le funzioni *pure* (mappa preset, validatori nomi, parser file).
**Target Platform**: Xubuntu 26.04, Xfce 4.20.x, X11 primary; Wayland fallback con classificazione per-feature (vedi sotto).
**Project Type**: Xfce panel plugin (desktop-app, native shared module).
**Performance Goals**: Apertura del menu non DEVE peggiorare rispetto a Whisker upstream. L'applicazione di un preset (scrittura batch Xfconf + reload menu) DEVE completarsi entro la prossima riapertura del menu (target percepito: invisibile all'utente).
**Constraints**:
- No daemon (Constitution III).
- Xfconf è l'unico storage user-state (Constitution IV).
- Garcon resta source of truth applicazioni (Constitution VI).
- Tutte le nuove stringhe traducibili via `_()` + aggiornamento `po/`.
- I tre preset built-in compilati nel binario, non in file esterni: non possono essere persi né corrotti.
**Scale/Scope**: ≤ 24 nuovi requisiti funzionali, ~12 nuove proprietà Xfconf, ~1 nuova scheda dialogo + revisione organizzazione delle 3 schede esistenti. Numero di preset utente: limite naturale Xfconf (decine ragionevoli).

## Constitution Check

Il template del plan ha un placeholder; sostituisco con check concreti derivati da [.specify/memory/constitution.md](../../memory/constitution.md).

### I. Spec-First Development (NON-NEGOTIABLE)
- [x] Spec approvata: `spec.md` esiste, checklist passata.
- [x] Plan in corso (questo file).
- [ ] Tasks: da generare con `/speckit-tasks` dopo l'approvazione di questo plan.

### II. Small Reviewable Patches
- [x] La feature è scomponibile in PR indipendenti (vedi sezione "Phasing" più sotto: 7 PR mappabili 1:1 con blocchi di task).
- [x] Branch naming: spec/plan su `chore/spec-002-presets`; l'implementazione partirà da `feature/002-layout-presets`, allineato con la cartella spec sotto `.specify/specs/002-layout-presets/`.

### III. Native Stack Only
- [x] C++ (gnu++11), GTK 3, Meson — solo. Nessuna dipendenza Python/web/daemon.
- [x] Nessuna dipendenza GTK 4 introdotta.
- [x] Nessuna nuova optional dep mandatoria; il modulo resta funzionante senza accountsservice/gtk-layer-shell come oggi.

### IV. Xfconf as Single Source of Truth
- [x] Tutte le nuove proprietà sotto il namespace plugin esistente (`/plugins/whiskermenu-N/...`); preset utente sotto `/plugins/whiskermenu-N/presets/<id>/...`.
- [x] Ogni nuova proprietà ha un default sicuro (vedi [data-model.md](data-model.md)).
- [x] Reset-to-default è un'operazione esplicita esposta nella scheda General.
- [x] Schema migration: nuovo Integer `schema_version`. Migrazione da v0 (Whisker upstream) a v1 (MeowMenu Layout Presets) descritta in [research.md](research.md) e [data-model.md](data-model.md).
- [x] Nessun parallel store. Il file di export/import è formato di trasporto, non storage canonico (replicato fedelmente dentro Xfconf all'import).

### V. X11 Primary, Wayland Graceful Fallback

Classificazione per requisito (estensione di `spec.md`):

- Modern, Classic, applicazione preset, personalizzazione granulare, salvataggio/rinomina/eliminazione preset utente, export/import: **X11+Wayland parity**.
- Distacco dal pannello (Modern): **X11+Wayland parity** — su X11 via `gtk_window_move`, su Wayland via `gtk_layer_set_margin` (già presente in `window.cpp` ~ riga 1039, gated da `HAVE_GTK_LAYER_SHELL`).
- FullScreen preset (finestra a tutto schermo): **X11-first with Wayland fallback** — su X11 dimensiona la finestra al monitor; su Wayland richiede gtk-layer-shell per il layer fullscreen. Se `gtk_layer_is_supported()` è falso, FullScreen mostra un avviso UI e si applica come finestra grande ma non veramente fullscreen.

### VI. Garcon for Application Discovery
- [x] Nessuna modifica al percorso di scoperta applicazioni. Garcon resta intatto. La griglia FullScreen consuma lo stesso `GtkTreeModel` di [applications-page.cpp](../../../panel-plugin/applications-page.cpp).

### VII. Optional Dependencies Degrade Cleanly
- [x] Nessuna nuova optional dep introdotta. Le degradazioni Wayland/non-layer-shell sono già coperte.

**Esito Constitution Check**: tutti i gate passano. Nessuna voce in "Complexity Tracking".

## Project Structure

### Documentation (this feature)

```text
.specify/specs/002-layout-presets/
├── spec.md              # Feature specification (input)
├── plan.md              # This file
├── research.md          # Phase 0 output — design decisions e alternative considerate
├── data-model.md        # Phase 1 output — entità preset, mappa Xfconf, schema migration
├── contracts/
│   ├── xfconf-schema.md         # Nuove proprietà Xfconf, namespace, default
│   ├── preset-file-format.md    # Formato file di export/import
│   └── settings-dialog-tabs.md  # Riorganizzazione delle schede del dialog
├── quickstart.md        # Phase 1 output — procedura manuale di verifica end-to-end
├── checklists/
│   └── requirements.md          # Spec quality checklist (esistente)
└── tasks.md             # Phase 2 output (/speckit-tasks command — NON creato qui)
```

### Source Code (repository root)

Estensioni minimali a file esistenti, nessun nuovo modulo strutturale. La feature *non* introduce nuovi file `.cpp/.h` se non per isolare la logica di preset (file nuovo `preset.cpp/h`) e di import/export (file nuovo `preset-io.cpp/h`).

```text
panel-plugin/
├── settings.h                    # MODIFY — aggiungere ~12 nuovi Boolean/Integer
├── settings.cpp                  # MODIFY — load/save dei nuovi field, schema_version
├── settings-dialog.cpp           # MODIFY — riorganizzare General, estendere
│                                 #          Appearance/Behavior con nuovi controlli
├── settings-dialog.h             # MODIFY — eventuali nuovi metodi init_*_tab
├── window.cpp                    # MODIFY — on_draw_event (corner radius +
│                                 #          dual-opacity), move_window (panel_gap),
│                                 #          eventuale fullscreen path
├── launcher-icon-view.cpp        # MODIFY — gtk_icon_view_set_columns +
│                                 #          item_padding/spacing per densità
├── applications-page.cpp         # MODIFY — branca FullScreen layout
├── plugin.cpp                    # MODIFY — applicare preset di default su fresh install
├── preset.h                      # NEW — struct LayoutPreset, BUILTIN_PRESETS array,
│                                 #       apply/diff/load_from_settings
├── preset.cpp                    # NEW — implementazione + costanti dei tre built-in
├── preset-io.h                   # NEW — interfaccia export/import su file
├── preset-io.cpp                 # NEW — serializer/parser formato preset
└── meson.build                   # MODIFY — aggiungere preset.cpp e preset-io.cpp
                                  #          all'array plugin_sources

po/
└── *.po                          # MODIFY — nuove stringhe traducibili
                                  #          (gestito automaticamente da xgettext;
                                  #           solo verifica)

tests/                            # NEW — primo bundle di unit test C++
├── meson.build                   # NEW
├── test_preset.cpp               # NEW — test apply/diff/round-trip
└── test_preset_io.cpp            # NEW — test parser/serializer + file corrotti
```

**Structure Decision**: estensione del progetto esistente, nessuna nuova top-level directory salvo `tests/` per i primi unit test. La logica preset è isolata in due nuovi moduli (`preset` per i dati, `preset-io` per il formato file) per mantenere `settings.cpp` e `settings-dialog.cpp` leggibili e per poter testare in unità funzioni pure senza X11/GTK.

## Phasing

I task generati in Phase 2 si raggruppano in 7 PR indipendenti, in quest'ordine:

1. **Schema & nuove proprietà** — aggiungere i nuovi field in `settings.{h,cpp}`, aggiungere `schema_version`, scrivere la migrazione v0→v1. Nessun cambio UI; nessun cambio rendering. Test: la build esistente continua a funzionare; le nuove proprietà appaiono in `xfconf-query`.
2. **Modulo preset (built-in only)** — creare `preset.{h,cpp}`, definire i tre preset built-in come costanti, esporre `apply_preset_by_name()`, `current_preset_id()`, `current_preset_dirty()`. Nessun cambio UI. Test unitari sul diff e sull'apply.
3. **Rendering: corner radius + dual opacity + panel gap** — estendere `window.cpp` (cairo path in `on_draw_event`, offset in `move_window`). I valori sono *letti* da Xfconf; ancora nessun preset selector nella UI. Verifica manuale modificando le proprietà via `xfconf-query`.
4. **Layout granulare: griglia colonne/densità + posizioni elementi** — estendere `launcher-icon-view.cpp` (colonne, spacing) e `window.cpp` (riposizionamento search/sidebar/profile). Verifica manuale.
5. **Riorganizzazione del dialogo (General come hub preset)** — riscrivere `init_general_tab()` per esporre il selettore preset + Save/Reset; aggiungere i nuovi controlli granulari nelle Appearance/Behavior; spostare le impostazioni search in "Advanced Search" (eredita lavoro milestone 001). Test manuale completo.
6. **Preset utente + persistenza Xfconf** — UI per Save-as / rename / delete; storage in `/presets/<id>/`. Test unitari sul namespacing e sui conflitti di nome.
7. **Export / Import file** — modulo `preset-io.{h,cpp}`, file chooser nativo, gestione errori, conflitti di nome all'import. Test unitari su formato + file corrotti.

Ogni PR include: build OK, `meson test` OK, aggiornamento `po/`, manual-test steps nel PR description.

## Test strategy

La codebase upstream Whisker non ha unit test in `panel-plugin/`. Questa milestone è l'occasione per introdurre **`tests/`** con i primi test C++:

- **Pure functions testabili a unità** (no GTK, no X server):
  - `preset.cpp::apply_to(Settings&)`, `preset.cpp::diff(const Settings&)`, `preset.cpp::find_by_name()`.
  - `preset-io.cpp::serialize(const Preset&)`, `preset-io.cpp::parse(const std::string&)`, e i casi di errore (file vuoto, chiave sconosciuta, valore fuori range).
  - Validatori di nome preset (no duplicati con built-in, no caratteri invalidi).
  - Schema migration v0→v1 (input: snapshot Xfconf v0, output: settings con default).

- **Integration test** (richiedono `xfconfd` mockato o vero — TBD in research): applicare ciascun preset built-in, leggere indietro i valori, ripristinare default. Probabilmente in `tests/integration/` con un fixture che lancia `xfconfd --replace` su un `XDG_CONFIG_HOME` temporaneo.

- **Manual UI test plan**: in [quickstart.md](quickstart.md), una checklist passo-passo per verificare ogni preset, il save/rename/delete, e l'export/import su nested Xfce session o VM Xubuntu.

Se Phase 2 (tasks) non riesce a coprire l'integration test in CI per vincoli di ambiente, il task corrispondente DEVE essere flaggato come "manual-only" e documentato.

## Complexity Tracking

> Vuoto. Nessun gate Constitution è violato; nessuna giustificazione richiesta.

## Phase 0 — Research output

Generato come [research.md](research.md). Risolve le seguenti decisioni:
- Built-in preset: array C++ vs JSON embeddata vs file di sistema. **Scelto: array C++**.
- Preset utente: Xfconf annidato vs file in `XDG_CONFIG_HOME`. **Scelto: Xfconf annidato**, per Constitution IV.
- Formato file export/import: keyfile (.desktop-style), JSON, TOML. **Scelto: keyfile** — già in uso in tutto Xfce, parser disponibile in GLib (`GKeyFile`), nessuna nuova dipendenza.
- Schema migration: campo `schema_version` Integer; migrazione lazy alla prima inizializzazione di `Settings::load()`.
- "Active preset" tracking: campo Xfconf `current_preset_id` (String). "Dirty" calcolato a runtime via diff con la definizione del preset.
- Wayland FullScreen: graceful fallback a finestra grande non-fullscreen quando layer-shell assente; banner UI esplicito.
- Direzione del distacco dal pannello: derivata automaticamente dal `panel_position` esistente, l'utente non la configura.

## Phase 1 — Design output

- [data-model.md](data-model.md) — entità Preset/UserPreset/SchemaVersion, mappa completa nuove proprietà Xfconf con default per preset, schema migration v0→v1.
- [contracts/xfconf-schema.md](contracts/xfconf-schema.md) — namespace, tipi, default, vincoli per ogni nuova proprietà.
- [contracts/preset-file-format.md](contracts/preset-file-format.md) — grammatica del file `.meowpreset`, regole versionamento, gestione chiavi sconosciute.
- [contracts/settings-dialog-tabs.md](contracts/settings-dialog-tabs.md) — riorganizzazione delle schede del dialog (cosa va dove, cosa si rinomina, cosa si sposta).
- [quickstart.md](quickstart.md) — procedura manuale di verifica end-to-end della milestone.

## Notes for /speckit-tasks

- Ogni PR del Phasing ↑ corrisponde a un blocco di task; il blocco 1 deve atterrare per primo perché tutti gli altri ne dipendono.
- I task UI hanno verifica manuale obbligatoria (nested Xfce session); marcarlo esplicitamente.
- Aggiornamento `po/` è un task per ogni PR che introduce stringhe nuove, non un singolo task finale.
- Aggiungere un task "audit i18n" alla fine per verificare che nessuna stringa nuova sia non traducibile.
