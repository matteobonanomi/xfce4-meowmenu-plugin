# Piano 001 — Search Ranking 2.0 Foundation

| Campo | Valore |
|---|---|
| **Spec di riferimento** | `.specify/specs/001-search-ranking-2.0/spec.md` |
| **Branch** | `feature/001-search-ranking-2.0` |
| **Stato** | DRAFT — in attesa di approvazione umana |
| **Data** | 2026-05-04 |

---

## 1. Sintesi architetturale

Il piano introduce **tre capacità nuove** (fuzzy matching, frecency boost,
alias) mediante modifiche chirurgiche a sei file esistenti più un file nuovo.
Nessuna classe viene riscritta; nessuna dipendenza esterna viene aggiunta.

```
Panel-plugin (shared_module)
│
├── usage-stats.{h,cpp}       ← NUOVO: carica/salva stats cache, calcola frecency
│
├── query.{h,cpp}              ← +match_fuzzy() (Levenshtein livello 3)
│
├── launcher.{h,cpp}           ← +alias lookup in search(); +fuzzy fallback
│
├── settings.{h,cpp}           ← +6 nuove proprietà Xfconf; +mappa alias
│
├── search-page.{h,cpp}        ← Match esteso con frecency; sort composito
│
├── page.cpp                   ← hook lancio → UsageStats::record_launch()
│
└── settings-dialog.{h,cpp}    ← +init_search_tab(); +gestione alias GtkTreeView
```

`panel-plugin/meson.build` riceve una riga aggiuntiva per `usage-stats.cpp`.

---

## 2. Inventario dei file

### 2.1 File nuovo

| File | Ruolo |
|---|---|
| `panel-plugin/usage-stats.h` | Dichiarazione classe `UsageStats` |
| `panel-plugin/usage-stats.cpp` | Implementazione: load/save TSV, frecency O(1) |

### 2.2 File modificati

| File | Modifica principale | Righe chiave pre-patch |
|---|---|---|
| `panel-plugin/query.h` | Aggiunta `match_fuzzy(haystack, max_errors)` | — |
| `panel-plugin/query.cpp` | Funzione `levenshtein()` + implementazione `match_fuzzy` | `match()` line 42, `set()` line 171 |
| `panel-plugin/launcher.h` | Nessuna modifica all'interfaccia pubblica | `search()` line 99 |
| `panel-plugin/launcher.cpp` | `search()`: alias lookup + fuzzy fallback | da leggere in task |
| `panel-plugin/settings.h` | 6 nuove proprietà; `aliases` map; `UsageStats* usage_stats` | wrappers lines 33–210 |
| `panel-plugin/settings.cpp` | Ctor: nuove proprietà; `load_aliases/save_aliases` | ctor line 33, `property_changed` line 368 |
| `panel-plugin/search-page.h` | `Match`: +`m_frecency`, +`set_frecency()`; `operator<` composito | `Match` class lines 57–95 |
| `panel-plugin/search-page.cpp` | `set_filter()`: populate frecency; `update_search_order()` invariato | `set_filter` line 85 |
| `panel-plugin/page.cpp` | `launcher_activated()`: hook → `usage_stats->record_launch()` | line 253 |
| `panel-plugin/settings-dialog.h` | `+init_search_tab()`; nuovi membri widget | `init_*_tab` lines 58–62 |
| `panel-plugin/settings-dialog.cpp` | Nuovo tab; alias `GtkTreeView`; save su chiusura | da leggere in task |
| `panel-plugin/meson.build` | `+'usage-stats.cpp'` in `plugin_sources` | line 1 |

---

## 3. Modulo A — `UsageStats` (file nuovo)

### 3.1 Responsabilità

- Carica `$XDG_CACHE_HOME/xfce4/whiskermenu/stats` in un
  `std::unordered_map<std::string, AppStats>` all'avvio.
- Espone `get_frecency(desktop_id, alpha, max_launches)` → `double [0,1]`.
- Aggiorna i record in memoria e riscrive il file (asincrono) quando
  un'app viene lanciata.
- Non possiede thread, timer o segnali: opera solo on-demand.

### 3.2 Struttura dati

```cpp
// usage-stats.h
namespace WhiskerMenu {

struct AppStats {
    gint64 last_launch_unix = 0;
    int    launch_count     = 0;
};

class UsageStats {
public:
    UsageStats();                     // chiama load()

    // Calcolo frecency in-memory, O(1). alpha = peso recenza [0..1]
    double get_frecency(const char* desktop_id,
                        double alpha,
                        int max_launches = 100) const;

    // Chiamato da Page::launcher_activated() — aggiorna mappa + rischedula write
    void record_launch(const char* desktop_id);

private:
    void load();
    void save() const;           // scrive file TSV atomicamente

    std::unordered_map<std::string, AppStats> m_stats;
    std::string m_cache_path;    // $XDG_CACHE_HOME/xfce4/whiskermenu/stats
};

} // namespace WhiskerMenu
```

### 3.3 Formula frecency

```
recency_score(app)   = 1.0 / (delta_giorni + 1)
frequency_score(app) = log2(launch_count + 1) / log2(max_launches + 1)
frecency(app)        = alpha * recency_score + (1 - alpha) * frequency_score
```

`delta_giorni` = `(g_get_real_time() / G_USEC_PER_SEC - last_launch_unix) / 86400`
(divisione intera; massimo ~1 giorno di granularità, sufficiente per lo use case).

### 3.4 Scrittura asincrona

`record_launch()` aggiorna la mappa in memoria, poi chiama
`g_idle_add_once(write_stats_cb, this)` — la scrittura avviene nel main
loop appena il thread UI è idle, senza ritardare l'apertura dell'app.
`g_file_set_contents()` (scrittura atomica via file temporaneo) garantisce
che il file non sia corrotto in caso di crash tra apertura e flush.

### 3.5 Robustezza

- File mancante → `load()` ritorna silenziosamente; la mappa è vuota.
- Riga TSV malformata → riga saltata con `g_warning`.
- Desktop-id non presente nella mappa → `get_frecency()` ritorna `0.0`.
- `save()` fallisce → log `g_warning`, nessun crash.

---

## 4. Modulo B — Fuzzy matching in `query.{h,cpp}`

### 4.1 Nuova funzione statica (file-scope)

```cpp
// query.cpp (aggiunta prima di Query::match)
static int levenshtein(const std::string& a, const std::string& b)
{
    // matrice rolling a 2 righe, O(m·n) tempo, O(n) spazio
    std::vector<int> prev(b.size()+1), curr(b.size()+1);
    std::iota(prev.begin(), prev.end(), 0);
    for (size_t i = 1; i <= a.size(); ++i) {
        curr[0] = i;
        for (size_t j = 1; j <= b.size(); ++j)
            curr[j] = (a[i-1] == b[j-1])
                      ? prev[j-1]
                      : 1 + std::min({prev[j], curr[j-1], prev[j-1]});
        std::swap(prev, curr);
    }
    return prev[b.size()];
}
```

### 4.2 Nuova funzione membro pubblica

```cpp
// query.h — aggiunta alla classe Query
unsigned int match_fuzzy(const std::string& haystack, int max_errors) const;
```

```cpp
// query.cpp — implementazione
unsigned int Query::match_fuzzy(const std::string& haystack, int max_errors) const
{
    if (m_query.empty() || m_query_words.size() > 1)
        return UINT_MAX;   // fuzzy solo su query a singolo token
    if (static_cast<int>(haystack.length()) < static_cast<int>(m_query.length()) - max_errors)
        return UINT_MAX;   // haystack troppo corto anche nel caso migliore

    // Confronta la query contro ogni parola dell'haystack
    std::string word;
    std::stringstream ss(haystack);
    while (ss >> word) {
        if (levenshtein(m_query, word) <= max_errors)
            return 0x400;
    }
    return UINT_MAX;
}
```

**Score 0x400** si inserisce dopo i livelli esistenti (0x80 = substring,
0x100/0x200 = char sequence) e prima di UINT_MAX. Garantisce RF-03 R3.4:
un match fuzzy è sempre più debole di un match esatto, prefisso o substring.

**Perché solo query a singolo token:** con query multi-token il confronto
parola-per-parola diventa ambiguo e può produrre falsi positivi fastidiosi.
La spec (RF-03 R3.1–R3.3) non richiede fuzzy su query multi-token.

### 4.3 Soglia adattiva (calcolata nel chiamante)

Il calcolo della soglia non vive in `Query` ma in `Launcher::search()`,
che conosce sia `m_settings->fuzzy_threshold` (0=adattivo, 1 o 2=fisso)
sia la lunghezza della raw query:

```cpp
int max_errors = (m_settings->fuzzy_threshold != 0)
    ? static_cast<int>(m_settings->fuzzy_threshold)
    : (m_query.raw_query().size() <= 4 ? 1 : 2);
```

---

## 5. Modulo C — Alias e fuzzy in `launcher.cpp`

### 5.1 Struttura attuale di `Launcher::search()`

Il metodo (riga non letta — da verificare in task) chiama `m_query.match()`
su `m_search_name`, `m_search_generic_name`, `m_search_comment`,
`m_search_command` e `m_query.match_as_characters()` su `m_search_keywords`,
restituendo il punteggio migliore.

### 5.2 Modifiche

Due aggiunte in coda alla logica esistente, **dopo** tutti i check già
presenti, prima del `return UINT_MAX` finale:

**A — Alias lookup:**
```cpp
// cerca gli alias registrati per questo launcher
const auto& aliases = m_settings->get_aliases(get_desktop_id());
for (const auto& alias : aliases) {
    const unsigned int score = m_query.match(alias);
    if (score != UINT_MAX)
        best = std::min(best, score);  // alias vale come keyword primaria
}
```

**B — Fuzzy fallback su `m_search_name` (solo se fuzzy abilitato):**
```cpp
if (m_settings->fuzzy_enabled && best == UINT_MAX) {
    int max_errors = /* soglia adattiva come in §4.3 */;
    best = m_query.match_fuzzy(m_search_name, max_errors);
}
```

L'ordine è importante: alias prima di fuzzy, così un alias esatto batte
sempre un match fuzzy sul nome.

---

## 6. Modulo D — Nuove impostazioni in `settings.{h,cpp}`

### 6.1 Nuove proprietà Xfconf (da aggiungere nel costruttore `Settings::Settings()`)

```cpp
// settings.h — nuovi membri pubblici (seguendo la convenzione esistente)
Boolean fuzzy_enabled;           // "/search/fuzzy-enabled",      true
Integer fuzzy_threshold;         // "/search/fuzzy-threshold",     0, 0, 2
Boolean favorites_boost_enabled; // "/search/favorites-boost-enabled", true
Integer favorites_boost_level;   // "/search/favorites-boost-level",   2, 1, 3
Integer frecency_alpha;          // "/search/frecency-alpha",      70, 0, 100
                                 // (70 = alpha 0.70; diviso 100.0 a runtime)
```

Nessun wrapper `Double` necessario: `frecency_alpha` è Integer [0-100]
(es. 70 = 0.70), convertito a `double` con `/100.0` nei punti d'uso.

### 6.2 Mappa alias

La mappa alias non si presta al pattern `Boolean/Integer/String` (le chiavi
sono dinamiche). Si usa una semplice map con load/save espliciti:

```cpp
// settings.h — aggiunta a Settings
std::unordered_map<std::string, std::vector<std::string>> aliases;

// API interna
const std::vector<std::string>& get_aliases(const char* desktop_id) const;
void set_aliases(const std::string& desktop_id,
                 const std::vector<std::string>& terms);
void load_aliases(XfconfChannel* channel);
void save_aliases(XfconfChannel* channel) const;
```

**Load** (chiamato dentro `Settings::load(property_base)` dopo l'apertura
del canale): enumera i figli del path `/search/aliases/` via
`xfconf_channel_get_arrayv` per ogni desktop-id trovato.

**Save** (chiamato da `SettingsDialog::response()` al momento della
chiusura del dialog, seguendo lo stesso pattern di `SearchActionList`
in `plugin.cpp:346-352`): scrive ogni entry come
`xfconf_channel_set_string_list(channel, path, ...)`.

### 6.3 `UsageStats` come membro di `Settings`

```cpp
// settings.h
UsageStats usage_stats;   // costruito nel ctor di Settings
```

Posizionarlo in `Settings` è la scelta con minore friction: tutti i
componenti che già ricevono `Settings*` (Plugin, Page, SearchPage, Window)
ottengono accesso a `usage_stats` senza nuove dipendenze.

### 6.4 `property_changed` — nuovi casi

In `Settings::property_changed()` (line 368) aggiungere dispatch per le
nuove proprietà, analogamente ai casi esistenti:

| Proprietà | Azione |
|---|---|
| `fuzzy-enabled`, `fuzzy-threshold` | nessun reload menu richiesto — effect al prossimo `set_filter()` |
| `favorites-boost-enabled`, `favorites-boost-level`, `frecency-alpha` | nessun reload menu richiesto |

---

## 7. Modulo E — Sort composito in `search-page.{h,cpp}`

### 7.1 Estensione della classe `Match`

La classe `Match` (attualmente in `search-page.h:57-95`) riceve un secondo
campo di ordinamento:

```cpp
class Match {
public:
    // ... (costruttore, element(), update() — invariati)

    // NUOVO
    void set_frecency(double frecency, bool is_favorite, int boost_level);

    bool operator<(const Match& other) const
    {
        // 1. Punteggio testuale: più piccolo vince
        if (m_relevancy != other.m_relevancy)
            return m_relevancy < other.m_relevancy;
        // 2. A parità di testo: boost composito più alto vince
        return m_boost > other.m_boost;
    }

    // ... (operator==, invalid() — invariati)

private:
    Element*      m_element;
    unsigned int  m_relevancy = UINT_MAX;
    double        m_boost     = 0.0;     // NUOVO: frecency + favorites_bonus
};
```

**`set_frecency(frecency, is_favorite, boost_level)`:**
```
FAVORITES_BONUS[level] = { 0.5, 1.0, 2.0 }   // Bassa/Media/Alta

m_boost = frecency
          + (is_favorite ? FAVORITES_BONUS[boost_level - 1] : 0.0)
```

Il bonus favorites è una costante maggiore di qualsiasi frecency [0,1],
garantendo RF-05 R5.2 (preferiti prima dei non-preferiti a parità di score
testuale) mentre RF-02 R2.3 (boost non assoluto) è garantito dal fatto che
il boost agisce solo **a parità di `m_relevancy`**.

### 7.2 Modifiche a `set_filter()`

In `SearchPage::set_filter()` (line 85), dopo il loop che chiama
`match.update(m_query)` (line 136-139), aggiungere:

```cpp
// Popola frecency per ogni Match che ha superato il filtro testuale
if (m_settings->favorites_boost_enabled || m_settings->recent_boost_enabled) {
    const double alpha = m_settings->frecency_alpha / 100.0;
    for (auto& match : m_matches) {
        if (Match::invalid(match)) continue;
        const Launcher* l = dynamic_cast<const Launcher*>(match.element());
        if (!l) continue;
        const bool is_fav = m_settings->favorites_boost_enabled
                            && m_settings->favorites.find(l->get_desktop_id()) >= 0;
        const double frecency = m_settings->recent_boost_enabled
                                ? m_settings->usage_stats.get_frecency(
                                      l->get_desktop_id(), alpha)
                                : 0.0;
        match.set_frecency(frecency, is_fav, m_settings->favorites_boost_level);
    }
}
```

Il `std::stable_sort` già esistente (line 141) non cambia: usa
`Match::operator<` che ora è composito.

### 7.3 `update_search_order()` — invariato

Il metodo `update_search_order()` (lines 241-263) sposta fisicamente i
launcher preferiti/recenti in testa all'array `m_launchers` come tiebreaker
posizionale di ultima istanza. Questo meccanismo rimane inalterato e opera
in modo **ortogonale** al boost frecency: il boost agisce sul punteggio
del `Match`, `update_search_order` agisce sull'ordine iniziale dell'array
prima della valutazione. Non c'è interferenza.

---

## 8. Modulo F — Hook di lancio in `page.cpp`

### 8.1 Punto di inserimento

`Page::launcher_activated()` (line 253) è il singolo punto in cui
qualsiasi launcher del plugin viene eseguito dall'utente (sia dal click su
una pagina Preferiti/Recent/Applicazioni, sia dalla SearchPage tramite
`activate_path` → `launcher_activated`).

La modifica è minimale, subito dopo il blocco esistente che aggiunge
l'app alla lista "recenti" (line 255-259):

```cpp
// Aggiorna usage stats (frecency)
m_settings->usage_stats.record_launch(launcher->get_desktop_id());
```

### 8.2 Perché non in `Launcher::run()`

`Launcher::run()` è virtuale e viene chiamato anche da `launcher_action_activated`
(desktop actions) e potenzialmente da percorsi non-interattivi. Agganciarlo
lì conterebbe anche i lanci di azioni secondarie come lanci dell'app principale.
`Page::launcher_activated()` è il punto semanticamente corretto (lancio intenzionale
dell'app dall'utente).

---

## 9. Modulo G — Tab "Advanced Search" in `settings-dialog.{h,cpp}`

### 9.1 Nuovo metodo

```cpp
// settings-dialog.h — aggiunta
GtkWidget* init_search_tab();
```

Il metodo segue esattamente il pattern degli altri `init_*_tab()`:
costruisce il widget del tab, connette i segnali, ritorna il `GtkWidget*`.
Il tab è aggiunto al `GtkNotebook` nel costruttore `SettingsDialog()`,
dopo il tab "Search Actions" esistente, con label `_("_Advanced Search")`.

### 9.2 Funzione helper `make_info_frame`

```cpp
// settings-dialog.cpp (file-scope, prima di init_search_tab)
static GtkWidget* make_info_frame(const gchar* title,
                                   GtkWidget*   content,
                                   const gchar* info_text);
```

Crea un `GtkFrame` con:
- **Intestazione**: `GtkBox` orizzontale con `[<b>title</b>]` + pulsante `"?"`.
- Il pulsante "?" è `GTK_RELIEF_NONE`, on-click mostra un `GtkPopover`
  con `GtkLabel` (testo `info_text`, wrap 45 chars, margini 8px).
- Il popover è creato con `gtk_popover_new(info_btn)` e mostrato con
  `gtk_popover_popup()`.

### 9.3 Layout del tab

```
[GtkBox vertical, spacing=6]
  ┌─ Fuzzy Search ─────────────────────────── [?] ─┐
  │  GtkBox orizz:                                   │
  │    [Fuzzy matching:] [GtkSwitch]                 │
  │    [sep] [Max errors (0=auto):] [GtkSpinButton]  │
  └──────────────────────────────────────────────────┘
  ┌─ Usage Boost ──────────────────────────── [?] ─┐
  │  GtkGrid:                                        │
  │  row0: GtkBox orizz:                             │
  │    [Boost favorites:] [GtkSwitch]                │
  │    [sep] [Boost level:] [GtkComboBoxText]        │
  │  row1: [Recency weight (%):] [GtkScale 0–100]   │
  │  row2: (testo hint piccolo, sempre visibile)     │
  └──────────────────────────────────────────────────┘
  ┌─ Application Aliases ───────────────────────────┐
  │  GtkTreeView (App | Alias CSV)                   │
  │  [Add] [Remove]                                  │
  └──────────────────────────────────────────────────┘
  ── Footer ──────────────────────────────────────────
  GtkButton "Reset to Defaults"  [allineato a destra]
```

**Testo info popover — "Fuzzy Search":**
> "Finds apps even when you mistype a word.\n
> Example: \"firfox\" still finds Firefox.\n
> Max errors 0 = automatic (1 for short queries, 2 for longer ones)."

**Testo info popover — "Usage Boost":**
> "Promotes apps you use frequently or marked as favorites.\n
> Favorites always appear before non-favorites at equal relevance.\n
> Recency weight controls the balance between how recently vs.\n
> how often you launched an app."

**Testo hint recency (sempre visibile, sotto la scala):**
> "Higher = more weight to recently launched apps; lower = more weight
> to launch frequency."  (font PANGO_SCALE_SMALL)

### 9.4 Alias TreeView — dettagli

| Colonna | Tipo renderer | Editabile | Contenuto |
|---|---|---|---|
| `App` | `GtkCellRendererText` | No | Display name dell'app (da `Launcher::get_display_name()`) |
| `Aliases` | `GtkCellRendererText` | Sì | Stringa CSV (`browser, web, internet`) |

`GtkListStore` con due colonne STRING. La colonna App usa il desktop-id
come dato nascosto (third column G_TYPE_STRING, invisible) per identificare
l'app al momento del salvataggio.

Il pulsante "Aggiungi" apre un selettore di app (popup con lista dei
launcher caricati) per associare un desktop-id alla nuova riga, evitando
che l'utente debba digitare il nome a mano (RF-06 R6.4 aggiornata in R4.4).

### 9.5 Save su chiusura

In `SettingsDialog::response()` (pattern già presente per `search_actions`),
aggiungere:

```cpp
m_settings->save_aliases(m_settings->get_channel());
```

`Settings` espone il canale Xfconf tramite un getter interno già usato
da `StringList::save()`.

---

## 10. Modifica `meson.build`

```diff
 plugin_sources = [
   'applications-page.cpp',
+  'usage-stats.cpp',
   ...
 ]
```

Nessuna nuova dipendenza di sistema da aggiungere a `dependencies`.

---

## 11. Score arithmetic — quadro riassuntivo

La tabella seguente mostra l'intera gerarchia degli score dopo la patch,
in ordine crescente (più basso = più rilevante):

| Score | Tipo di match | Campo | Nota |
|---|---|---|---|
| `0x4` | Uguale esatto | `Name`, `GenericName` | Invariato |
| `0x8` | Prefisso | `Name`, `GenericName` | Invariato |
| `0x10` | Inizio parola | qualsiasi | Invariato |
| `0x20` | Parole in ordine | qualsiasi | Invariato |
| `0x40` | Parole in qualsiasi ordine | qualsiasi | Invariato |
| `0x80` | Substring | qualsiasi | Invariato |
| `0x100` | Char → iniziali parole | Keywords | Invariato |
| `0x200` | Char → qualsiasi posizione | Keywords | Invariato |
| `0x400` | **Levenshtein ≤ soglia** | **solo `Name`** | **NUOVO** |
| boost su `m_boost` | frecency + favorites | — | **NUOVO** (tiebreaker a parità di score testuale) |
| `UINT_MAX` | Nessun match | — | Eliminato dalla lista |

Invariante garantita: RF-03 R3.4 e RF-07 R7.1 sono soddisfatti per
costruzione — uno score `0x400` non può mai battere un `0x4`–`0x200`.

---

## 12. Flusso dati end-to-end

```
Utente digita "firfox"
        │
        ▼
Window::search()          [window.cpp:1122]
  → m_contents_stack → "search"
        │
        ▼
SearchPage::set_filter("firfox")  [search-page.cpp:85]
  1. m_query.set("firfox")
  2. per ogni Launcher in m_launchers:
       match.update(m_query)
         → Launcher::search(query)
             a. query.match(m_search_name)     → UINT_MAX  (nessun prefix/substring)
             b. query.match(m_search_generic_name) → UINT_MAX
             c. alias lookup                   → UINT_MAX  (nessun alias)
             d. query.match_fuzzy(m_search_name, max_errors=2)
                → levenshtein("firfox","firefox") = 1 ≤ 2 → 0x400
         → ritorna 0x400
       match.m_relevancy = 0x400
  3. per ogni Match valido:
       match.set_frecency(
           usage_stats.get_frecency("firefox.desktop", 0.70),
           favorites.find("firefox.desktop") >= 0,
           favorites_boost_level)
  4. stable_sort → Firefox fuzzy-match in lista, sotto eventuali match esatti
  5. GtkListStore → LauncherView
        │
        ▼
Utente vede Firefox nella lista
```

```
Utente clicca Firefox
        │
        ▼
Page::launcher_activated()     [page.cpp:238]
  → m_window->get_recent()->add(launcher)   [invariato]
  → m_settings->usage_stats.record_launch("firefox.desktop")
        │  aggiorna m_stats in memoria
        │  g_idle_add_once → write_stats_cb
        ▼
Launcher::run()  [launcher.cpp]
```

---

## 13. Constitution compliance check

| Principio | Verifica | Esito |
|---|---|---|
| **I — Spec-First** | Piano segue spec approvata 001 | ✓ |
| **II — Small patches** | Ogni modulo → 1 PR (vedi tasks) | ✓ |
| **III — Native stack only** | Nessuna dipendenza nuova; Levenshtein ~15 righe inline | ✓ |
| **IV — Xfconf single source of truth** | Config utente (fuzzy, boost, alias) in Xfconf; stats in `$XDG_CACHE_HOME` (cache derivata, non config) | ✓ |
| **V — X11 primary, Wayland fallback** | Tutti i componenti classificati X11+Wayland parity in spec §6 | ✓ |
| **VI — Garcon** | Nessun bypass: i launcher restano quelli caricati da `ApplicationsPage::load_garcon_menu()` | ✓ |
| **VII — Optional deps degrade** | Nessuna nuova dipendenza opzionale introdotta | ✓ |

---

## 14. Complexity tracking

Questa milestone introduce una sola complessità degna di nota.

**C1 — Stats cache separato da Xfconf**

`UsageStats` usa `$XDG_CACHE_HOME` invece di Xfconf. Questo è una
deviazione dal Principio IV che è stata deliberatamente classificata come
accettabile perché:
- I dati di stats sono cache derivata dal comportamento dell'utente, non
  impostazioni configurate dall'utente.
- La spec §4 RNF-03 documenta esplicitamente la distinzione e la sua
  motivazione (semantica XDG corretta, footprint < 5 KB, eliminabile senza
  perdita di configurazione).
- La decisione è stata confermata dall'utente (P1 — Opzione A) dopo la
  presentazione delle alternative.

**Sunset condition:** se in una futura spec si desidera sincronizzare le
stats di utilizzo tra macchine (es. per Profili 2.0), la migrazione verso
Xfconf o un backend sync-friendly dovrà essere valutata in quella spec.

---

## 15. Domande aperte

Nessuna. Tutte le domande di design sono risolte nella spec.

Il piano è pronto per la generazione dei tasks (`/speckit.tasks`).
