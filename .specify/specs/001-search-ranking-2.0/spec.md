# Spec 001 — Search Ranking 2.0 Foundation

| Campo | Valore |
|---|---|
| **Feature ID** | 001 |
| **Titolo** | Search Ranking 2.0 Foundation |
| **Milestone** | Fase 1 — Foundation UX |
| **Target platform** | Xubuntu 26.04 / Xfce 4.20 |
| **Stato** | PRONTO PER APPROVAZIONE — tutte le domande risolte |
| **Data** | 2026-05-04 |
| **Riferimento prodotto** | `docs/whisker-modernization-spec.md` §9.1 |
| **Decisioni di design** | `whisker-searchbar-decisions.md` (allegato utente, 2026-05-04) |

---

## 1. Contesto e motivazione

Whisker Menu dispone già di un motore di ricerca funzionante basato su
corrispondenza testuale con punteggio deterministico. Il sistema attuale ha
due limiti principali rispetto a un launcher moderno:

1. **Nessuna tolleranza ai refusi.** Un'unica lettera errata elimina
   completamente il risultato. L'utente che digita "firfox" non trova Firefox.

2. **Nessun segnale di utilizzo nel ranking.** A parità di match testuale,
   le app preferite e le app recenti non emergono rispetto a quelle mai usate.
   L'utente che lancia Firefox dieci volte al giorno lo trova allo stesso livello
   di un'app installata da mesi ma mai aperta.

Il gap è percepibile rispetto a GNOME Shell, KDE Krunner e ai launcher
moderni di Windows/macOS, dove il "tipo 2 lettere e trovi subito l'app giusta"
è un'expectation implicita dell'utente.

Questa spec definisce i requisiti per colmare entrambi i gap **senza
introdurre dipendenze esterne, daemon di indicizzazione o architetture
event-driven complesse**. L'obiettivo è un'implementazione puramente
in-memory, invisibile all'utente, che migliora la qualità dei risultati
senza modificare l'interfaccia esistente — salvo l'aggiunta di un tab
"Search" nelle Impostazioni.

**Corpus target:** ~200 app installate su una Xubuntu 26.04 tipica.
Il dimensionamento dell'algoritmo è calibrato su questo ordine di grandezza;
non è richiesta scalabilità a migliaia di voci.

---

## 2. Obiettivo della milestone

Rendere la ricerca di Whisker Menu comparabile a un launcher moderno in
termini di:

- **pertinenza dei risultati**: l'app giusta deve essere in cima quando
  l'utente ha una chiara intenzione, anche con refusi leggeri o abbreviazioni;
- **personalizzazione passiva**: preferiti e app usate di frequente devono
  avere un vantaggio nell'ordinamento, senza che l'utente debba fare nulla;
- **prevedibilità**: l'utente deve poter capire perché un risultato è in
  cima, senza che il ranking sembri "magico".

---

## 3. Requisiti funzionali

### RF-01 — Matching multi-campo

Il motore di ricerca deve considerare almeno i seguenti campi per ogni
applicazione, con pesi di rilevanza distinti:

| Campo | Priorità |
|---|---|
| Nome dell'app (`Name`) | Primario |
| Nome generico (`GenericName`) | Primario |
| Parole chiave (`Keywords`) | Secondario |
| Descrizione (`Comment`) | Secondario |
| Eseguibile (`Exec`, solo parte eseguibile) | Terziario |

Un risultato è incluso nella lista se almeno un campo produce un match
valido. Il punteggio finale di rilevanza deve riflettere in quale campo è
avvenuto il match migliore.

**Nota:** Whisker attualmente cerca già su tutti questi campi. Il requisito
non è aggiungere i campi, ma garantire che il sistema di punteggio li tratti
con priorità differenziate e documentate, e che RF-02 e RF-03 si integrino
coerentemente con questi pesi.

### RF-02 — Boost per utilizzo (frecency)

Le app che l'utente ha marcato come **preferite** o lanciato di **recente**
devono ricevere un incremento di rilevanza che le porti in cima alla lista
quando il punteggio di match testuale è equivalente o molto vicino.

**Formula adottata (Q2 — risolta):**

```
score_finale(app) = score_testuale(app)
                  + boost_preferiti(app)           [se app è in Favorites]
                  + alpha * recency_score(app)
                  + (1 - alpha) * frequency_score(app)

recency_score(app)   = 1.0 / (delta_giorni + 1)
                       // decadimento iperbolico; delta_giorni = intero (unix_now - last_launch) / 86400

frequency_score(app) = log2(launch_count + 1) / log2(MAX_LAUNCHES + 1)
                       // normalizzato [0, 1]; MAX_LAUNCHES = 100 (cap fisso)

alpha                = 0.7  (default configurabile; vedi RF-06)
```

- `boost_preferiti`: valore fisso costante per tutta la sessione (intensità
  configurabile, vedi RF-06).
- `recency_score` e `frequency_score` richiedono i dati del stats cache
  (vedi §7.2).
- Tutti i calcoli sono O(1) per app e avvengono solo al momento del
  filtering, mai in background.

Requisiti di dettaglio:

- R2.1 — Le app in `Favorites` ricevono un boost fisso costante per tutta
  la sessione.
- R2.2 — Le app nella lista `Recent` ricevono un boost frecency che
  combina decadimento temporale e frequenza di lancio secondo la formula
  sopra.
- R2.3 — L'effetto di boost deve essere percepibile ma non assoluto: se
  un'app preferita ha un match testuale molto peggiore rispetto a un'altra
  app, deve comunque perdere.
- R2.4 — Il boost non si applica a ricerche con 5 o più token distinti,
  dove si assume che l'utente abbia un'intenzione molto precisa.
- R2.5 — I dati `last_launch_unix` e `launch_count` sono aggiornati
  ogni volta che l'utente lancia un'app dal menu, scritti in modo asincrono
  nel stats cache (vedi §7.2). Nessuna scrittura in background.

### RF-03 — Tolleranza ai refusi leggeri (fuzzy)

Il motore deve trovare risultati anche in presenza di piccoli errori di
battitura. La strategia è a **tre livelli in cascata**, valutati in ordine
di priorità decrescente:

| Livello | Tipo di match | Esempio |
|---|---|---|
| 1 | Prefix esatto | `"fire"` → **Fire**fox |
| 2 | Substring esatto | `"fox"` → Fire**fox** |
| 3 | Levenshtein ≤ soglia adattiva | `"firfox"` → Firefox |

**Algoritmo adottato (Q1 — risolto):** Levenshtein semplice (matrice
rolling a 2 righe, O(m·n) tempo, O(n) spazio), senza dipendenze esterne.
Con un corpus di ~200 app il costo computazionale è irrilevante.

**Soglia adattiva:**
```
max_errors = 1   se len(query) ≤ 4
max_errors = 2   se len(query) >  4
```

Requisiti di dettaglio:

- R3.1 — Un singolo carattere errato nella query (sostituzione, omissione
  o aggiunta) produce il risultato atteso per query di almeno 3 caratteri.
- R3.2 — Due caratteri errati su una query di almeno 5 caratteri possono
  produrre risultati, con punteggio di rilevanza più basso rispetto al
  match esatto.
- R3.3 — La tolleranza si applica **solo al campo `Name`**. Non si applica
  a `Comment`, `Keywords` o `Exec` per evitare risultati rumorosi. Il
  livello 1 e 2 (prefix/substring) restano attivi su tutti i campi
  indipendentemente dalla tolleranza fuzzy.
- R3.4 — Un match per tolleranza deve avere punteggio **sempre inferiore**
  a un match esatto o prefisso sullo stesso campo. Non deve mai spostare
  un'app corrisposta esattamente verso il basso.
- R3.5 — La tolleranza è **disattivabile** dall'utente dal tab Search
  delle Impostazioni (vedi RF-06).

### RF-04 — Alias e abbreviazioni configurabili

L'utente deve poter definire termini alternativi per trovare un'app specifica.

**UI adottata (Q4 — risolto):** `GtkTreeView` editabile con due colonne,
stesso pattern delle Search Actions esistenti nel dialog preferenze.

| Colonna | Editabile | Contenuto |
|---|---|---|
| App Name | No (sola lettura) | Nome display dell'applicazione |
| Aliases | Sì | Alias separati da virgola (`gimp, image editor, foto`) |

Requisiti di dettaglio:

- R4.1 — Il sistema deve consentire di associare uno o più alias a un'app,
  oltre alle keyword già previste dal `.desktop`.
- R4.2 — Un alias è un termine di ricerca aggiuntivo trattato come keyword
  del campo primario: se la query corrisponde all'alias, l'app appare nei
  risultati con il punteggio corrispondente.
- R4.3 — Gli alias sono configurati dall'utente tramite il tab Search delle
  Impostazioni (vedi RF-06), senza modificare i file `.desktop` di sistema.
- R4.4 — Gli alias sono memorizzati in **Xfconf** sotto il namespace
  `/search/` (vedi §7.1). Opzione A scelta dall'utente (2026-05-04):
  Xfconf è il backend, il dialog serializza CSV↔StringList al
  salvataggio/caricamento.
- R4.5 — L'utente può aggiungere, modificare e rimuovere alias senza
  riavviare il pannello o il menu. Le modifiche si riflettono alla prossima
  apertura del menu o al prossimo reload.
- R4.6 — Il parsing degli alias è `split(',')` + trim su ogni token.

### RF-05 — Ordinamento stabile e comprensibile

La lista dei risultati deve seguire un ordinamento prevedibile che l'utente
possa capire dopo breve uso.

Requisiti di dettaglio:

- R5.1 — A parità di score finale, l'ordinamento è **alfabetico per nome
  dell'app**. Non deve mai sembrare casuale.
- R5.2 — Il gruppo di app preferite deve sempre precedere le app non
  preferite a parità di score testuale.
- R5.3 — All'interno delle app non preferite, l'ordine segue lo score
  frecency (decadimento iperbolico + frequenza, formula in RF-02).
- R5.4 — La logica di ordinamento deve essere documentata in un commento
  inline nel codice del modulo di scoring, comprensibile a un manutentore
  in pochi minuti.

### RF-06 — Tab "Advanced Search" nelle Impostazioni

**Collocazione adottata (Q3 — risolto):** tab separato "Advanced Search" nel
dialog preferenze esistente di Whisker, coerente con il pattern dei tab
esistenti (Appearance, Behavior, Commands, Search Actions).

**Widget e opzioni:**

| Opzione | Widget GTK | Default | Descrizione (visibile in UI) |
|---|---|---|---|
| Abilita fuzzy search + soglia errori | `GtkSwitch` + `GtkSpinButton` sulla stessa riga | ON / adattiva | Switch e numero massimo errori affiancati |
| Peso recenza (alpha) | `GtkScale` 0.0–1.0 | 0.7 | Bilancia recenza e frequenza nel boost utilizzo |
| Abilita boost preferiti + intensità | `GtkSwitch` + `GtkComboBox` sulla stessa riga | ON / Media | Switch e intensità affiancati |
| Gestione alias | `GtkTreeView` (vedi RF-04) | — | Termini alternativi per trovare un'app |

Vincoli di layout:

- R6.1 — Ogni opzione ha una descrizione di una frase visibile accanto al
  controllo, non nascosta in tooltip.
- R6.2 — Il tab ha un pulsante "Ripristina valori predefiniti" che resetta
  tutte le opzioni di questa sezione e svuota la lista alias.
- R6.3 — Le modifiche alle opzioni switch, scale e combobox entrano in
  effetto alla prossima apertura del menu, senza riavvio del pannello.
- R6.4 — Il tab è compatto: massimo 8 widget visibili senza scroll. Se
  le opzioni crescessero oltre, usare `GtkExpander` per le avanzate, non
  aggiungere un ulteriore tab.
- R6.5 — La soglia fuzzy mostra il valore `0` (adattivo) di default; se
  l'utente la modifica, mostra il numero scelto (1 o 2).
- R6.6 — Ogni sezione del tab ("Fuzzy Search", "Usage Boost") ha un piccolo
  pulsante "?" nell'intestazione della sezione. Al click mostra un `GtkPopover`
  con una spiegazione breve in linguaggio utente (2–4 righe), comprensibile
  da un utente non tecnico. Il popover scompare al click esterno.
- R6.7 — Per il controllo "Recency weight" è presente una riga di testo
  esplicativo di dimensione ridotta immediatamente sotto il cursore, visibile
  senza interazione (nessun click richiesto).

### RF-07 — Nessuna regressione sull'esperienza esistente

Il nuovo motore di ranking deve produrre risultati **almeno equivalenti**
all'attuale in tutti i casi in cui l'utente digita una stringa esatta o un
prefisso di nome app.

- R7.1 — Una query esatta sul nome di un'app restituisce quell'app come
  primo risultato, indipendentemente dai boost.
- R7.2 — Una query prefisso (es. "fire" per Firefox) restituisce l'app
  corrispondente nelle prime posizioni.
- R7.3 — Le Search Actions esistenti continuano a funzionare invariate.
- R7.4 — La Run Action ("Esegui …") continua ad apparire nell'elenco.
- R7.5 — La latenza percepita della ricerca non aumenta rispetto al
  baseline su corpus di ~200 app.

---

## 4. Requisiti non funzionali

### RNF-01 — Prestazioni

- La ricerca aggiorna i risultati a ogni battitura senza lag percepibile.
- Il calcolo del ranking avviene interamente in memoria sui dati già
  caricati da garcon. Nessuna lettura da disco al momento della ricerca
  (il stats cache è caricato in memoria all'avvio, non riletto a ogni query).
- Il debounce esistente (se presente) può essere mantenuto ma non aumentato.
- Corpus di riferimento per i test di latenza: ~200 app.

### RNF-02 — Nessuna dipendenza esterna aggiuntiva

- Nessun daemon di indicizzazione (Zeitgeist, Tracker, Baloo, ecc.) come
  requisito obbligatorio.
- Nessuna libreria di fuzzy matching esterna. Levenshtein è implementato
  nel codice del fork (~15 righe, nessuna dipendenza).

### RNF-03 — Persistenza: due livelli distinti

Questa feature introduce due categorie di dati persistenti con storage
diverso:

**Configurazione utente → Xfconf** (constitution §IV, invariabile):

- Opzioni del tab Search (fuzzy enabled, soglia, alpha, boost preferiti,
  intensità boost).
- Alias utente. ⚠ Punto pendente P1 — vedi §9.

**Cache di utilizzo → `$XDG_CACHE_HOME`** (dati derivati, non configurazione):

- File `$XDG_CACHE_HOME/xfce4/whiskermenu/stats`: contiene
  `app_desktop_id`, `last_launch_unix`, `launch_count` per ogni app
  lanciata almeno una volta. Formato TSV, una riga per app.
- Il file può essere eliminato senza perdita di configurazione: al
  successivo avvio il boost frecency si azzera e si ricostruisce
  progressivamente.
- Footprint atteso: < 5 KB per ~200 app.
- Scrittura solo on-demand al momento del lancio, mai in background.

Questo schema non introduce una "parallel configuration store" perché il
file di stats non contiene impostazioni user-facing: non è modificabile
dall'utente, non compare nelle Impostazioni e non è importato/esportato
con il profilo.

### RNF-04 — Localizzazione

- Tutte le stringhe UI nuove sono marcate con le macro gettext esistenti.
- Gli alias inseriti dall'utente sono testo libero e non vengono tradotti.

### RNF-05 — Accessibilità

- I nuovi controlli nel tab Search hanno label accessibili.
- Nessuna regressione sul comportamento della search entry (focus, Esc,
  Enter, tastiera).

---

## 5. Criteri di accettazione

| ID | Criterio | Come verificare |
|---|---|---|
| AC-01 | Query esatta → app in primo posto | Digitare il nome esatto di un'app, verificare che sia prima indipendentemente dai boost |
| AC-02 | Refuso singolo → app trovata | Digitare "Firfox" (len=6, max_errors=2): Firefox deve apparire nella lista |
| AC-03 | Refuso → posizione inferiore al match esatto | "Firfox" deve posizionare Firefox più in basso di quanto farebbe "Firefox" |
| AC-04 | App preferita emerge a parità di score testuale | Cercare una stringa presente in molte app; l'app preferita deve essere prima tra quelle con score testuale uguale |
| AC-05 | App recente scala con frecency | Lanciare un'app ripetutamente, cercarla: deve essere alta nella lista; dopo settimane senza lanci, deve scendere |
| AC-06 | Alias funziona | Aggiungere alias "browser" a Firefox, cercare "browser": Firefox compare |
| AC-07 | Alias formato CSV | Aggiungere "browser, web, internet" a Firefox: ognuno dei tre termini produce Firefox |
| AC-08 | Disabilitare fuzzy | Con fuzzy OFF: "Firefox" → trovato; "Firfox" → nessun risultato |
| AC-09 | Disabilitare boost preferiti | Con boost OFF: preferiti e non-preferiti a parità di score testuale sono in ordine alfabetico |
| AC-10 | Reset impostazioni | "Ripristina predefiniti" → tutte le opzioni tornano ai default; alias rimossi |
| AC-11 | Nessuna regressione Search Actions | Search action "?" → web con "?openstreetmap": azione eseguita correttamente |
| AC-12 | Nessuna regressione latenza | Su hardware tipico (~200 app), nessun lag visibile durante la digitazione |
| AC-13 | Stats cache persiste tra sessioni | Chiudere la sessione Xfce, riaprire: le app lanciate frequentemente mantengono il boost frecency |
| AC-14 | Configurazione persiste tra sessioni | Chiudere la sessione Xfce, riaprire: opzioni Search e alias sono invariati |
| AC-15 | Eliminazione stats cache | Cancellare manualmente il file stats: il menu si apre normalmente, il boost frecency si azzera senza crash |

---

## 6. Classificazione Wayland

| Componente | Classificazione | Motivazione |
|---|---|---|
| Motore di ranking (core, in-memory) | **X11+Wayland parity** | Algoritmo puramente in-memory, nessuna dipendenza da display server |
| Tab Search nel dialog Impostazioni | **X11+Wayland parity** | Finestra di dialogo standard GTK, nessuna funzione X11-specifica |
| Lettura/scrittura stats cache (XDG_CACHE_HOME) | **X11+Wayland parity** | Accesso a file su filesystem, indipendente dal display server |
| Alias storage (Xfconf) | **X11+Wayland parity** | Xfconf non dipende dal display server |
| Aggiornamento stats al lancio di un'app | **X11+Wayland parity** | L'evento di lancio è intercettato a livello plugin, non dal display server |

Non esistono componenti in questa milestone che richiedano fallback Wayland.

---

## 7. Storage: schema dettagliato

### 7.1 Xfconf — configurazione utente

Tutte le proprietà vivono nel canale del plugin (base da
`xfce_panel_plugin_get_property_base`), sotto il prefisso `/search/`.

| Proprietà | Tipo Xfconf | Default | Descrizione |
|---|---|---|---|
| `/search/fuzzy-enabled` | Bool | `true` | Abilita tolleranza ai refusi |
| `/search/fuzzy-threshold` | Int [1–2] | `0` (adattivo) | Soglia errori: 0 = adattiva, 1 o 2 = fissa |
| `/search/frecency-alpha` | Double [0.0–1.0] | `0.7` | Peso recenza vs frequenza nella formula frecency |
| `/search/favorites-boost-enabled` | Bool | `true` | Abilita boost per app preferite |
| `/search/favorites-boost-level` | Int [1–3] | `2` | Intensità boost preferiti (1=Bassa, 2=Media, 3=Alta) |
| `/search/aliases/<desktop-id>/terms` | StringList | `[]` | Alias CSV per quell'app (⚠ punto pendente P1) |

**Requisito di migrazione:** tutte le proprietà hanno default che
riproducono il comportamento pre-milestone. Gli utenti esistenti che non
aprono le Impostazioni non notano cambiamenti di comportamento, salvo
l'attivazione di default del fuzzy (`fuzzy-enabled = true`), considerato
un miglioramento trasparente.

### 7.2 XDG_CACHE_HOME — stats di utilizzo

**Path:** `$XDG_CACHE_HOME/xfce4/whiskermenu/stats`
(tipicamente `~/.cache/xfce4/whiskermenu/stats`)

**Formato:** TSV, una riga per app lanciata, colonne:

```
<app_desktop_id>  <last_launch_unix>  <launch_count>
firefox.desktop   1746300000          47
gimp.desktop      1746200000          12
```

**Comportamento:**

- Il file è caricato in un `std::unordered_map` all'avvio del plugin
  (o al primo accesso, lazy).
- Al lancio di un'app: aggiorna `last_launch_unix = time(nullptr)`,
  incrementa `launch_count`, riscrive il file in modo asincrono
  (non blocca il thread UI).
- Non esiste un thread di manutenzione o un timer in background.
- Il file non è mai letto durante una query di ricerca; i dati sono già
  in memoria.
- Se il file manca o è corrotto: il boost frecency si azzera silenziosamente
  senza crash o warning all'utente.

---

## 8. Out of scope per questa milestone

| Funzionalità | Riferimento |
|---|---|
| Ricerca file e documenti (full-text, filesystem) | Futura milestone Fase 3 |
| Ricerca semantica o assistita da AI | Non-goal permanente (constitution §III) |
| Provider pluggable (modello KRunner) | Futura milestone Fase 3 §10.1 |
| Indicatori app in esecuzione | Fase 2 §9.8 |
| Modifica dei file `.desktop` di sistema | Non-goal permanente |
| Indexer esterno (Tracker, Zeitgeist) | Non-goal permanente (RNF-02) |
| Recent Documents (file recenti) | Fase 1 §9.5 — spec separata |
| Layout Presets | Fase 1 §9.2 — spec separata |
| Sincronizzazione stats cache tra macchine | Fuori scope — XDG_CACHE è locale |

---

## 9. Punti pendenti

Nessun punto aperto. Tutte le domande sono risolte:

| ID | Domanda | Decisione |
|---|---|---|
| Q1 | Algoritmo fuzzy | Levenshtein semplice, soglia adattiva, strategia a tre livelli |
| Q2 | Boost recenti | Frecency = decadimento iperbolico + log(count), alpha=0.7 |
| Q3 | Collocazione opzioni Search | Tab separato "Advanced Search" nel dialog preferenze |
| Q4 | UI gestione alias | GtkTreeView editabile, alias CSV in colonna singola |
| Q5 | Timestamp decadimento | Timestamp UNIX esistente + `launch_count` intero nel stats cache |
| P1 | Storage alias | **Opzione A — Xfconf** (confermata dall'utente 2026-05-04) |

La spec è pronta per `/speckit.plan`.

---

## 10. Dipendenze e prerequisiti

| Tipo | Dettaglio |
|---|---|
| **Codice upstream** | Nessuna dipendenza da patch non ancora in upstream Whisker |
| **Dipendenze di sistema** | Nessuna nuova dipendenza oltre allo stack corrente |
| **Spec prerequisite** | Nessuna — questa è la prima milestone |
| **Spec che dipendono da questa** | Spec 005 (Recent Hub 2.0) beneficia dell'infrastruttura frecency; Spec 002 (Layout Presets) non dipende da questa |

---

## 11. Test strategy di alto livello

### 11.1 Test funzionali

Per ogni criterio di accettazione in §5 deve esistere un test case nei
tasks. I test che richiedono un'istanza grafica devono avere un percorso
di riproduzione manuale su sessione Xfce X11 (Xubuntu 26.04 VM o nested
Xfce session).

### 11.2 Test di non regressione

Il task di implementazione deve verificare che le seguenti query producano
risultati equivalenti o migliori rispetto al baseline:

- query esatta su `Name`
- query prefisso su `Name`
- query su `GenericName`
- query su `Keywords`
- search action con pattern letterale
- search action con pattern regex

### 11.3 Test di latenza

Un task separato deve misurare `SearchPage::set_filter` prima e dopo la
modifica su corpus di ~200 app. Se la latenza media aumenta di più del
**20%** rispetto al baseline, il task non può essere marcato done senza
approvazione esplicita.

### 11.4 Test del stats cache

- Eliminare il file stats prima dell'avvio: il menu si apre senza errori.
- File stats corrotto (testo non parsabile): il menu si apre, il boost
  frecency è assente senza crash.
- File stats assente al momento del lancio di un'app: viene creato
  automaticamente.

---

## 12. Glossario

| Termine | Definizione |
|---|---|
| **Frecency** | Metrica composita che bilancia recenza (iperbolica) e frequenza (logaritmica) per stimare la probabilità che l'utente voglia aprire quell'app; parametro `alpha` controlla il bilanciamento |
| **Boost** | Incremento dello score finale applicato a un risultato in virtù del suo status (preferito) o del suo punteggio frecency, indipendente dal match testuale |
| **Levenshtein** | Distanza di edit che conta inserzioni, cancellazioni e sostituzioni; usato per il fuzzy matching di livello 3 |
| **Soglia adattiva** | Numero massimo di errori tollerati, calcolato automaticamente dalla lunghezza della query (≤4 car → 1, >4 car → 2) |
| **Stats cache** | File TSV in `$XDG_CACHE_HOME` che registra `last_launch_unix` e `launch_count` per ogni app; cache derivata, non configurazione |
| **Alias** | Termine di ricerca aggiuntivo definito dall'utente per un'app specifica; trattato come keyword primaria durante il matching |
| **Score testuale** | Punteggio calcolato dalla corrispondenza tra query e campi dell'app — componente ereditata dal sistema esistente |
| **Score finale** | Score testuale + boost frecency + boost preferiti — determina l'ordine nella lista risultati |
| **Baseline** | Comportamento del motore di ricerca corrente prima di questa milestone |
