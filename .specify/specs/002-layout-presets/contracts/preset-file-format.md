# Contract: Preset file format `.meowpreset`

Formato di trasporto per export / import di preset utente. **Non è uno storage canonico** (Constitution IV) — è una rappresentazione serializzata del sotto-albero Xfconf di un singolo preset utente.

## Struttura

GLib KeyFile (INI-like). Encoding UTF-8. Estensione canonica: `.meowpreset`.

```ini
[Preset]
Name=Mio Layout
SchemaVersion=1
CreatedBy=meowmenu-0.2.0
ExportedAt=2026-05-05T14:30:00Z

[Settings]
corner-radius=12
panel-gap=8
categories-opacity=100
apps-opacity=80
sidebar-position=left
search-bar-position=bottom
profile-position=top
commands-position=top-right
grid-columns=4
grid-density=medium
layout-mode=docked
hover-switch-category=true
view-mode-default=icons
```

## Sezioni

### `[Preset]` (obbligatoria)

| Key | Type | Required | Note |
|---|---|---|---|
| `Name` | string | yes | Nome di display. Importer lo userà come `display-name`. |
| `SchemaVersion` | int | yes | Versione dello schema MeowMenu al momento dell'export. |
| `CreatedBy` | string | no | Per debug. |
| `ExportedAt` | string (ISO 8601) | no | Per debug. |

### `[Settings]` (obbligatoria)

Una key per ogni proprietà che il preset memorizza. Stesso nome (kebab-case) della proprietà runtime (vedi `xfconf-schema.md`). Tipi:
- `bool`: `true` | `false`
- `int`: decimale
- `string`: testo. KeyFile gestisce escape automaticamente.

Solo le proprietà presenti in questa sezione vengono applicate; le altre restano com'erano.

## Validazione all'import

L'importer DEVE:

1. Verificare che il file sia un GKeyFile valido. Errore parser → import abortito, nessuna scrittura, messaggio utente.
2. Verificare presenza delle sezioni `[Preset]` e `[Settings]`. Mancanti → errore, abort.
3. Verificare presenza delle chiavi obbligatorie di `[Preset]`. Mancanti → errore, abort.
4. Per ciascuna chiave in `[Settings]`:
   - Se è una proprietà nota del nostro schema, validare il valore (range/dominio per quel tipo).
   - Se è una proprietà sconosciuta, **ignorarla silenziosamente** + log a stderr (`g_message`). Nessun abort.
   - Se il valore è invalido (fuori range, dominio non riconosciuto), **ignorare quella entry** + warning. Le altre proseguono.
5. Risolvere il conflitto di nome con `[Preset]/Name`:
   - Nome uguale a un built-in → rifiutare l'import con errore esplicito.
   - Nome uguale a un preset utente esistente → chiedere all'utente: sovrascrivi / rinomina / annulla. (UI in `settings-dialog.cpp`.)
6. Se tutto è valido: generare un nuovo `<uuid>` (a meno di sovrascrittura), scrivere `display-name`, scrivere ogni entry valida sotto `/presets/<uuid>/`. NON applicare il preset (l'utente lo applicherà esplicitamente dopo).

## Esportazione

L'exporter:

1. Legge il preset utente target da `/presets/<uuid>/`.
2. Compone il KeyFile con:
   - `[Preset].Name` ← `display-name`
   - `[Preset].SchemaVersion` ← `schema-version` corrente
   - `[Preset].CreatedBy` ← `meowmenu-X.Y.Z` (versione corrente)
   - `[Preset].ExportedAt` ← timestamp ISO 8601 UTC
   - `[Settings].*` ← una entry per ogni proprietà presente nel sotto-albero (escluse `display-name` e `created-by`, che sono metadati).
3. Scrive il file via `g_key_file_save_to_file()` nella destinazione scelta dall'utente nel `GtkFileChooser`.

## Versionamento

Future versioni dello schema:
- Aggiungeranno chiavi nuove → vecchi importer le ignoreranno (forward compat).
- Cambieranno il dominio di una chiave → handlers di migrazione per-key all'import. Documentati quando necessario.
- Rimuoveranno chiavi → importer le ignoreranno se sconosciute.

`SchemaVersion` permette agli importer futuri di applicare migrations specifiche se necessario.

## Sicurezza

- Il file è plain text. Nessuna esecuzione di codice è prevista; nessun rischio di code injection.
- I path nei valori (es. icone custom) NON sono ammessi in nessun campo: tutti i valori sono primitive (bool/int/enum/short string). Se in futuro venisse aggiunta una proprietà-path, va auditata in questa sede.
