# Quickstart: verifica manuale della milestone Layout Presets

Procedura passo-passo da eseguire su nested Xfce session o VM Xubuntu 26.04 dopo aver installato il plugin di sviluppo.

## Setup

```bash
# Build & install local
meson setup build
meson compile -C build
sudo meson install -C build  # o usa --destdir per nested session

# Pulizia config (per testare scenario "fresh install")
xfconf-query -c xfce4-panel -lv | grep whiskermenu
xfconf-query -c xfce4-panel -r -R /plugins/whiskermenu-1   # adatta N
```

Lancia `xfce4-panel --replace` e assicurati che il plugin MeowMenu sia presente.

---

## T1. Modern di default su fresh install

**Cosa verifica**: SC-001, SC-002, FR-002, FR-003.

1. Resetta il channel come sopra. Apri il menu dal pannello.
2. Verifica visivamente:
   - [ ] Angoli del menu arrotondati (radius ~12 px)
   - [ ] Lieve distacco dal pannello (gap ~8 px nella direzione corretta)
   - [ ] Categorie a sinistra
   - [ ] Profilo utente in alto
   - [ ] Barra di ricerca in basso
   - [ ] Applicazioni come icone (non lista)
   - [ ] Doppia opacità: categorie 100%, apps 80% (controllabile passando un widget colorato sul desktop dietro al menu)
3. Apri impostazioni → scheda General. Verifica:
   - [ ] Preset selezionato: "Modern"
   - [ ] Indicatore "Customized" non visibile

## T2. Cambio preset

**Cosa verifica**: User Story 2, FR-004, FR-005.

1. Apri impostazioni → General → seleziona "Classic" dal dropdown.
2. Chiudi impostazioni, apri menu.
   - [ ] Menu ha aspetto Whisker upstream (no angoli, no gap, list view, search in alto, ecc.)
3. Riapri impostazioni → General → seleziona "FullScreen".
4. Apri menu.
   - [ ] Menu occupa l'intero monitor (X11) o quasi (Wayland senza layer-shell)
   - [ ] Search bar in alto al centro
   - [ ] Categorie in riga sotto la search
   - [ ] Griglia applicazioni 6×3, centrata
   - [ ] Comandi (logout/lock/shutdown) in basso a destra
5. Torna a Modern.

## T3. Personalizzazione granulare

**Cosa verifica**: User Story 3, FR-008, FR-009, FR-010.

1. Preset attivo: Modern. Apri impostazioni → Appearance.
2. Cambia "Corner radius" da 12 a 4. Chiudi e riapri menu.
   - [ ] Solo gli angoli sono cambiati; tutto il resto (gap, opacity, ecc.) invariato.
3. Apri impostazioni → General.
   - [ ] Preset ancora "Modern"
   - [ ] Indicatore "Customized" visibile
4. Clicca "Reset preset" e conferma.
   - [ ] Corner radius torna a 12
   - [ ] Indicatore "Customized" sparisce

## T4. Salvataggio preset utente

**Cosa verifica**: User Story 4, FR-011, FR-012, FR-013, FR-015.

1. Preset Modern, modifica corner-radius a 20 e panel-gap a 16.
2. Apri General → "Save as new preset…" → inserisci nome "Mio Layout".
   - [ ] Preset "Mio Layout" appare nel dropdown
   - [ ] È il preset corrente
3. Cambia preset a Classic, poi torna a "Mio Layout".
   - [ ] Corner-radius e panel-gap tornano ai valori salvati (20 e 16)
4. Tenta di rinominarlo in "Modern".
   - [ ] Errore: il nome è già usato da un built-in
5. Rinominalo in "Lavoro".
   - [ ] Appare nel dropdown col nuovo nome
6. Tenta di eliminare "Modern".
   - [ ] Pulsante Delete disabilitato (o errore esplicito)
7. Elimina "Lavoro".
   - [ ] Sparisce dal dropdown
   - [ ] Preset corrente diventa vuoto (o l'ultimo built-in) e la config attiva resta invariata

## T5. Export / import

**Cosa verifica**: User Story 5, FR-016 → FR-020.

1. Crea un preset utente "Test1" (vedi T4).
2. General → "Export…" → salva su `~/test1.meowpreset`.
   - [ ] File creato, leggibile come testo, contiene sezione `[Preset]` e `[Settings]`
3. Elimina "Test1" dal dropdown.
4. General → "Import…" → seleziona `~/test1.meowpreset`.
   - [ ] "Test1" riappare nel dropdown
   - [ ] Applicandolo, i valori coincidono con quelli originali
5. Modifica `~/test1.meowpreset` cambiando il nome in "Test1" (lasciandolo uguale) e re-importa.
   - [ ] Sistema chiede: sovrascrivi / rinomina / annulla
6. Crea `~/corrupt.meowpreset` con testo random e importa.
   - [ ] Errore parser, messaggio comprensibile, nessuna modifica al channel

## T6. Wayland fallback FullScreen

**Cosa verifica**: Constitution V, classification per FullScreen.

1. Su sessione Wayland senza `gtk-layer-shell` installato, seleziona preset FullScreen.
   - [ ] Menu si apre come finestra grande ma con decorazioni/bordi visibili
   - [ ] `GtkInfoBar` in scheda General avvisa che FullScreen su Wayland richiede gtk-layer-shell
2. Su sessione Wayland con `gtk-layer-shell` installato:
   - [ ] Menu copre tutto l'output, no decorazioni

## T7. Schema migration v0 → v1

**Cosa verifica**: D4 (research), Constitution IV.5.

1. Su un account con configurazione Whisker upstream preesistente (importata o costruita manualmente):
   ```bash
   xfconf-query -c xfce4-panel -p /plugins/whiskermenu-1/menu-opacity -s 75
   xfconf-query -c xfce4-panel -p /plugins/whiskermenu-1/launcher-show-description -s true
   ```
2. Avvia il plugin (replace panel).
   - [ ] Le proprietà preesistenti restano invariate
   - [ ] `xfconf-query -c xfce4-panel -p /plugins/whiskermenu-1/schema-version` ritorna `1`
   - [ ] `current-preset-id` è `"classic"` (non Modern, perché non era fresh)
   - [ ] `menu-opacity` legacy è migrato su `categories-opacity = 75`

## T8. Persistenza tra sessioni

1. Applica Modern, modifica corner-radius a 8, salva come "MioModern".
2. Logout / login.
3. Apri menu e impostazioni:
   - [ ] Preset "MioModern" presente
   - [ ] Valori coerenti
   - [ ] Menu mostra corner-radius 8

---

## Test automatici

Eseguibili da CI:

```bash
meson test -C build
```

Coprono (vedi `tests/`):
- `test_preset`: apply/diff/find_by_name + edge cases.
- `test_preset_io`: serialize/parse round-trip + file corrotti / chiavi sconosciute / valori invalidi.
- `test_schema_migration`: snapshot v0 → v1, idempotenza.

I test UI (T1–T8) restano manuali finché un framework di automazione GTK testabile in CI non sia introdotto in una milestone successiva.

---

## Criteri di accettazione finali

La milestone è "done" quando:

- [ ] Tutti i test T1–T8 passano su X11 (Xubuntu 26.04).
- [ ] T1, T2, T3, T4, T5, T7, T8 passano anche su Wayland (T6 testato esplicitamente per la classificazione fallback).
- [ ] `meson test` passa.
- [ ] `meson compile` passa con `-Werror` se abilitato dal CI.
- [ ] `po/` aggiornato; no stringhe nuove non traducibili (audit gettext).
- [ ] CHANGELOG / commit history riferisce `.specify/specs/002-layout-presets/`.
