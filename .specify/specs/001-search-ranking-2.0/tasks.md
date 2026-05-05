# Tasks: Search Ranking 2.0 Foundation

**Input**: `.specify/specs/001-search-ranking-2.0/spec.md` + `plan.md`
**Branch**: `feature/001-search-ranking-2.0`
**Stato**: DRAFT — in attesa di approvazione umana prima di `/speckit-implement`

---

## User Stories

| ID | Storia | Spec | Criteri di accettazione |
|---|---|---|---|
| **US1** | Come utente, voglio trovare app anche con piccoli errori di battitura | RF-03 | AC-02, AC-03, AC-07, AC-11 |
| **US2** | Come utente, voglio che le mie app preferite e usate di frequente emergano prima | RF-02, RF-05 | AC-04, AC-05, AC-08, AC-13 |
| **US3** | Come utente, voglio definire alias personalizzati per trovare le mie app | RF-04 | AC-06, AC-07, AC-14 |
| **US4** | Come utente, voglio configurare il comportamento della ricerca dall'interfaccia grafica | RF-06 | AC-09, AC-10, AC-12, AC-14, AC-15 |

---

## Phase 1 — Setup

**Scopo**: Aggiunta del nuovo file sorgente al build system.

- [X] T001 Aggiungere `'usage-stats.cpp'` alla lista `plugin_sources` in `panel-plugin/meson.build` (dopo `'run-action.cpp'`, prima di `'search-action.cpp'` per ordine alfabetico)
- [ ] T002 Verificare che `meson setup build && meson compile -C build` completino senza errori dopo T001 (con file stub vuoto per `usage-stats.{h,cpp}`)

---

## Phase 2 — Foundational

**Scopo**: Infrastruttura bloccante — nessuna user story può procedere prima che questa fase sia completa.

⚠️ **CRITICO**: US1, US2, US3 dipendono dalle impostazioni Xfconf definite qui. US2 dipende da `UsageStats`.

- [X] T003 [P] Creare `panel-plugin/usage-stats.h`: dichiarare struct `AppStats { gint64 last_launch_unix; int launch_count; }` e classe `UsageStats` con metodi pubblici `get_frecency(desktop_id, alpha, max_launches)`, `record_launch(desktop_id)` e privati `load()`, `save()`, membro `std::unordered_map<std::string, AppStats> m_stats`, membro `std::string m_cache_path`
- [X] T004 Creare `panel-plugin/usage-stats.cpp`: implementare `UsageStats::UsageStats()` (costruisce `m_cache_path` da `g_get_user_cache_dir() + "/xfce4/whiskermenu/stats"` e chiama `load()`), `load()` (legge TSV, skip silenzioso se file assente o riga malformata), `save()` (scrive TSV atomicamente con `g_file_set_contents`), `record_launch()` (aggiorna mappa + schedula `save()` via `g_idle_add_once`), `get_frecency()` (formula: `alpha / (delta_giorni+1) + (1-alpha) * log2(count+1)/log2(max+1)`, ritorna 0.0 se desktop_id assente) — dipende da T003
- [X] T005 [P] Aggiungere a `panel-plugin/settings.h`: cinque nuovi membri pubblici `Boolean fuzzy_enabled`, `Integer fuzzy_threshold`, `Boolean favorites_boost_enabled`, `Integer favorites_boost_level`, `Integer frecency_alpha` seguendo la convenzione dei wrapper esistenti (vedi esempi `Boolean hover_switch_category`, `Integer recent_items_max`)
- [X] T006 [P] Aggiungere a `panel-plugin/settings.h`: membro `UsageStats usage_stats`, metodi `get_aliases(desktop_id) const`, `set_aliases(desktop_id, terms)`, `load_aliases(channel)`, `save_aliases(channel) const`, e membro privato `std::unordered_map<std::string, std::vector<std::string>> m_aliases` — richiede `#include "usage-stats.h"`
- [X] T007 Aggiungere in `panel-plugin/settings.cpp`, nel costruttore `Settings::Settings()`, l'inizializzazione delle cinque nuove proprietà Xfconf: `fuzzy_enabled(this, "/search/fuzzy-enabled", true)`, `fuzzy_threshold(this, "/search/fuzzy-threshold", 0, 0, 2)`, `favorites_boost_enabled(this, "/search/favorites-boost-enabled", true)`, `favorites_boost_level(this, "/search/favorites-boost-level", 2, 1, 3)`, `frecency_alpha(this, "/search/frecency-alpha", 70, 0, 100)` — dipende da T005
- [X] T008 Implementare in `panel-plugin/settings.cpp` i metodi `load_aliases`, `save_aliases`, `get_aliases`, `set_aliases`: `load_aliases` enumera le proprietà `/search/aliases/*/terms` via `xfconf_channel_get_string_list`; `save_aliases` scrive ogni entry con `xfconf_channel_set_string_list`; `get_aliases` ritorna `const std::vector<std::string>&` (o ref a vettore vuoto se assente); `set_aliases` aggiorna `m_aliases[desktop_id]` — dipende da T006
- [X] T009 Chiamare `load_aliases(m_channel)` alla fine del blocco `Settings::load(const std::string& property_base)` in `panel-plugin/settings.cpp`, dopo che il canale Xfconf è già aperto — dipende da T008
- [X] T010 Aggiungere in `Settings::property_changed()` in `panel-plugin/settings.cpp` i dispatch per le nuove proprietà: le cinque nuove proprietà `/search/*` non richiedono `reload_menu()` — solo `Boolean::load`, `Integer::load` come gli altri casi analoghi

**Checkpoint**: `meson compile -C build` passa. `UsageStats` è istanziabile. Le nuove proprietà Xfconf hanno default corretti. `get_aliases` ritorna vettore vuoto per app senza alias. `get_frecency` ritorna 0.0 per app mai lanciata.

---

## Phase 3 — US1: Fuzzy Search

**Goal**: Trovare app con piccoli errori di battitura (score 0x400, sempre sotto i match esatti).

**Test indipendente**: Aprire il menu, digitare "Firfox" → Firefox appare nella lista. Digitare "Firefox" → Firefox è più in alto di "Firfox". Disattivare fuzzy → "Firfox" non produce risultati.

- [X] T011 [US1] Aggiungere funzione statica `static int levenshtein(const std::string& a, const std::string& b)` in `panel-plugin/query.cpp` (prima di `Query::match`): implementazione rolling-array a 2 righe, O(m·n) tempo O(n) spazio (vedi piano §4.1 per il codice di riferimento)
- [X] T012 [US1] Dichiarare `unsigned int match_fuzzy(const std::string& haystack, int max_errors) const` nella classe `Query` in `panel-plugin/query.h`
- [X] T013 [US1] Implementare `Query::match_fuzzy` in `panel-plugin/query.cpp`: ritorna `UINT_MAX` se `m_query` è vuota o ha più di un token; confronta `m_query` contro ogni parola dell'haystack con `levenshtein`; ritorna `0x400` se distanza ≤ `max_errors`, altrimenti `UINT_MAX` — dipende da T011, T012
- [X] T014 [US1] In `panel-plugin/launcher.cpp`, in `Launcher::search()`, aggiungere il fallback fuzzy **dopo** tutti i check esistenti e **dopo** il check alias (vedi T019), subito prima del `return UINT_MAX` finale: `if (m_settings->fuzzy_enabled && best == UINT_MAX) { int max_errors = (m_settings->fuzzy_threshold != 0) ? (int)m_settings->fuzzy_threshold : (m_query.raw_query().size() <= 4 ? 1 : 2); best = std::min(best, m_query.match_fuzzy(m_search_name, max_errors)); }` — dipende da T013, T005

**Checkpoint**: AC-02 verificato (Firfox → Firefox trovato). AC-03 verificato (Firfox posiziona Firefox sotto Firefox). AC-07 verificato (fuzzy disattivato → Firfox non trova nulla). AC-11 verificato (nessun lag percepibile).

---

## Phase 4 — US2: Frecency Boost

**Goal**: App preferite e usate di frequente emergono in cima a parità di match testuale.

**Test indipendente**: Cercare "f" (query ambigua, molte app): un'app in Favorites deve precedere app non-preferite con stesso score testuale. Lanciare un'app 5 volte, cercarla: deve essere prima rispetto a un'app mai lanciata.

- [X] T015 [US2] In `panel-plugin/page.cpp`, in `Page::launcher_activated()` (line 253–265), aggiungere dopo il blocco `m_window->get_recent()->add(launcher)` (line 257): `m_settings->usage_stats.record_launch(launcher->get_desktop_id());` — dipende da T004, T006
- [X] T016 [US2] In `panel-plugin/search-page.h`, nella classe interna `Match` (lines 57–95): aggiungere membro privato `double m_boost = 0.0`; aggiungere metodo pubblico `void set_frecency(double frecency, bool is_favorite, int boost_level)`; aggiornare `operator<` per usare `m_boost` come tiebreaker quando `m_relevancy == other.m_relevancy` (boost più alto vince: `return m_boost > other.m_boost`)
- [X] T017 [US2] Implementare `Match::set_frecency` in `panel-plugin/search-page.cpp`: `constexpr double kFavBonus[] = {0.5, 1.0, 2.0}; m_boost = frecency + (is_favorite ? kFavBonus[boost_level - 1] : 0.0);`
- [X] T018 [US2] In `SearchPage::set_filter()` in `panel-plugin/search-page.cpp`, dopo il loop `for (auto& match : m_matches) { match.update(m_query); }` (line 136–139) e **prima** di `erase + stable_sort`, aggiungere il loop che popola frecency per ogni match valido: per ogni `Match` non-`invalid`, fare `dynamic_cast<Launcher*>`, chiamare `match.set_frecency(usage_stats.get_frecency(...), is_fav, favorites_boost_level)` solo se `favorites_boost_enabled || recent_boost_enabled` — dipende da T015, T016, T017

**Checkpoint**: AC-04 verificato (app preferita emerge prima a parità di score testuale). AC-05 verificato (app lanciata di recente scala). AC-08 verificato (disabilitare boost → ordine alfabetico a parità). AC-13 verificato (frecency persiste tra sessioni).

---

## Phase 5 — US3: Alias

**Goal**: L'utente può definire termini alternativi per trovare un'app specifica.

**Test indipendente**: Aprire Impostazioni → tab Search → aggiungere alias "browser" a Firefox. Aprire il menu, cercare "browser": Firefox compare. Cercare "browser, web" CSV: entrambi i termini funzionano separatamente.

- [X] T019 [US3] In `panel-plugin/launcher.cpp`, in `Launcher::search()`, aggiungere il lookup alias **dopo** tutti i check sui campi standard (`m_search_name`, `m_search_generic_name`, ecc.) e **prima** del fuzzy fallback (T014): iterare su `m_settings->get_aliases(get_desktop_id())`; per ogni alias calcolare `m_query.match(alias)`; aggiornare `best = std::min(best, score)` se il match non è `UINT_MAX` — dipende da T008

**Checkpoint**: AC-06 verificato (alias "browser" → Firefox trovato). AC-07 verificato (alias "browser, web, internet" CSV: ogni termine funziona separatamente).

---

## Phase 6 — US4: Settings UI — Tab "Advanced Search"

**Goal**: Tutte le nuove opzioni sono accessibili dall'interfaccia grafica delle Preferenze, con reset a default. Il tab è denominato "Advanced Search". Ogni sezione ha un pulsante "?" con spiegazione in popover; Fuzzy matching + Max errors sono sulla stessa riga; Boost favorites + Level sono sulla stessa riga.

**Test indipendente**: Aprire Impostazioni Whisker → verificare presenza tab "Advanced Search" → verificare che fuzzy switch e spinbutton siano affiancati → verificare che boost switch e combo siano affiancati → cliccare "?" delle sezioni → leggi spiegazione → modificare un'opzione → chiudere e riaprire: l'opzione è mantenuta → premere "Reset": torna al default.

- [X] T020 [US4] In `panel-plugin/settings-dialog.h`: dichiarare `GtkWidget* init_search_tab()`; aggiungere membri widget per il tab Search: `GtkWidget* m_fuzzy_enabled`, `GtkWidget* m_fuzzy_threshold`, `GtkWidget* m_favorites_boost_enabled`, `GtkWidget* m_favorites_boost_level`, `GtkWidget* m_frecency_alpha`, `GtkTreeView* m_aliases_view`, `GtkListStore* m_aliases_model`, `GtkWidget* m_alias_add`, `GtkWidget* m_alias_remove` — dipende da T005, T006
- [X] T021 [US4] Implementare `SettingsDialog::init_search_tab()` in `panel-plugin/settings-dialog.cpp` secondo il layout aggiornato (piano §9.3): (a) aggiungere helper file-scope `make_info_frame(title, content, info_text)` che crea `GtkFrame` con intestazione `[<b>title</b>][?]` — il "?" mostra un `GtkPopover` con spiegazione on-click; (b) sezione "Fuzzy Search": fuzzy switch e spinbutton Max errors affiancati in `GtkBox` orizzontale su una sola riga; (c) sezione "Usage Boost": boost switch e combo Level affiancati su una sola riga; (d) riga "Recency weight" con hint testuale sempre visibile sotto il cursore (font piccolo); (e) ogni sezione avvolto in `make_info_frame` con testo popover appropriato — dipende da T020
- [X] T022 [US4] In `SettingsDialog::init_search_tab()`: implementare la sezione Alias con `GtkTreeView` a tre colonne (App display name, Aliases CSV, desktop-id nascosto); pulsante "Aggiungi" apre popup con lista launcher per selezionare l'app target; pulsante "Rimuovi" elimina la riga selezionata; la colonna Aliases è editabile inline (renderer text editabile); popolare il model con i dati da `m_settings->aliases` — dipende da T021
- [X] T023 [US4] Aggiungere pulsante "Reset to Defaults" in fondo al tab: al click, resetta `fuzzy_enabled`, `fuzzy_threshold`, `favorites_boost_enabled`, `favorites_boost_level`, `frecency_alpha` ai rispettivi default e svuota `m_aliases_model` — dipende da T021
- [X] T024 [US4] In `SettingsDialog::SettingsDialog()` (costruttore) in `panel-plugin/settings-dialog.cpp`: aggiornare la label del tab da `_("_Search")` a `_("_Advanced Search")` — dipende da T021
- [X] T025 [US4] In `SettingsDialog::response()` in `panel-plugin/settings-dialog.cpp`: aggiungere `m_settings->save_aliases(m_settings->get_channel())` al momento della chiusura del dialog (stesso pattern di `search_actions` save in `plugin.cpp:346-352`) — dipende da T022

**Checkpoint**: AC-09 verificato (reset impostazioni). AC-12 verificato (configurazione persiste). AC-14 verificato (alias persiste). AC-15 verificato (eliminazione stats cache non causa crash).

---

## Phase 7 — Polish & Non-regression

**Scopo**: Traduzioni, verifica build completa, test di latenza, verifica acceptance criteria.

- [X] T026 [P] Aggiornare `po/` per le nuove stringhe traducibili introdotte nel tab Search (label widget, descrizioni, nome tab "Search"): eseguire `xgettext` sul codice modificato, aggiornare `panel-plugin/whiskermenu.pot`, verificare che le stringhe siano marcate con le macro gettext esistenti (`_("...")`)
- [ ] T027 Misurare latenza baseline di `SearchPage::set_filter()` sul corpus reale (o su ~200 launcher simulati) **prima** di integrare le modifiche di T013, T014, T018, T019 — annotare il risultato in un commento nel PR
- [ ] T028 Eseguire `meson setup build && meson compile -C build && meson test -C build` e riportare l'output: se `meson test` riporta zero test per i moduli toccati, aprire un task di follow-up per aggiungere test (per costituzione §Build gates)
- [ ] T029 Misurare latenza di `SearchPage::set_filter()` dopo la patch (stesso corpus di T027): verificare che l'incremento medio sia < 20% rispetto al baseline; se supera la soglia, non marcare questo task come done senza approvazione esplicita
- [ ] T030 Eseguire il checklist di non-regressione manuale su sessione Xfce X11 (Xubuntu 26.04 VM o nested Xfce): verificare AC-01 (query esatta), AC-10 (Search Actions), AC-11 (latenza percepita), AC-15 (stats cache eliminata → no crash)
- [ ] T031 Eseguire il checklist acceptance criteria completo su sessione Xfce X11: verificare AC-01 → AC-15 come descritto nella spec §5; documentare l'esito nel PR description

---

## Dipendenze e ordine di esecuzione

### Dipendenze tra fasi

```
Phase 1 (Setup)
    └─► Phase 2 (Foundational)  [blocca tutto]
            ├─► Phase 3 (US1: Fuzzy)   ─────────────────┐
            ├─► Phase 4 (US2: Frecency)  ────────────────┤
            ├─► Phase 5 (US3: Aliases)  ─────────────────┤─► Phase 6 (US4: UI)
            └─► (tutti e tre completati) ────────────────┘
                                                              └─► Phase 7 (Polish)
```

### Dipendenze interne alla Phase 2

```
T003 ──► T004  (usage-stats.h → usage-stats.cpp)
T005 ─┐
T006 ─┤─► T007 (settings.h → settings.cpp ctor)
       ├─► T008 (alias methods)
       │     └─► T009 (load_aliases call)
       └─► T010 (property_changed dispatch)
```

### Dipendenze tra fasi e task specifici

| Task | Dipende da |
|---|---|
| T014 (fuzzy in launcher) | T013 (match_fuzzy), T005 (fuzzy_enabled setting) |
| T015 (launch hook) | T004 (UsageStats impl), T006 (usage_stats in Settings) |
| T018 (frecency in set_filter) | T015 (launch hook), T016+T017 (Match esteso) |
| T019 (alias in launcher) | T008 (get_aliases impl) |
| T020–T025 (settings UI) | T005–T010 (tutti i settings fondati) |

### Opportunità di parallelismo

- **T003 e T005 e T006**: paralleli (file diversi — usage-stats.h, settings.h x2)
- **T011, T012**: paralleli (levenshtein function e header declaration)
- **T026 e T027**: paralleli (po/ e baseline latency sono indipendenti)
- **Phase 3, Phase 4, Phase 5**: dopo Phase 2, possono procedere in parallelo
  su file diversi (query.cpp, search-page.cpp, launcher.cpp con sezioni distinte)

---

## Mapping PR → Tasks

| PR | Titolo | Tasks | File toccati |
|---|---|---|---|
| PR-1 | `usage-stats: add frecency cache module` | T001–T004, T006, T007 (parziale) | `meson.build`, `usage-stats.{h,cpp}`, `settings.{h,cpp}` (solo usage_stats member) |
| PR-2 | `settings: add search ranking xfconf properties` | T005, T007, T008, T009, T010 | `settings.{h,cpp}` |
| PR-3 | `search: add levenshtein fuzzy matching` | T011, T012, T013 | `query.{h,cpp}` |
| PR-4 | `search: integrate fuzzy and alias in launcher` | T014, T019 | `launcher.cpp` |
| PR-5 | `search: frecency boost in set_filter` | T015, T016, T017, T018 | `page.cpp`, `search-page.{h,cpp}` |
| PR-6 | `prefs: add Search tab with alias management` | T020–T025 | `settings-dialog.{h,cpp}` |
| PR-7 | `i18n+build: po update, latency test, acceptance` | T026–T031 | `po/`, build verification |

Ordine di merge consigliato: PR-1 → PR-2 → PR-3 → PR-4 → PR-5 → PR-6 → PR-7.
PR-3 e PR-4 possono essere sviluppati in parallelo a PR-5 (file diversi),
ma il merge di PR-4 richiede PR-2 già integrato.

---

## Strategia di implementazione

### MVP (solo US1 — Fuzzy Search)

1. Phase 1 + Phase 2 (T001–T010)
2. Phase 3 (T011–T014)
3. **STOP e VALIDA**: AC-02, AC-03, AC-07, AC-11 verificati
4. Il menu funziona con fuzzy ma senza boost ni alias: comportamento attuale + typo tolerance

### Incrementale

1. MVP (US1) → merge PR-3 + PR-4 parziale
2. Aggiungi US2 (PR-1 + PR-5) → frecency boost attivo
3. Aggiungi US3 (PR-4 completo) → alias funzionanti
4. Aggiungi US4 (PR-6) → UI configurabile
5. Polish (PR-7) → release-ready

---

## Note

- `[P]` = task eseguibile in parallelo con altri `[P]` della stessa fase (file diversi, nessuna dipendenza incompleta)
- Ogni PR deve compilare, passare `meson test` e non rompere i criteri AC-01, AC-10, AC-11 prima del merge
- I task UI (T020–T025) richiedono verifica manuale su sessione grafica Xfce X11 — non è sufficiente il solo `meson compile`
- Per la constitution §Build gates: se `meson test` riporta 0 test per i moduli toccati, il task T028 apre un follow-up task prima di dichiarare la milestone done
