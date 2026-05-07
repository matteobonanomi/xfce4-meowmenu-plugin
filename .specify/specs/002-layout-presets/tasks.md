---
description: "Task list for milestone 002 — Layout Presets"
---

# Tasks: Layout Presets (milestone 002)

**Input**: design documents in [.specify/specs/002-layout-presets/](.)
**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/)

**Tests**: SÌ (richiesti). La constitution impone test e l'implementazione introduce funzioni *pure* (mappe preset, validatori nomi, parser file) facili da unit-testare. UI test restano manuali via [quickstart.md](quickstart.md).

**Organization**: i task sono raggruppati per fase implementativa e, dove rilevante, etichettati con la user story di [spec.md](spec.md) (US1–US5). Ogni fase si chiude con un checkpoint che mappa 1:1 con un PR sul branch `feature/002-layout-presets`.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: può essere parallelizzato (file diversi, no dipendenze)
- **[Story]**: user story di riferimento (US1, US2, US3, US4, US5)
- I path sono assoluti rispetto alla repo root

## Convenzioni di branch & commit

- Branch implementativo: `feature/002-layout-presets` (creato a partire da `main` quando questo plan è approvato).
- Commit message: imperativo, riferimento alla cartella spec. Esempio: `presets: add corner-radius runtime property (.specify/specs/002-layout-presets)`.
- Ogni checkpoint = un PR autocontenuto. Niente PR che attraversa più checkpoint.

---

## Phase 1: Setup (infrastruttura condivisa)

**Scopo**: rendere il repo pronto a ricevere nuove proprietà Xfconf, nuovi file sorgenti e i primi test unitari.

- [x] **T001** Crea la directory [tests/](../../../tests/) e aggiungi `tests/meson.build` con un singolo target `test_smoke` (un `int main(){return 0;}`) per validare che l'infrastruttura test compili. Aggiorna [meson.build](../../../meson.build) (root) per includere `subdir('tests')` dietro un'opzione `enable_tests=true` (default true).
- [x] **T002** [P] Aggiungi `panel-plugin/preset.h` e `panel-plugin/preset.cpp` come scaffold (header guard, namespace `WhiskerMenu`, file vuoti tranne forward declarations). Includili in `panel-plugin/meson.build` nel `plugin_sources`.
- [x] **T003** [P] Aggiungi `panel-plugin/preset-io.h` e `panel-plugin/preset-io.cpp` come scaffold (per export/import). Includili in `panel-plugin/meson.build`.
- [x] **T004** Verifica che `meson setup build && meson compile -C build && meson test -C build` passi con questo scaffolding. Documenta nel PR.

**Checkpoint / PR-1**: scaffolding di build & test in piedi, nessun cambio funzionale visibile all'utente.

---

## Phase 2: Foundational (prerequisiti bloccanti)

**Scopo**: schema delle proprietà Xfconf esteso, migrazione v0→v1, struttura `LayoutPreset`. Tutto ciò che serve PRIMA di poter applicare qualsiasi preset.

⚠️ **CRITICAL**: nessuna user story può iniziare prima del completamento di questa fase.

### Schema Xfconf & migrazione

- [x] **T010** Estendi [panel-plugin/settings.h](../../../panel-plugin/settings.h) con i nuovi field elencati in [contracts/xfconf-schema.md](contracts/xfconf-schema.md): `schema_version`, `current_preset_id`, `corner_radius`, `panel_gap`, `categories_opacity`, `apps_opacity`, `sidebar_position`, `search_bar_position`, `profile_position`, `commands_position`, `grid_columns`, `grid_rows`, `grid_density`, `layout_mode`. Usa i wrapper `Boolean`/`Integer`/`String` esistenti. Default e range come da contract.
- [x] **T011** Estendi `Settings::load()` in [panel-plugin/settings.cpp](../../../panel-plugin/settings.cpp) per leggere ogni nuova proprietà dal channel; `Settings::save()` per scriverle. Mantieni il pattern esistente.
- [x] **T012** Aggiungi `Settings::migrate_schema()` in [panel-plugin/settings.cpp](../../../panel-plugin/settings.cpp) chiamato dal costruttore dopo `xfconf_channel_get_properties()`. Implementa l'algoritmo descritto in [data-model.md](data-model.md) §"Schema migration". Includi: detection fresh-install, mapping `menu-opacity` legacy → `categories-opacity`, scrittura finale `schema_version=1`. **NON** applicare ancora il preset Modern (lo farà T030 una volta che `apply_preset()` esiste); per ora lascia un TODO esplicito + scrivi i default V1 e `current_preset_id`.
- [x] **T013** [P] Aggiungi `tests/test_schema_migration.cpp`: snapshot v0 → v1 (tabella di proprietà di partenza), idempotenza (eseguire due volte → stesso risultato), preservazione dei valori esistenti, mapping `menu-opacity → categories-opacity`. Registra il binario in `tests/meson.build`.

### Struttura dati preset

- [x] **T020** Implementa in [panel-plugin/preset.h](../../../panel-plugin/preset.h) la struttura `LayoutPreset` e il tipo `PresetValueMap` (`std::map<std::string, PresetValue>`, dove `PresetValue` è `std::variant<bool, int, std::string>` o equivalente compatibile gnu++11). Non includere logica di apply qui: solo dati.
- [x] **T021** [P] Definisci in [panel-plugin/preset.cpp](../../../panel-plugin/preset.cpp) `BUILTIN_PRESETS[3]` con i tre preset Classic, Modern, FullScreen, valori esatti come da [data-model.md](data-model.md) §"Mappa proprietà ↔ preset". Tradurre `display_name` e `description` con `_()`.
- [x] **T022** Implementa `apply_preset(const LayoutPreset&, Settings&)` in [panel-plugin/preset.cpp](../../../panel-plugin/preset.cpp): applica i valori del preset al `Settings` (e quindi al channel) come scrittura batch — niente reload del menu qui, lo fa il caller. Setta `current_preset_id` al termine.
- [x] **T023** Implementa `find_preset_by_id(const std::string&)` (tre built-in + lookup user via Xfconf). Per ora il ramo "user preset" può ritornare `nullptr` e verrà esteso in fase 5.
- [x] **T024** Implementa `compute_preset_diff(const LayoutPreset&, const Settings&)` → `bool` (true = dirty). Usato dalla UI per mostrare l'indicatore "Customized".
- [x] **T025** [P] Aggiungi `tests/test_preset.cpp`: per ciascun built-in, verifica che `apply_preset` scriva il numero atteso di proprietà; che `compute_preset_diff` ritorni false subito dopo apply e true dopo modifica; che `find_preset_by_id` funzioni per i tre id built-in e ritorni nullptr per id ignoto. Registra in `tests/meson.build`.

### Wiring iniziale

- [x] **T030** Modifica `Settings::migrate_schema()` (T012) rimuovendo il TODO: se fresh-install, chiama `apply_preset(MODERN, *this)` e marca `current_preset_id="modern"`; se non fresh, marca `current_preset_id="classic"` senza scrivere alcunché. Aggiorna `tests/test_schema_migration.cpp` con il caso "fresh install applica Modern".

**Checkpoint / PR-2**: schema esteso, migrazione idempotente, struttura preset definita, tre built-in caricati, test unitari di base passano. Il menu visivamente è ancora identico a Whisker upstream — le nuove proprietà sono lette ma non ancora rese.

---

## Phase 3: User Story 1 — Modern as default & US2 (P1) — Cambio preset 🎯 MVP

**Scopo**: rendere visibile l'effetto di Modern e degli altri due preset al primo apri-menu dopo l'install. Coperti SC-001, SC-002, SC-003.

**Independent Test**: vedi [quickstart.md](quickstart.md) §T1, §T2.

### Rendering delle nuove proprietà

- [x] **T040** [US1] In [panel-plugin/window.cpp](../../../panel-plugin/window.cpp) `Window::on_draw_event()` (~ riga 934) leggi `corner_radius` e disegna il path arrotondato (cairo `cairo_arc` ai 4 corner) prima del fill. Default `0` = comportamento attuale (rettangolo netto).
- [x] **T041** [US1] Sempre in `on_draw_event()`, separa l'opacity in due zone: applica `categories_opacity/100` al rettangolo della sidebar (zona categorie+profilo) e `apps_opacity/100` al rettangolo apps+search. Le zone sono determinate dall'allocazione dei child widget esistenti (`m_panel_box`); non introdurre nuovi widget.
- [x] **T042** [US1] In `Window::move_window()` ([panel-plugin/window.cpp](../../../panel-plugin/window.cpp) ~ riga 1029) leggi `panel_gap` e applica l'offset nella direzione opposta al panel (top → +Y, bottom → −Y, left → +X, right → −X). Su Wayland riusa il ramo `gtk_layer_set_margin` esistente (~ riga 1039).
- [x] **T043** [US2] In [panel-plugin/launcher-icon-view.cpp](../../../panel-plugin/launcher-icon-view.cpp) `LauncherIconView::reload_icon_size()` (~ riga 223) rendi configurabili `columns` da `grid_columns` e `item_padding`/`column_spacing` da `grid_density` (low/medium/high → 3 step di padding). Default `medium` = comportamento attuale.

### Posizionamento elementi (sidebar/search/profile/commands)

- [x] **T044** [US2] In [panel-plugin/window.cpp](../../../panel-plugin/window.cpp), nella costruzione dei `GtkBox` figli, leggi `sidebar_position` (left/right/hidden), `search_bar_position` (top/bottom), `profile_position`, `commands_position` per riordinare i child. Riconcilia con la logica esistente "alternate side" (settings-dialog.cpp ~ riga 682–706): la nuova combobox a 3 valori sostituisce il toggle. Quando una posizione è `hidden`, nascondi il widget con `gtk_widget_hide`.
- [x] **T045** [US2] Aggiungi un nuovo modo di rendering "fullscreen" attivato da `layout_mode == "fullscreen"`: la finestra si dimensiona al monitor (X11: `gtk_window_fullscreen` o resize manuale; Wayland: `gtk_layer_set_anchor` su tutti e 4 i lati se `gtk_layer_is_supported()`, altrimenti fallback a finestra grande con avviso). Documenta la classificazione in [research.md](research.md) D7.

### Hub preset minimale (General tab riscritta — versione P1)

- [x] **T050** [US1][US2] In [panel-plugin/settings-dialog.cpp](../../../panel-plugin/settings-dialog.cpp) riscrivi `init_general_tab()` (~ riga 148+) come da [contracts/settings-dialog-tabs.md](contracts/settings-dialog-tabs.md) §"Tab 0 — General": `GtkComboBox` "Layout preset" popolata coi 3 built-in, etichetta descrittiva, indicatore "Customized" (nascosto in P1, abilitato in fase 4). Rimuovi i campi spostati (view-mode, dimensione icone, dimensioni menu, opacity singola) — destinazione: Appearance tab (T060).
- [x] **T051** [US2] On-change della combobox: chiama `apply_preset(...)` + `Window::reload_menu()` (un solo reload finale, non uno per proprietà — vedi [data-model.md](data-model.md) §"Vincoli di performance").

### Riorganizzazione tab (parte minima necessaria a P1)

- [x] **T060** [US1] In [panel-plugin/settings-dialog.cpp](../../../panel-plugin/settings-dialog.cpp) sposta in `init_appearance_tab()` i controlli rimossi da General in T050 (view-mode, icon size, menu width/height). Rimuovi lo slider singolo "Background opacity" (~ riga 639–654) — la sua funzione è coperta dai due nuovi slider che arriveranno in fase 4.

**Checkpoint / PR-3 (MVP)**: fresh install mostra Modern al primo apri-menu; il dropdown General permette di switchare tra i 3 built-in con effetto immediato; T1 e T2 di [quickstart.md](quickstart.md) passano. Settings panel funziona ancora ma l'Appearance non ha ancora i nuovi controlli granulari.

---

## Phase 4: User Story 3 (P2) — Personalizzazione granulare

**Scopo**: ogni impostazione governata dai preset ha un controllo individuale; appare l'indicatore "Customized"; "Reset preset" riapplica i valori.

**Independent Test**: [quickstart.md](quickstart.md) §T3.

### Nuovi controlli in Appearance

- [x] **T070** [US3] In `init_appearance_tab()` ([panel-plugin/settings-dialog.cpp](../../../panel-plugin/settings-dialog.cpp)) aggiungi i controlli da [contracts/settings-dialog-tabs.md](contracts/settings-dialog-tabs.md) §"Tab 1 — Appearance":
  - Corner radius (`GtkSpinButton`, 0–24)
  - Categories opacity (`GtkScale`, 0–100)
  - Apps & search opacity (`GtkScale`, 0–100)
  - Sidebar position (`GtkComboBox`: left/right/hidden)
  - Search bar position (combo: top/bottom)
  - Profile position (combo: top/bottom/bottom-right/hidden)
  - Commands position (combo: top-right/bottom-right/hidden)

  Ogni controllo è bidirezionale col `Settings` e fa scattare un `Window::reload_menu()` (debounced se serve).

### Nuovi controlli in Behavior

- [x] **T071** [US3] In `init_behavior_tab()` ([panel-plugin/settings-dialog.cpp](../../../panel-plugin/settings-dialog.cpp)) aggiungi:
  - Panel gap (spinbox, 0–50)
  - Layout mode (combo: docked/fullscreen)
  - Grid columns / Grid rows / Grid density (in un gruppo collapsible visibile solo quando `layout_mode==fullscreen` o preset corrente è FullScreen)

### Indicatore "Customized" e Reset

- [x] **T072** [US3] In `init_general_tab()` mostra l'indicatore "Customized" quando `compute_preset_diff(current_preset, *settings) == true`. Riconnetti il calcolo a tutti i `notify::*` delle proprietà governate (no polling).
- [x] **T073** [US3] Aggiungi pulsante **Reset preset** in General: riapplica `apply_preset(current_preset, *settings)`, conferma esplicita via `GtkMessageDialog`. Aggiorna `Window::reload_menu()`.

**Checkpoint / PR-4**: T3 passa; ogni proprietà ha un controllo; lo stato dirty è visibile; Reset preset funziona.

---

## Phase 5: User Story 4 (P2) — Salvataggio / rinomina / eliminazione preset utente

**Scopo**: l'utente può salvare la configurazione attiva come nuovo preset, rinominarlo, eliminarlo. CRUD completo.

**Independent Test**: [quickstart.md](quickstart.md) §T4.

### Lettura/scrittura preset utente in Xfconf

- [x] **T080** [US4] Estendi [panel-plugin/preset.cpp](../../../panel-plugin/preset.cpp) con `enumerate_user_presets() → std::vector<UserPreset>`: chiama `xfconf_channel_get_properties()` sul prefisso `/presets/` e raggruppa per `<uuid>`. Filtra solo le entry con `display-name` valido.
- [x] **T081** [US4] Implementa `save_current_as_user_preset(const std::string& display_name) → uuid`: genera uuid (8 byte casuali → base32 lower-case), valida unicità del `display-name` (non collide con built-in né altri user, vincolo da [data-model.md](data-model.md) §"Vincoli di integrità"), scrive ogni proprietà governata sotto `/presets/<uuid>/`. Ritorna l'uuid.
- [x] **T082** [US4] Implementa `rename_user_preset(uuid, new_name)` e `delete_user_preset(uuid)` (quest'ultimo via `xfconf_channel_reset_property(..., recursive=TRUE)`, pattern `settings.cpp:1070,1222`). Eliminare il preset corrente setta `current_preset_id=""` (vincolo 4 di [data-model.md](data-model.md)).
- [x] **T083** Estendi `find_preset_by_id()` (T023) per il ramo user preset.
- [x] **T084** [P] Aggiorna `tests/test_preset.cpp` con casi: save → enumerate trova il nuovo, rename → display-name aggiornato, delete preset corrente → `current_preset_id` vuoto, validazione duplicati.

### UI in General

- [x] **T090** [US4] In `init_general_tab()` aggiungi i pulsanti **Save as new preset…** (apre `GtkDialog` con `GtkEntry` per il nome, valida e chiama T081), **Rename…** (abilitato solo se selected è user), **Delete** (idem, conferma esplicita).
- [x] **T091** [US4] Popola il dropdown preset con built-in + user presets (in ordine di creazione, built-in prima). Sincronizza dopo create/rename/delete (riemissione del modello).

**Checkpoint / PR-5**: T4 passa; CRUD completo; vincoli di unicità rispettati.

---

## Phase 6: User Story 5 (P3) — Export / Import `.meowpreset`

**Scopo**: serializzare un preset utente su file e re-importarlo, con validazione.

**Independent Test**: [quickstart.md](quickstart.md) §T5.

- [x] **T100** [US5] Implementa `export_user_preset(uuid, GFile* dest)` in [panel-plugin/preset-io.cpp](../../../panel-plugin/preset-io.cpp) come da [contracts/preset-file-format.md](contracts/preset-file-format.md) §"Esportazione": compone `GKeyFile` con `[Preset]` (Name, SchemaVersion, CreatedBy, ExportedAt) + `[Settings]` (una entry per ogni proprietà sotto `/presets/<uuid>/`).
- [x] **T101** [US5] Implementa `import_user_preset(GFile* src) → ImportResult` come da [contracts/preset-file-format.md](contracts/preset-file-format.md) §"Validazione all'import": parsing con `g_key_file_load_from_file`, controllo sezioni/chiavi obbligatorie, validazione per-chiave (range/dominio), gestione conflitto di nome (built-in → reject; user esistente → ritorna enum `ConflictUser` per chiamante). Su valori invalidi: warning + skip, non abort.
- [x] **T102** [P] [US5] Aggiungi `tests/test_preset_io.cpp`: round-trip serialize→parse, file corrotto (testo random), chiavi sconosciute (devono essere ignorate con log), valore fuori range (skip della entry, le altre entry vengono comunque importate), conflitto built-in (rifiutato).

### UI in General

- [x] **T110** [US5] In `init_general_tab()` aggiungi **Export…** (`GtkFileChooserNative` save mode, suggerisce `<display-name>.meowpreset`).
- [x] **T111** [US5] Aggiungi **Import…** (`GtkFileChooserNative` open mode, filtro `*.meowpreset`). Su `ConflictUser` mostra `GtkMessageDialog` con tre opzioni: sovrascrivi / rinomina / annulla.
- [x] **T112** [US5] Pulsante **Reset to defaults** in fondo al General tab: conferma esplicita, cancella tutto il channel del plugin (eccetto `/presets/`) e applica Modern.

**Checkpoint / PR-6**: T5 passa; export/import round-trip funziona; file corrotti danno errore comprensibile.

---

## Phase 7: Wayland fallback FullScreen + polish

**Scopo**: chiudere i corner case Wayland, rifinire i dettagli UI, documentazione.

**Independent Test**: [quickstart.md](quickstart.md) §T6.

- [x] **T120** [US2] In `init_general_tab()` aggiungi un `GtkInfoBar` warning (visibile solo se `!gtk_layer_is_supported() && current_preset == fullscreen`) come da [research.md](research.md) D7. Tradotto via `_()`.
- [x] **T121** [P] Aggiorna [po/](../../../po/): `meson compile -C build whiskermenu-pot && intltool-update --merge` per ogni lingua attualmente presente. Audit: nessuna nuova stringa user-facing senza `_()`.
- [x] **T122** [P] Documentazione: aggiorna [docs/whisker-modernization-spec.md](../../../docs/whisker-modernization-spec.md) §9.2 con un puntatore a `.specify/specs/002-layout-presets/` per "implementazione tracciata".
- [x] **T123** Esegui `meson setup build && meson compile -C build && meson test -C build` da pulito; tutti i test pass.
- [ ] **T124** Esegui manualmente T1–T8 di [quickstart.md](quickstart.md) su Xubuntu 26.04 X11 e Wayland; allega screenshot/note al PR.

**Checkpoint / PR-7**: tutti i criteri di accettazione finali di [quickstart.md](quickstart.md) §"Criteri di accettazione finali" sono soddisfatti.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: nessuna dipendenza, può partire subito.
- **Phase 2 (Foundational)**: dipende dalla Phase 1. **BLOCCA** ogni user story.
- **Phase 3 (US1+US2)**: dipende dalla Phase 2.
- **Phase 4 (US3)**: dipende dalla Phase 3 (i nuovi controlli toccano gli stessi tab).
- **Phase 5 (US4)**: dipende dalla Phase 3 (riusa la combobox preset). Indipendente da Phase 4 nel codice, ma per coerenza UI esce dopo.
- **Phase 6 (US5)**: dipende dalla Phase 5 (esporta preset utente).
- **Phase 7 (Polish)**: dipende da tutto il resto.

### Tasks parallelizzabili

- T002, T003 dopo T001.
- T013 in parallelo con T020–T024 (file diversi).
- T021 in parallelo con T022–T024 (T021 = solo dati; T022–T024 toccano `preset.cpp` → sequenziali tra loro).
- T084, T102 in parallelo con i task UI corrispondenti.
- T121, T122 in parallelo a fine milestone.

### Vincoli di stile e coerenza

- Ogni nuovo header rispetta lo stile dei file vicini (header guard `WHISKERMENU_*_H`, namespace `WhiskerMenu`).
- Ogni stringa user-facing nuova passa da `_()`.
- Niente `xfconf_channel_set_*()` diretto: solo via wrapper `Settings::Boolean/Integer/String`.
- Niente reformatting di file esistenti che non si stiano modificando.

---

## Implementation Strategy

### MVP (PR-1 → PR-3)

Phase 1 + Phase 2 + Phase 3. A PR-3 mergiato:
- fresh install → Modern visibile
- dropdown General switcha tra i 3 preset
- Quickstart T1 e T2 passano
- US1 e US2 (P1) sono **deliverable** anche senza Phase 4–6

### Incremental delivery

Ogni PR successivo aggiunge una user story senza rompere le precedenti. Stop a qualunque checkpoint = release intermedia possibile.

---

## Notes

- Spec e plan sono il riferimento autorevole; in caso di conflitto con questo file, aggiornare *prima* questo file via re-run di `/speckit-tasks`, non improvvisare.
- I task sono dimensionati per essere reviewabili in 30–60 min ciascuno. Se un task cresce oltre, suddividerlo prima di iniziare.
- Commit dopo ogni task o al massimo dopo un mini-gruppo coerente.
- **Niente `--no-verify`** sui commit. Niente skip di test falliti.
