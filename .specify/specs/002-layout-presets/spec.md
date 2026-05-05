# Feature Specification: Layout Presets

**Feature Branch**: `002-layout-presets`  
**Created**: 2026-05-05  
**Status**: Draft  
**Input**: User description: "Layout Presets (MeowMenu milestone 002): trasformare le opzioni sparse di Whisker Menu in tre preset iniziali (Classic, Modern, FullScreen) facilmente selezionabili, con possibilità di personalizzare e salvare preset utente, esportarli/importarli e riorganizzare il pannello delle impostazioni. Riferimento: docs/whisker-modernization-spec.md §9.2."

---

## User Scenarios & Testing

### User Story 1 — Modern attivo subito, zero configurazione iniziale (Priority: P1)

Un utente installa MeowMenu (o lo avvia per la prima volta dopo l'aggiornamento dal vecchio Whisker Menu) e apre il menu dal pannello. Senza aver mai toccato le impostazioni, vede il preset **Modern** già applicato: angoli arrotondati, layout categorie-a-sinistra/ricerca-in-basso, doppia opacità, icone applicazioni di dimensione normale. L'utente non ha bisogno di entrare nelle impostazioni per ottenere un'esperienza moderna e curata.

**Why this priority**: È il percorso d'uso più importante in assoluto. Il valore di MeowMenu come fork rispetto a Whisker upstream è proprio questo: il default è già "moderno" senza alcuna configurazione richiesta. Le impostazioni sono solo per chi vuole cambiare. Senza questo, la milestone non raggiunge il proprio obiettivo.

**Independent Test**: Su una macchina in cui MeowMenu non è mai stato configurato, installare il plugin e aprirlo: il menu deve presentarsi come definito da Modern senza alcun intervento dell'utente.

**Acceptance Scenarios**:

1. **Given** MeowMenu è appena stato installato e nessuna impostazione utente è stata salvata, **When** l'utente apre il menu dal pannello, **Then** il menu si presenta con tutte le caratteristiche di Modern (angoli arrotondati, distacco dal pannello, doppia opacità 100%/80%, categorie a sinistra, profilo in alto, ricerca in basso, applicazioni come icone normali, switch categoria al passaggio del mouse).
2. **Given** un'installazione fresca, **When** l'utente apre la scheda General delle impostazioni, **Then** il preset attivo indicato è "Modern".
3. **Given** un'installazione fresca, **Then** l'utente è in grado di usare il menu in modo fluido e produttivo senza mai aprire le impostazioni.

---

### User Story 2 — Cambiare preset quando il default non piace (Priority: P1)

Un utente che preferisce l'esperienza Whisker classica, oppure che vuole provare il layout a tutto schermo in stile GNOME, apre la scheda **General** delle impostazioni, vede i tre preset disponibili con una breve descrizione visiva di ciascuno, ne seleziona un altro e il menu si aggiorna immediatamente.

**Why this priority**: Anche se Modern è il default, la possibilità di scegliere è il cuore della milestone. Senza il cambio di preset funzionante, la feature non esiste.

**Independent Test**: Dalle impostazioni selezionare in sequenza Classic e poi FullScreen, verificare che il menu cambi apparenza e comportamento come da definizione di ciascun preset.

**Acceptance Scenarios**:

1. **Given** Modern è il preset attivo, **When** l'utente seleziona il preset "Classic" nella scheda General, **Then** il menu adotta esattamente le impostazioni default di Whisker Menu upstream e la selezione è confermata visivamente nella UI delle impostazioni.
2. **Given** Modern è il preset attivo, **When** l'utente seleziona il preset "FullScreen", **Then** il menu adotta le impostazioni definite per FullScreen (finestra a tutto schermo, barra di ricerca in alto al centro, categorie in riga, griglia di applicazioni 6×3) e la modifica è immediata senza riavvio.
3. **Given** un preset diverso da Modern è attivo, **When** l'utente seleziona di nuovo "Modern", **Then** il menu torna allo stato Modern.
4. **Given** un preset è attivo, **When** l'utente apre il menu, **Then** l'aspetto corrisponde fedelmente a quello del preset selezionato.

---

### User Story 3 — Affinare un preset con impostazioni di dettaglio (Priority: P2)

Dopo aver scelto un preset (o partendo dal Modern di default), l'utente vuole modificare un singolo aspetto (es. dimensione icone, posizione della barra di ricerca) senza cambiare il resto. Naviga nelle altre schede delle impostazioni (Appearance, Behavior o equivalenti) e modifica liberamente i singoli controlli. Le modifiche si sovrappongono al preset senza perdere il resto della configurazione.

**Why this priority**: I preset devono essere punti di partenza, non gabbie. La personalizzazione granulare è necessaria per utenti intermedi.

**Independent Test**: Partire da "Modern", cambiare la dimensione delle icone nella scheda Appearance, verificare che solo quella impostazione sia variata rispetto al preset.

**Acceptance Scenarios**:

1. **Given** il preset "Modern" è attivo, **When** l'utente modifica la dimensione delle icone in Appearance, **Then** solo la dimensione delle icone cambia; tutte le altre impostazioni del preset rimangono invariate.
2. **Given** l'utente ha modificato alcune impostazioni sopra un preset, **When** torna alla scheda General, **Then** il preset selezionato è ancora visibile come punto di riferimento (es. badge "personalizzato" o asterisco accanto al nome).
3. **Given** l'utente ha personalizzato il preset "Modern", **When** clicca "Ripristina preset", **Then** tutte le impostazioni tornano ai valori definiti per "Modern" e la personalizzazione precedente viene rimossa.

---

### User Story 4 — Salvare la configurazione attuale come preset utente (Priority: P2)

Un utente ha configurato manualmente il menu fino a soddisfarlo e non vuole perdere la configurazione. Dalla scheda General sceglie "Salva come nuovo preset", assegna un nome, e il preset appare nell'elenco insieme ai tre built-in. Può rinominarlo o eliminarlo in seguito.

**Why this priority**: Permette agli utenti di capitalizzare il lavoro di configurazione e di avere più configurazioni rapide.

**Independent Test**: Configurare manualmente almeno due impostazioni, salvare come preset "Mio Layout", chiudere e riaprire le impostazioni, verificare che il preset esista e che applicarlo ripristini le impostazioni salvate.

**Acceptance Scenarios**:

1. **Given** l'utente ha modificato le impostazioni, **When** sceglie "Salva come nuovo preset" e inserisce il nome "Mio Layout", **Then** "Mio Layout" appare nell'elenco dei preset subito dopo il salvataggio.
2. **Given** il preset utente "Mio Layout" è nell'elenco, **When** l'utente lo seleziona, **Then** il menu adotta esattamente le impostazioni che erano attive al momento del salvataggio.
3. **Given** il preset utente "Mio Layout" è nell'elenco, **When** l'utente lo rinomina in "Lavoro", **Then** il preset appare con il nuovo nome senza perdere le impostazioni.
4. **Given** il preset utente "Lavoro" è nell'elenco, **When** l'utente lo elimina, **Then** scompare dall'elenco e la configurazione attiva non cambia.
5. **Given** l'utente tenta di eliminare uno dei tre preset built-in, **Then** l'azione è bloccata e l'utente è informato che i preset di sistema non sono eliminabili.

---

### User Story 5 — Esportare e importare un preset utente (Priority: P3)

Un utente vuole condividere la propria configurazione con un altro computer o con un amico. Dalla scheda General seleziona un preset utente e sceglie "Esporta". Il sistema produce un file di configurazione completo. Su un'altra macchina lo stesso utente (o un altro) sceglie "Importa" e carica il file: il preset appare nell'elenco ed è immediatamente applicabile.

**Why this priority**: Funzionalità utile ma non bloccante; il valore principale della milestone è la selezione e personalizzazione dei preset.

**Independent Test**: Esportare il preset "Mio Layout" in un file, eliminarlo dall'elenco, reimportarlo dal file, verificare che riappaia con tutte le impostazioni originali.

**Acceptance Scenarios**:

1. **Given** il preset utente "Mio Layout" è nell'elenco, **When** l'utente sceglie "Esporta" e seleziona una destinazione, **Then** viene creato un file che contiene tutte le impostazioni del preset in modo leggibile e completo.
2. **Given** il file esportato esiste, **When** l'utente sceglie "Importa" e seleziona il file, **Then** il preset viene aggiunto all'elenco con il nome originale e tutte le impostazioni sono ripristinate.
3. **Given** viene importato un file non valido o corrotto, **Then** l'importazione fallisce con un messaggio comprensibile e nessuna impostazione viene modificata.
4. **Given** viene importato un file con un nome già esistente nell'elenco, **Then** il sistema chiede all'utente se sovrascrivere o rinominare il preset importato.

---

### Edge Cases

- Cosa succede se l'utente applica un preset e poi chiude le impostazioni senza salvare esplicitamente? → Le modifiche devono essere persistite immediatamente all'applicazione del preset.
- Cosa succede se il file di importazione è stato creato con una versione precedente di MeowMenu con impostazioni non più esistenti? → Le impostazioni sconosciute vengono ignorate silenziosamente; quelle valide vengono applicate.
- Cosa succede se l'utente salva un preset utente con un nome identico a un preset built-in (es. "Classic")? → Il sistema deve rifiutare il nome e chiederne uno diverso.
- Cosa succede se l'utente ha apportato modifiche non salvate e seleziona un nuovo preset? → Il sistema avvisa che le modifiche non salvate andranno perse (a meno che non sia già attivo il meccanismo di applicazione immediata con possibilità di "Ripristina").
- Cosa succede al preset utente attivo se MeowMenu viene aggiornato e le sue impostazioni cambiano? → Il preset utente viene conservato così com'è; le impostazioni sconosciute vengono ignorate.

---

## Requirements

### Functional Requirements

**Modello preset come macro**

- **FR-000**: Un preset è un **insieme denominato di valori per impostazioni individuali** (una macro). Applicare un preset equivale ad applicare simultaneamente tutti i valori che quel preset definisce. Ogni impostazione governata da un preset DEVE avere un controllo individuale corrispondente nelle schede di dettaglio, accessibile e modificabile indipendentemente dal preset attivo.

**Preset built-in**

- **FR-001**: Il sistema DEVE fornire tre preset built-in non eliminabili e non rinominabili: **Classic**, **Modern**, **FullScreen**, con le caratteristiche descritte nella sezione Entità Chiave.
- **FR-002**: Su un'installazione fresca di MeowMenu (assenza di configurazione utente preesistente), il preset **Modern** DEVE essere applicato automaticamente come default, senza richiedere alcun intervento dell'utente.
- **FR-003**: L'utente DEVE poter usare MeowMenu in modo completo e produttivo senza mai aprire la finestra delle impostazioni: il default Modern fornisce un'esperienza "out-of-the-box" coerente.
- **FR-004**: Ciascun preset built-in DEVE essere applicabile con un singolo gesto (click/selezione) nella scheda General delle impostazioni.
- **FR-005**: L'applicazione di un preset DEVE essere immediata: il menu aperto o alla prossima apertura DEVE riflettere le nuove impostazioni senza richiedere riavvio del plugin o del pannello.
- **FR-006**: Ogni preset DEVE essere accompagnato da una breve descrizione testuale o da una miniatura/icona rappresentativa nella scheda General, per orientare l'utente prima di applicarlo.
- **FR-007**: Il preset attivo DEVE essere chiaramente indicato nell'elenco delle impostazioni.

**Personalizzazione granulare sui preset**

- **FR-008**: Dopo l'applicazione di un preset, l'utente DEVE poter modificare individualmente qualsiasi impostazione di dettaglio nelle schede Appearance, Behavior e simili senza che questo elimini le impostazioni non toccate del preset.
- **FR-009**: Quando la configurazione attiva differisce dal preset di base (perché sono state apportate modifiche manuali), il sistema DEVE segnalarlo visualmente nella scheda General (es. indicatore "personalizzato").
- **FR-010**: L'utente DEVE poter ripristinare le impostazioni di un preset built-in con un singolo gesto, annullando tutte le personalizzazioni successive.
- **FR-010a**: Le schede di dettaglio DEVONO esporre come controlli individuali almeno le seguenti categorie di impostazioni (l'elenco è non-esaustivo; la pianificazione tecnica può aggiungere controlli pertinenti):
  - *Aspetto visivo*: curvatura degli angoli del menu; opacità della zona categorie/profilo; opacità della zona applicazioni/ricerca (le due aree sono controllate separatamente).
  - *Geometria e posizionamento*: entità del distacco tra la finestra del menu e il pannello; la direzione del distacco è calcolata automaticamente in base alla posizione del pannello (sopra/sotto/sinistra/destra), senza richiedere input esplicito all'utente.
  - *Layout elementi*: posizione della barra di ricerca (alto/basso); posizione della sidebar categorie (sinistra/destra/nascosta); posizione del profilo utente (alto/basso/nascosto).
  - *Griglia applicazioni*: numero di colonne; numero di righe; densità/compattezza (bassa, media, alta); modalità di visualizzazione applicazioni (icone o lista).
  - *Comportamento navigazione*: attivazione categoria al passaggio del mouse (on/off).

**Preset utente**

- **FR-011**: L'utente DEVE poter salvare la configurazione attiva come nuovo preset utente assegnandogli un nome.
- **FR-012**: I preset utente DEVONO essere rinominabili e eliminabili dall'utente in qualsiasi momento.
- **FR-013**: I preset built-in NON DEVONO essere eliminabili né rinominabili.
- **FR-014**: Il numero di preset utente salvabili NON DEVE avere un limite artificiale (il limite naturale è lo spazio disponibile nella configurazione utente).
- **FR-015**: Non DEVE essere possibile creare un preset utente con lo stesso nome di un preset built-in.

**Esportazione e importazione**

- **FR-016**: L'utente DEVE poter esportare un preset utente in un file di configurazione auto-contenuto, utilizzando una finestra di selezione file nativa (nessun terminale richiesto).
- **FR-017**: Il file esportato DEVE contenere tutte le impostazioni del preset in modo completo, tale da permettere la ricostruzione fedele del preset su un'altra installazione.
- **FR-018**: L'utente DEVE poter importare un preset da file tramite finestra di selezione file nativa; il preset importato appare nell'elenco dei preset utente.
- **FR-019**: In caso di importazione di un file non valido o corrotto, il sistema DEVE informare l'utente con un messaggio comprensibile e non modificare alcuna impostazione esistente.
- **FR-020**: In caso di conflitto di nome durante l'importazione, il sistema DEVE chiedere all'utente se sovrascrivere o rinominare.

**Riorganizzazione del pannello impostazioni**

- **FR-021**: La scheda **General** DEVE diventare il punto centrale per la gestione dei preset (selezione, salvataggio, esportazione, importazione, rinomina, eliminazione preset utente).
- **FR-022**: Le schede **Appearance** e **Behavior** (o schede equivalenti, eventualmente rinominate o suddivise) DEVONO esporre in modo organizzato tutte le impostazioni di dettaglio che costituiscono e sovrascrivono i preset; nessuna impostazione rilevante DEVE restare nascosta o irraggiungibile.
- **FR-023**: Le impostazioni relative alla ricerca DEVONO essere raggruppate in una scheda dedicata (es. "Advanced Search") o eliminate se ridondanti rispetto alle funzionalità introdotte nella milestone Search Ranking 2.0.
- **FR-024**: La struttura complessiva delle schede delle impostazioni DEVE essere coerente, intuitiva e non richiedere documentazione esterna per essere navigata da un utente non tecnico.

---

### Key Entities

- **Preset built-in**: Configurazione di sistema non modificabile che raccoglie un insieme coerente di impostazioni. Attributi rilevanti: nome, descrizione breve, insieme delle impostazioni coperte. I tre preset built-in sono definiti come segue:
  - *Classic*: replica esatta delle impostazioni default di Whisker Menu upstream. Punto di riferimento per utenti che conoscono già Whisker.
  - *Modern*: stile visivo aggiornato — angoli arrotondati, lieve distacco dal pannello proporzionale alla posizione del pannello, doppia opacità (categorie 100%, applicazioni/ricerca 80% di default), categorie a sinistra, profilo utente in alto, barra di ricerca in basso, applicazioni visualizzate come icone di dimensione normale, switch di categoria al passaggio del mouse.
  - *FullScreen*: finestra che occupa tutto lo spazio disponibile sullo schermo, barra di ricerca centrata in alto, categorie in riga orizzontale sotto la barra (default "Tutte le applicazioni"), griglia di applicazioni 6 colonne × 3 righe centrata con margini, compattezza della griglia regolabile (bassa/media/alta, default media), comandi di sistema (logout / blocco / spegnimento) e icona profilo utente in basso a destra.
- **Preset utente**: Configurazione salvata dall'utente. Attributi rilevanti: nome (univoco tra tutti i preset), insieme completo delle impostazioni al momento del salvataggio. Operazioni: creare, rinominare, eliminare, esportare, importare.
- **Configurazione attiva**: Lo stato corrente di tutte le impostazioni di MeowMenu. Può coincidere con un preset o esserne una variante personalizzata.
- **File di preset**: Rappresentazione su disco di un preset utente, prodotta dall'esportazione. Deve essere auto-contenuta e versionata per gestire la compatibilità con versioni future.

---

## Success Criteria

### Measurable Outcomes

- **SC-001**: Su un'installazione fresca, alla prima apertura del menu l'utente vede il preset Modern già applicato in tutti i suoi aspetti, senza alcun passaggio nelle impostazioni.
- **SC-002**: Un utente che vuole solo "usare il menu" e accetta il default Modern non ha bisogno di aprire la finestra delle impostazioni in nessun momento del flusso di adozione.
- **SC-003**: Un utente può passare da un preset all'altro e vedere il menu aggiornato entro il tempo necessario per riaprirlo — nessun riavvio del pannello o del plugin è richiesto.
- **SC-004**: Un utente non tecnico, senza leggere documentazione, riesce a selezionare un preset diverso dal default, personalizzare almeno un'impostazione e ripristinare il preset originale in meno di 3 minuti.
- **SC-005**: Un utente riesce a salvare la propria configurazione come preset, esportarla su file e reimportarla su una seconda macchina con configurazione identica, compiendo l'intera operazione senza usare il terminale.
- **SC-006**: Tutti i preset built-in sopravvivono invariati a un aggiornamento di MeowMenu; i preset utente vengono conservati o migrati senza perdita di dati.
- **SC-007**: Nessuna impostazione precedentemente accessibile nell'interfaccia viene resa irraggiungibile dalla riorganizzazione del pannello delle impostazioni.
- **SC-008**: L'importazione di un file corrotto non causa crash, perdita di dati o modifiche non intenzionali alla configurazione esistente.

---

## Assumptions

- **Modern come default**: Su un'installazione fresca senza configurazione utente preesistente, MeowMenu applica Modern automaticamente. Su installazioni esistenti che ereditano la configurazione di Whisker Menu (upgrade in-place), il sistema rispetta la configurazione precedente — non sovrascrive impostazioni utente esistenti senza consenso. La promozione di Modern come default vale solo per stati "puliti".
- I tre preset built-in non coprono necessariamente tutte le impostazioni esistenti di MeowMenu: le impostazioni non definite in un preset rimangono invariate rispetto alla configurazione attiva al momento dell'applicazione del preset; solo le impostazioni esplicitamente definite dal preset vengono sovrascritte.
- Il formato del file di esportazione preset è definito nella fase di pianificazione tecnica; i requisiti funzionali richiedono solo che sia auto-contenuto, leggibile da esseri umani (es. testo strutturato) e compatibile con versioni future tramite un meccanismo di migrazione.
- La scheda General può essere rinominata o riorganizzata nella fase di pianificazione, purché rimanga il punto di accesso unico alla gestione dei preset.
- Il supporto Wayland per le funzionalità legate a FullScreen (finestra a tutto schermo) è classificato come **X11-first with Wayland fallback**: su Wayland il preset FullScreen è selezionabile ma il comportamento a tutto schermo potrebbe essere limitato o non disponibile, con un avviso esplicito all'utente.
- **Nuovi controlli vs. controlli esistenti**: diversi controlli individuali elencati in FR-010a non esistono in Whisker Menu upstream (es. curvatura angoli, doppia opacità, distacco dal pannello, densità griglia). Questa milestone ha la responsabilità di introdurli come impostazioni Xfconf con i valori di default definiti dal preset Modern. La pianificazione tecnica deve identificare quali controlli sono nuovi e quali già presenti.
- La milestone non include un editor visivo avanzato dei colori/temi (quello è milestone 003 — Theme Editor); le impostazioni esposte nelle schede Appearance e Behavior riguardano layout, dimensioni, comportamento, non temi grafici.
- Il numero massimo di preset utente non è definito esplicitamente: il sistema non impone un limite artificiale, ma un numero ragionevole per un singolo utente è nell'ordine delle decine.
