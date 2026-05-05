# Phase 0 — Research: Layout Presets

Decisioni architetturali e alternative considerate, prima di passare al design (Phase 1).

---

## D1. Storage dei preset built-in (Classic / Modern / FullScreen)

**Decision**: i tre preset built-in vivono come **costanti C++** in `panel-plugin/preset.cpp`, dentro un array `static const LayoutPreset BUILTIN_PRESETS[3]`. Ogni preset è una mappa nominale `{property_name → typed_value}`.

**Rationale**:
- Constitution III impone stack nativo C++/Meson; nessun parser esterno.
- I built-in NON DEVONO essere modificabili né eliminabili (FR-013): compilati nel binario, sono immutabili per costruzione.
- Permette refactor type-safe (la mappa property→default può essere validata a compile time contro lo schema in `settings.h`).
- Costo: ogni cambio ai built-in richiede ricompilazione. Accettabile: cambiare un built-in è una decisione di prodotto, non un'operazione runtime.

**Alternatives considered**:
- *JSON embeddato come stringa*: aggiunge un parser runtime e un punto di fallimento (typo nel JSON = crash all'avvio).
- *File `.meowpreset` di sistema in `/usr/share/meowmenu/presets/`*: rischia che un packager modifichi/cancelli i built-in, contraddicendo FR-013. Inoltre dipende dal layout filesystem del distro.
- *Xfconf channel di sistema con default-property fallback*: tecnicamente possibile ma confuso da debuggare e da migrare.

---

## D2. Storage dei preset utente

**Decision**: Xfconf annidato sotto `/plugins/whiskermenu-N/presets/<id>/`, dove `<id>` è uno slug derivato dal nome utente (lower-snake-case con dedupe). Per ogni preset utente, le sue proprietà sono salvate sotto questo sotto-albero con gli stessi nomi delle proprietà runtime.

**Rationale**:
- Constitution IV.1: Xfconf è single source of truth, NO parallel store.
- Esistono già pattern Xfconf per liste/array nella codebase (vedi `search-actions` in `settings.cpp`).
- `xfconf-query` è un canale di debug per power user senza terminale-only feature.

**Alternatives considered**:
- *File `~/.config/meowmenu/presets/*.meowpreset`*: violerebbe Constitution IV.1.
- *SQLite locale*: violerebbe Constitution IV.1 e introdurrebbe una dipendenza nuova non giustificata.

**Note implementative**:
- `<id>` deve essere stabile rispetto al rename: nuovo nome → nuovo id sarebbe rotto perché il preset attivo è tracked by id. Quindi: `<id>` generato al momento della creazione (UUID compatto, es. base32 di 8 byte casuali) e MAI cambiato. Il `display_name` è una proprietà nel sotto-albero e può cambiare.

---

## D3. Formato file di export/import

**Decision**: file con estensione `.meowpreset`, formato **keyfile** (stile `.desktop`/`.ini`), parsabile con GLib `GKeyFile`.

**Struttura**:
```
[Preset]
Name=Mio Layout
SchemaVersion=1
CreatedBy=meowmenu-X.Y.Z

[Settings]
corner-radius=12
panel-gap=8
categories-opacity=100
apps-opacity=80
... (tutti i campi che il preset definisce)
```

**Rationale**:
- `GKeyFile` è già in GLib (zero nuove dipendenze).
- È il formato file XDG/Xfce per eccellenza: leggibile a mano, editabile in caso di emergenza.
- Type coercion semplice: tutti i nostri valori sono `bool`/`int`/`string`.

**Alternatives considered**:
- *JSON*: richiederebbe `json-glib` (dipendenza nuova) o un parser embeddato.
- *TOML*: nessun parser standard in GLib; introduce una dipendenza.
- *XML*: verboso, non c'è ragione per preferirlo a keyfile.

**Versionamento**: la chiave `SchemaVersion` permette migrazione di file vecchi su installazioni future. Importer ignora chiavi sconosciute e logga un warning.

---

## D4. Schema migration

**Decision**: nuovo Integer `schema_version` in Xfconf, default `0` se assente (Whisker upstream). Migrazione lazy nel costruttore di `Settings`:

- v0 → v1: leggere tutte le proprietà esistenti come prima; scrivere i default delle nuove proprietà; settare `schema_version = 1`. Se il channel era completamente vuoto (nuova installazione, nessun valore Whisker preesistente), settare `current_preset_id = "modern"` e applicare i valori del preset Modern; altrimenti settare `current_preset_id = "classic"` (il comportamento utente non cambia).

**Rationale**:
- Constitution IV.5: gli utenti esistenti devono finire con default equivalenti o migliori dopo l'upgrade.
- Detection "fresh install" via assenza completa di proprietà nel channel evita di sovrascrivere config esistenti.

**Detection "fresh install"**:
- Probe: il channel ha *zero* proprietà sotto il namespace plugin (verificato con `xfconf_channel_get_properties()`)? Allora è fresh.
- Edge case: utente che ha resettato Whisker manualmente. Trattato come fresh — accettabile.

**Alternatives considered**:
- *Migrazione eager* in un binario di upgrade dedicato: troppo complesso per una single-binary plugin.
- *Sentinel file* in `XDG_CONFIG_HOME`: violerebbe Constitution IV.

---

## D5. Tracking del preset attivo + stato "dirty"

**Decision**:
- Proprietà Xfconf `current_preset_id` (String). Valori: `"classic"`, `"modern"`, `"fullscreen"`, `"<uuid>"` per i preset utente, `""` (vuoto) se l'utente ha applicato un reset totale e non c'è preset di riferimento.
- Lo stato "dirty" (configurazione attiva diverge dal preset) è **calcolato a runtime** confrontando i valori live con la definizione del preset corrente.

**Rationale**:
- Meno stato persistito = meno punti di drift.
- L'indicatore "personalizzato" (FR-009) viene aggiornato a ogni modifica.
- Le funzioni di diff sono pure → testabili a unità.

**Alternatives considered**:
- *Booleano persistito `current_preset_dirty`*: rischio di drift (es. se l'utente modifica una proprietà via `xfconf-query` direttamente).
- *Snapshot completo dei valori applicati* per confronto: ridondante, lo abbiamo già nella definizione del preset.

---

## D6. Riorganizzazione del settings dialog

**Decision**: estendere il `GtkNotebook` esistente. La scheda General viene **riscritta** come hub preset; Appearance/Behavior conservano la struttura ma ricevono nuovi controlli; le impostazioni di ricerca migrano nella scheda Advanced Search (eredita milestone 001).

**Tab dopo la milestone**:
1. **General** — riscritta: dropdown preset, descrizione preset selezionato, pulsanti Save-as / Rename / Delete / Reset / Export / Import. Indicatore "Customized" se dirty.
2. **Appearance** — estesa: aggiunge corner radius, dual opacity, sidebar position, search bar position, profile position. Conserva i campi esistenti (icone, dimensioni, ecc.).
3. **Behavior** — estesa: aggiunge gap dal pannello, hover-switch (già presente), grid columns / rows / density, layout mode (docked/fullscreen).
4. **Commands** — invariata.
5. **Search Actions** — invariata.
6. **Advanced Search** — già eredita le impostazioni di ricerca dalla milestone 001.

**Rationale**:
- FR-022: nessuna impostazione esistente DEVE essere resa irraggiungibile.
- Riusa il `GtkNotebook` esistente (settings-dialog.cpp ~ riga 148): zero refactor strutturale.
- Le scelte di esatta posizione tra Appearance/Behavior sono dettagliate in [contracts/settings-dialog-tabs.md](contracts/settings-dialog-tabs.md).

**Alternatives considered**:
- *Una nuova tab "Layout" separata*: aumenta il numero di tab e crea ambiguità con Appearance.
- *Sidebar a sinistra invece di tabs*: refactor strutturale non giustificato dalla milestone.

---

## D7. Wayland: comportamento FullScreen

**Decision**: il preset FullScreen è classificato come **X11-first with Wayland fallback**.

- Se `gtk_layer_is_supported()` è true: la finestra del menu usa `gtk_layer_set_anchor()` su tutti e quattro i lati per coprire l'intero output Wayland; nessun cambiamento visibile rispetto a X11.
- Se non lo è (o se `HAVE_GTK_LAYER_SHELL` non è compilato): la finestra viene dimensionata al monitor corrente come finestra normale; la decorazione del compositor potrebbe essere visibile. Mostriamo un `GtkInfoBar` non bloccante nella scheda General quando l'utente seleziona FullScreen su Wayland senza layer-shell, con messaggio: *"FullScreen mode requires gtk-layer-shell on Wayland. Some appearance features may be limited."*

**Rationale**:
- Coerente con il pattern già in uso in `window.cpp` (~ riga 1039 per i margin).
- Constitution V.3: feature non disponibili su Wayland devono avere un hint UI esplicito.

---

## D8. Direzione del distacco dal pannello (panel gap)

**Decision**: l'utente configura **un solo numero** (`panel_gap`, Integer, 0–50 px). La direzione è derivata automaticamente dalla posizione del pannello (top/bottom/left/right), già nota a `Window::move_window()` via `m_plugin->get_panel_position()`.

**Rationale**:
- Spec FR-010a (categoria "Geometria e posizionamento"): l'utente non configura la direzione, è auto-calcolata. UX più semplice, meno gradi di libertà.
- L'unica decisione utente è "quanto staccato": un singolo spinbox.

---

## D9. Strategia di test

**Decision**: introdurre `tests/` come directory top-level con un `meson.build` che dichiara unit test in C++ usando il testing framework minimale già supportato da Meson (`test()` con eseguibili C++ standalone, no GoogleTest/Catch2 a meno che già presenti).

**Rationale**:
- Constitution: "Se `meson test` reports zero tests for a touched area, flag it." Questa milestone tocca codice nuovo *e* logica isolabile in funzioni pure → primo bundle di test fattibile e dovuto.
- Niente nuove dipendenze: il framework è "main() ritorna 0 = pass".

**Alternatives considered**:
- *Aggiungere GoogleTest/Catch2*: nuova dipendenza, da valutare in milestone successive se i test crescono.
- *Saltare i test*: violerebbe la constitution.

---

## D10. Convenzione branch / cartelle Spec-Kit

**Decision**: la cartella della spec è `.specify/specs/002-layout-presets/`. Il lavoro di spec/plan/tasks vive sul branch `chore/spec-002-presets`. L'implementazione partirà da un nuovo branch `feature/002-layout-presets`, in linea con la convenzione `feature/NNN-short-slug` (Constitution / CLAUDE.md §3.1).

**Action**: nei tasks e nei commit message, usa il path della cartella spec (`.specify/specs/002-layout-presets`) come riferimento canonico.
