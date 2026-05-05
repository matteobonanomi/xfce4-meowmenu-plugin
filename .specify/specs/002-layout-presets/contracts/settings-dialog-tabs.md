# Contract: Settings Dialog tab reorganization

Dopo la milestone, il `GtkNotebook` di [panel-plugin/settings-dialog.cpp](../../../../panel-plugin/settings-dialog.cpp) ~ riga 148 ha 6 schede:

| # | Tab label | Stato post-milestone | Funzione `init_*_tab()` |
|---|---|---|---|
| 0 | `_General` | **RISCRITTA** — diventa hub preset | `init_general_tab()` (riscritta) |
| 1 | `_Appearance` | **ESTESA** — aggiunge nuovi controlli granulari | `init_appearance_tab()` (esistente, esteso) |
| 2 | `_Behavior` | **ESTESA** — aggiunge nuovi controlli granulari | `init_behavior_tab()` (esistente, esteso) |
| 3 | `_Commands` | invariata | `init_commands_tab()` |
| 4 | `Search Actio_ns` | invariata | `init_search_actions_tab()` |
| 5 | `_Advanced Search` | (eredita milestone 001) | – |

Numero totale di schede invariato (6).

---

## Tab 0 — General (hub preset, riscritta)

**Scopo**: punto unico di ingresso per la gestione dei preset.

**Layout dall'alto in basso**:

1. `GtkComboBox` "**Layout preset**" — popolato con i 3 built-in + tutti i preset utente trovati sotto `/presets/`. Selezione applica il preset immediatamente.
2. Etichetta descrittiva del preset corrente (1–2 righe). Tradotta via `_()`.
3. Indicatore "**Customized**" (visibile solo se `current_preset_dirty` è true).
4. Pulsantiera orizzontale:
   - **Save as new preset…** — apre prompt per nome, valida, crea sotto-albero `/presets/<uuid>/`.
   - **Reset preset** — riapplica i valori del preset selezionato (cancella le personalizzazioni).
   - **Rename…** — abilitato solo per preset utente.
   - **Delete** — abilitato solo per preset utente. Conferma esplicita.
   - **Export…** — apre `GtkFileChooserNative` save mode, suggerisce nome `<display-name>.meowpreset`.
   - **Import…** — apre `GtkFileChooserNative` open mode, filtra `.meowpreset`.
5. **Reset to defaults** — pulsante separato in basso, conferma esplicita. Cancella tutto il channel del plugin (eccetto preset utente) e applica Modern.

**Wayland banner**: se `gtk_layer_is_supported()` è false e l'utente seleziona FullScreen, mostrare un `GtkInfoBar` con l'avviso descritto in `research.md` D7.

**Non più presenti in General** (spostati): view-mode, dimensione icone, dimensioni menu, opacità singola.

---

## Tab 1 — Appearance (estesa)

**Conserva** (esistenti, vedi survey codebase):
- View mode (icons / list / tree) — settings-dialog.cpp ~ riga 384–483
- Launcher icon size — ~ riga 546–562
- Category icon size — ~ riga 569–591
- Menu width / height — ~ riga 603–632
- Position categories (alternate side) — ~ riga 682–706
- Position profile / search / commands — ~ riga 708–739
- Profile shape — ~ riga 742–760
- Panel button style / title / icon — ~ riga 774–846

**Aggiunge** (nuovi):
- Corner radius (spinbox, 0–24 px)
- Categories opacity (slider, 0–100 %)
- Apps & search opacity (slider, 0–100 %)
- Sidebar position (combo: left / right / hidden)
- Search bar position (combo: top / bottom)
- Profile position (combo: top / bottom / bottom-right / hidden)
- Commands position (combo: top-right / bottom-right / hidden)

**Sostituisce**:
- L'attuale slider singolo "Background opacity" (settings-dialog.cpp ~ riga 639–654) viene **rimosso**: la sua funzione è coperta dai due nuovi slider (categorie + apps). La proprietà legacy `menu-opacity` viene mappata in fase di migrazione su `categories-opacity` per non perdere la preferenza dell'utente.

**Nota Position categories**: l'opzione esistente "alternate side" (~ riga 682–706) viene riconciliata con la nuova `sidebar-position`. Nuova UI: una sola combobox a 3 valori invece di un toggle.

---

## Tab 2 — Behavior (estesa)

**Conserva** (esistenti):
- Default category — ~ riga 866–918
- Hover-switch category — ~ riga 927–946
- Stay on focus-out — ~ riga ...

**Aggiunge** (nuovi):
- Panel gap (spinbox, 0–50 px) — il distacco dal pannello.
- Layout mode (combo: docked / fullscreen) — selezione macro alternativa al preset selector.
- Grid columns (spinbox, 2–10)
- Grid rows (spinbox, 1–8) — visibile solo in layout-mode = fullscreen.
- Grid density (combo: low / medium / high)

**Sezione "FullScreen-specific"** (visibile solo se `layout-mode == fullscreen` o se preset corrente è FullScreen): collapsible group che raggruppa grid-columns/rows/density.

---

## Coerenza con la spec

- **FR-019 (General come hub preset)**: ✅ tab 0 ridisegnata.
- **FR-020 (nessuna impostazione resa irraggiungibile)**: ✅ nessun campo esistente è rimosso senza sostituto. `menu-opacity` legacy viene migrato.
- **FR-021 (settings di ricerca in Advanced Search)**: ✅ già fatto da milestone 001.
- **FR-022 (struttura coerente e intuitiva)**: ✅ ogni nuovo controllo è collocato per area semantica (visivo → Appearance, comportamento/spaziatura → Behavior).

## Verifica manuale

In [quickstart.md](../quickstart.md): per ciascuna scheda, una checklist di controlli da vedere e da provare.
