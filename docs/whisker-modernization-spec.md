# Specifica funzionale e tecnica
## Progetto: fork moderno di xfce4-whiskermenu-plugin per Xubuntu 26.04 / Xfce 4.20

Versione: 1.0  
Data: 2026-04-22  
Formato: documento di specifica per sviluppo spec-driven con Codex

---

## 1. Executive summary

L’obiettivo del fork è evolvere **Whisker Menu** da launcher classico molto solido a **launcher moderno, più discoverable e più personalizzabile**, senza trasformarlo in una shell alternativa e senza rompere i principi storici di Xfce: leggerezza, prevedibilità, integrazione nativa e configurazione semplice.

Whisker oggi offre già preferiti, recenti, ricerca immediata, drag&drop dei preferiti, categorie, proprietà divise in più tab e varie funzioni moderne aggiunte nel tempo come desktop actions, search actions, supporto a Xfconf, popup centrato a schermo, resize da bordi e diversi fix Wayland/multi-monitor. La base del progetto è quindi valida e matura: il fork non deve reinventare il launcher, ma colmare i gap di UX, configurabilità e architettura estensibile.

Il fork deve evolvere in tre direzioni:

1. **migliore UX di default**, simile alla frizione bassa di GNOME e KDE;
2. **personalizzazione profonda ma comprensibile**, più vicina a KDE;
3. **architettura estendibile**, senza introdurre dipendenze pesanti o servizi residenti obbligatori.

La regola di progetto è:

> **Classico per default, moderno quando desiderato, avanzato solo quando richiesto.**

---

## 2. Baseline tecnica reale

### 2.1 Stack attuale

Whisker Menu è un plugin Xfce scritto quasi interamente in **C++**, con build **Meson**, dipendenze principali **GTK 3**, **garcon**, **libxfce4panel**, **libxfce4ui**, **libxfce4util** e **xfconf**. Sono inoltre presenti feature opzionali via **AccountsService** e **gtk-layer-shell**.

### 2.2 Integrazione nel pannello Xfce

Dal lato Xfce, i panel plugin possono essere interni o esterni; esiste anche un esempio Python, ma il modello standard del pannello, i widget e la registrazione del plugin passano da **libxfce4panel** e dal sistema plugin nativo del pannello.

Per un launcher di uso quotidiano, sensibile a focus, popup, orientamento e posizione schermo, il percorso più solido resta il **plugin nativo GTK/C++**.

### 2.3 Garcon e menu spec

Whisker usa **Garcon**, che implementa la menu specification di freedesktop.org. Questo implica che categorie applicative, menu custom e compatibilità con editor dei menu debbano restare allineati al modello freedesktop/Xfce.

### 2.4 Configurazione

Le impostazioni di Whisker sono memorizzate in **Xfconf**, che è il sistema di configurazione standard di Xfce e propaga subito i cambiamenti tra GUI e applicazioni. Tutte le nuove feature devono continuare a usare Xfconf come backend di configurazione.

---

## 3. Target platform e assunzioni di progetto

### 3.1 Target primario

Prima piattaforma supportata:

- **Xubuntu 26.04**
- **Xfce 4.20.x**
- sessione **X11** come percorso di qualità principale
- sessione **Wayland** come percorso compatibile ma non feature-complete

### 3.2 Compatibilità supportata

Il fork deve essere:

- pienamente usabile senza terminale;
- installabile come plugin di pannello tradizionale;
- configurabile interamente via GUI;
- degradabile con grazia quando mancano feature opzionali o librerie opzionali.

### 3.3 Strategia di release engineering

Conviene assumere due baseline:

- **baseline distro**: ciò che Xubuntu 26.04 fornisce out of the box;
- **baseline progetto**: ciò che il fork può pacchettizzare o backportare sopra la base distro.

Questo evita di dipendere da una versione specifica di Whisker già presente nei repository della distro.

---

## 4. Visione di prodotto

### 4.1 Posizionamento

Il fork non deve competere con una shell intera. Deve essere descritto come:

> **Application launcher and search hub for Xfce, modern by default, deeply configurable, natively integrated.**

### 4.2 Esperienza utente desiderata

All’apertura, l’utente deve percepire tre cose:

- il menu risponde subito;
- ciò che cerca compare rapidamente;
- ciò che non sa configurare lo può comunque modificare da interfaccia.

### 4.3 Fonte d’ispirazione

- da **GNOME**: fluidità del “premi Super e inizia a scrivere”, overview, immediatezza della ricerca;
- da **KDE**: places, recenti, potenza della ricerca, desktop actions, personalizzazione modulare.

### 4.4 Principio guida UX

> **Default sobrio XFCE, potenza opt-in, tutto configurabile da GUI.**

---

## 5. Non-obiettivi

Il fork **non** deve:

- diventare una shell full-screen obbligatoria in stile GNOME Shell;
- richiedere un daemon separato sempre attivo;
- sostituire il sistema di settings di Xfce;
- dipendere da indicizzatori desktop pesanti come requisito obbligatorio;
- introdurre una UI web/HTML/JavaScript;
- partire con una riscrittura totale in Python;
- richiedere GTK4 come prerequisito iniziale.

---

## 6. Vincoli architetturali

### 6.1 Stack

Mantenere **C++ + GTK3 + librerie Xfce** come stack core.

Una riscrittura in Python aumenterebbe il rischio su:

- integrazione col pannello;
- packaging e distribuzione;
- startup path;
- debugging di popup/focus/embedding;
- regressioni su X11/Wayland e multi-monitor.

### 6.2 Configurazione

Ogni nuova feature utente deve:

- avere una property in Xfconf;
- comparire in GUI;
- avere default sicuri;
- supportare reset al default;
- essere esportabile in profili configurativi in una fase successiva.

### 6.3 Compatibilità applicazioni/menu

La fonte primaria delle applicazioni resta **Garcon/freedesktop menu spec**.

Categorie, launchers, desktop actions, custom menu files e comportamento del menu devono continuare a poggiare su quel layer.

### 6.4 Wayland

Su Wayland il fork deve puntare a:

- posizionamento corretto del popup;
- multi-monitor corretto;
- fallback chiari per funzioni non garantibili.

Non deve promettere parità piena con X11.

---

## 7. Analisi dello stato funzionale attuale di Whisker

Whisker oggi fornisce già:

- lista preferiti;
- lista delle app lanciate recentemente;
- ricerca con focus automatico del campo search;
- drag&drop e riordino dei preferiti;
- impostazioni suddivise in tab;
- supporto a search actions;
- supporto a desktop actions;
- supporto a launcher autostart;
- popup via comando dedicato;
- memorizzazione configurazione in Xfconf;
- popup centrato e resize da bordi;
- fix recenti per Wayland e multi-monitor.

### 7.1 Gap reali da colmare

I gap reali del progetto non sono le funzioni di base, ma:

1. ranking della ricerca;
2. discoverability delle impostazioni;
3. theming/menu appearance semplificati;
4. places / recent docs / frequent come primitive di UX più moderne;
5. architettura provider per una ricerca più universale.

---

## 8. Persona e casi d’uso

### 8.1 Persona primaria

Utente Xubuntu che:

- non usa la linea di comando;
- installa app da GUI;
- vuole un desktop classico;
- vuole però ritrovare comodità viste in GNOME/KDE.

### 8.2 Persona secondaria

Utente avanzato Xfce che:

- ama personalizzare layout e comportamento;
- vuole evitare shell pesanti;
- accetta opzioni avanzate purché non invasive.

### 8.3 Casi d’uso chiave

- premere Super, digitare 2–4 lettere, lanciare subito un’app;
- ritrovare documenti/cartelle recenti senza aprire il file manager;
- personalizzare layout e densità del menu dalla GUI;
- accedere a places, power/session e quick actions in modo coerente;
- usare il menu bene anche con tastiera, HiDPI e multi-monitor.

---

## 9. Feature conservative: specifica funzionale e tecnica

Queste feature costituiscono la **linea principale della v1**.

### 9.1 Search Ranking 2.0

#### Obiettivo
Rendere la ricerca comparabile a un launcher moderno.

#### Requisiti funzionali

- matching su nome, generic name, keyword e description;
- boost per preferiti e app recenti;
- typo tolerance leggera;
- alias/abbreviazioni configurabili;
- ordinamento stabile e comprensibile;
- evidenziazione del motivo del match in modalità debug avanzata.

#### Requisiti tecnici

- nessun indexer esterno obbligatorio;
- algoritmo in memoria sui dati già caricati;
- costo CPU controllato;
- debounce minimo configurabile.

#### Non-goal

- full text search di file system;
- ricerca semantica AI.

#### Criteri di accettazione

- la ricerca deve mantenere reattività percepita immediata su installazioni desktop tipiche;
- le app preferite e recenti devono emergere a parità di punteggio base;
- il ranking deve risultare prevedibile e non “magico”.

### 9.2 Layout Presets

#### Obiettivo
Trasformare molte opzioni sparse in preset facili da capire.

#### Preset iniziali

- Classic 
- Modern
- FullScreen

#### Requisiti

Ogni preset deve essere solo una macro su impostazioni esistenti e nuove, interamente reversibile e modificabile manualmente.

Di seguito un breve descrizione di come immgino i preset:
1. Classic è esattamente il Whisker Menu nelle suo opzioni di default
2. Modern è la scelta che vorrei visualizzata di default e che comprende una serie di migliorie:
- angolatura bordi presente, per rendere più moderno il menu
- lieve distacco della finestra del menu dalla pannello in cui è alloggiato (se il pannello è in basso, il menu è leggermente più in alto del Classic, se il pannello è a sinistra, il menu è leggermente più a destra, etc)
- possibilità di gestire l'opacità a due livelli: quella delle categorie/profili e quella delle applicazioni/ricerca (per qualche ragione adesso l'opacità controlla solo la zona delle categorie). Opacità delle categorie deve essere di default al 100% e delle applicazioni/ricerca all'80%
- categorie mostrate a sinistra, profilo mostrato in alto, barra di ricerca mostrata in basso
- applicazioni mostrate come icone e non come lista di default, icona delle applicazioni di dimensione Normale di default
- "switch category by hovering" selezionata di default
- non considerare queste specifiche esaustive, prendi esempio dal menu applicazioni/attività di default delle ultime versioni di Cinnamon o KDE per cercare una implementazione minimale idonea a XFCE
3. FullScreen cerca di prendere spunto dal menu/attività di GNOME:
- mostra una finestra di menu che occupa tutto lo spazio che una finesta potrebbe occupare
- mostra di default una barra di ricerca in alto e in centro
- sotto la barra sono presenti, in riga, tutte le categorie, di default è selezionato "All Applications"
- di default passando il mouse sopra le categorie o cliccandoci si seleziona una diversa categoria
- sotto la riga con tutte le categorie vengono mostrate in griglia le applicazioni con il loro nome sottostante e senza descrizione
- è possibile modificare la griglia di applicazioni, di default 6 (colonne) x 3 (righe) che è centrata e lascia spazio ai margini. si può anche modificare basso-medio-alta la compattezza della griglia, di default su media
- in basso a destra di defaul sono presenti i comandi (di default si mostrano solo le icone log-out / lock / shut-down) e l'icona del profilo utente
- non considerare queste specifiche esaustive, prendi esempio dal menu applicazioni/attività di default delle ultime versioni di GNOME per cercare una implementazione minimale idonea a XFCE


#### Criteri di accettazione

- applicazione immediata del preset;
- preview o descrizione sintetica;
- possibilità di annullare o tornare ai default.
- possibilità di modificare ulteriormente con le impostazioni esistenti o nuove di dettaglio
- possibilità di salvare tutte le impostazioni attuali in un nuovo preset definito e rinominato dall'utente.
- possibilità di scaricare un preset peronalizzato dall'utente in un file di config con tutte le impostazioni complete
- possibilità di caricare un preset personalizzato da file di config
- tutti i preset tranne i tre presenti inizialmente possono essere rinominati e rimossi

#### Revisione del pannello delle impostazioni di MeowMenu
le finestre General / Appearance / Behavior devono cambiare radicalmente contenuto:
- General può chiamarsi ancora così (in inglese) e dovrebbe consentire di selezionare un Preset o salvarne / scaricarne / modificarne / rinominarne uno personalizzato
- Appearence e Behavior possono cambiare radicalmente contenuto per alloggiare tutte le specifiche possibili e immaginabili che sottendono i preset
- le impostazioni relative alla ricerca possono essere inserite nella tab Advanced Search o eliminate se ridondanti
- Appearance e Behavior possono cambiare nome o si possono aggiungere ulteriori finestre tematiche purchè tutto resti consistente, intuitivo, ben strutturato ed organizzato.

### 9.3 Theme Editor visuale del menu

#### Obiettivo
Portare personalizzazione “alla KDE” senza far toccare CSS a mano.

#### Controlli

- compattezza;
- padding globale;
- raggio angoli;
- shadow intensity;
- opacità sfondo;
- sidebar style;
- dimensione icone;
- bordi e separatori;
- override dark/light del menu.

#### Vincolo

Il theme editor deve agire **solo sul menu**, non sul tema desktop globale.

#### Criteri di accettazione

- anteprima rapida o applicazione live;
- reset totale della sezione;
- nessuna richiesta di editing manuale di CSS per uso base.

### 9.4 Places Pane

#### Obiettivo
Avvicinare il launcher a Kickoff con una colonna “Luoghi”.

#### Contenuti minimi

- Home
- Desktop
- Documenti
- Download
- Musica
- Immagini
- Video
- filesystem root opzionale
- mount/removable media opzionali
- bookmark utente di Thunar/GTK

#### Vincolo

I Places non devono dipendere da Thunar in esecuzione.

#### Criteri di accettazione

- i bookmark utente devono comparire in modo coerente;
- i mount point rimovibili devono essere opzionali;
- l’intera colonna deve essere attivabile/disattivabile.

### 9.5 Recent Hub 2.0

#### Obiettivo
Separare concetti che oggi l’utente percepisce come distinti.

#### Sezioni

- Recent Applications
- Frequent Applications
- Recent Documents
- Recent Folders

#### Requisiti

- clear history per sezione;
- limit configurabile;
- possibilità di disattivare una o più sezioni.

#### Criteri di accettazione

- nessuna sezione deve apparire vuota senza spiegazione;
- la pulizia cronologia deve essere semplice e reversibile dove possibile;
- le sezioni devono poter essere riordinate.

### 9.6 Favorites 2.0

#### Obiettivo
Rendere i preferiti una home personale davvero utile.

#### Requisiti

- separatori;
- gruppi/cartelle;
- pin to top;
- blocco layout;
- drag&drop migliorato;
- import/export;
- reset rapido.

#### Criteri di accettazione

- l’utente deve poter costruire una home applicativa personale senza terminale;
- il comportamento di drag&drop deve essere chiaro e consistente.

### 9.7 Inline Quick Actions

#### Obiettivo
Ridurre il numero di click dopo il risultato di ricerca.

#### Azioni

- Add/Remove favorite
- Desktop actions dal `.desktop`
- Add to desktop
- Add to panel
- Open containing category
- Launch at login, quando coerente

#### Criteri di accettazione

- le azioni già esistenti devono diventare più visibili;
- le azioni non disponibili devono essere nascoste o disabilitate in modo comprensibile.

### 9.8 Running App Indicators

#### Obiettivo
Indicare se un’app è già aperta e facilitare il ritorno alla finestra.

#### Requisiti

- badge/dot discreto;
- jump-list delle finestre aperte opzionale;
- comportamento limitato e chiaro su Wayland.

#### Criteri di accettazione

- su X11 la feature deve essere affidabile;
- su Wayland deve degradare con grazia se l’infrastruttura non espone dati equivalenti.

### 9.9 Accessibility & Keyboard Pass

#### Obiettivo
Portare il menu a un livello di qualità input superiore.

#### Requisiti

- ordine focus prevedibile;
- visibilità del focus;
- frecce, PgUp/PgDn, Esc, Tab coerenti;
- target touch-friendly opzionali;
- label accessibili per screen reader.

#### Criteri di accettazione

- navigazione completa da tastiera;
- nessuna regressione nella search entry;
- focus sempre visibile.

### 9.10 Profili utente e backup configurazione

#### Obiettivo
Dare sicurezza agli utenti non tecnici.

#### Requisiti

- export/import profilo;
- restore defaults;
- preset clonabili;
- backup automatico prima di “Apply preset”.

#### Criteri di accettazione

- ripristino semplice;
- export/import senza terminale;
- profili chiaramente nominabili da GUI.

---

## 10. Feature disruptive ma coerenti con Xfce/Whisker

Queste feature non devono essere il default iniziale. Devono arrivare come **capability opzionali**.

### 10.1 Provider-based Search System

#### Obiettivo
Evolvere le search actions in un modello più vicino a KRunner.

#### Provider iniziali

- Applications
- Xfce Settings
- Favorites
- Recent Applications
- Recent Documents
- Places
- Calculator
- Unit Conversion
- Web Search

#### Vincoli tecnici

- provider caricati lazy;
- nessun provider deve bloccare il thread UI;
- provider disabilitabili da GUI;
- ordine provider configurabile.

### 10.2 Runner separato / Command Palette

#### Obiettivo
Aggiungere una modalità “search-only” in finestra compatta, distinta dal menu classico.

#### UX

- attivabile con shortcut dedicata;
- singolo campo search;
- risultati in lista;
- frecce + Enter;
- comportamento molto rapido.

#### Requisito di prodotto

Il runner non sostituisce Whisker; è una seconda interfaccia dello stesso motore.

### 10.3 Dashboard / Overview mode

#### Obiettivo
Offrire una modalità grande, centrata o quasi fullscreen, ispirata alla facilità dell’overview GNOME.

#### Componenti

- search top-centered;
- favorites/app grid;
- recenti;
- places;
- power/session block;
- optional running apps strip.

#### Vincolo

Deve poter essere completamente disattivata.

### 10.4 App Grid con cartelle custom

#### Obiettivo
Consentire organizzazione personale oltre le categorie freedesktop.

#### Requisiti

- cartelle utente create da GUI;
- drag&drop app dentro cartelle;
- cartelle locali al launcher, senza alterare i `.desktop` reali;
- export/import con il profilo.

### 10.5 Mini Quick Settings bridge

#### Obiettivo
Avvicinare il launcher alla comodità moderna senza rifare il pannello.

#### Prima fase

Entry intelligenti e launchers verso:

- Network settings
- Bluetooth
- Audio
- Display
- Power
- Appearance

#### Vincolo

Nella prima versione evitare toggle diretti se non esiste un’API affidabile e stabile.

### 10.6 Contextual Profiles / Activities-lite

#### Obiettivo
Cambiare insieme layout, preferiti e provider per contesti diversi.

#### Esempi

- Work
- Home
- Gaming
- Minimal
- Accessibility

#### Vincolo

Nessuna automazione “magica” nella v1; cambio profilo solo manuale.

### 10.7 Search Suggestions for Missing Apps

#### Obiettivo
Quando la ricerca non trova nulla, guidare l’utente.

#### Output possibili

- suggerimento parole correlate;
- apri software center;
- suggerimento pacchetto noto, solo se provider distro-specifico attivo.

#### Vincolo

Funzione modulare e distro-aware; non hardcodata nel core.

### 10.8 Windows & Workspaces Lite

#### Obiettivo
Mostrare lo stato del desktop senza replicare il task manager.

#### Requisiti

- elenco finestre aperte opzionale;
- filtro workspace corrente;
- jump to window.

#### Vincolo

Feature best-effort su Wayland e piena solo dove l’infrastruttura Xfce la supporta bene.

### 10.9 Plugin API interna per provider

#### Obiettivo
Evitare che tutto il fork debba crescere nel core.

#### Modello

- provider built-in per v1;
- API interna stabile ma non ancora pubblica;
- ABI/plugin esterni solo dopo stabilizzazione.

### 10.10 Touch/Handheld mode

#### Obiettivo
Preparare il launcher a schermi piccoli e input ibridi.

#### Requisiti

- scaling preset;
- grid ampia;
- target grandi;
- scroll fluido;
- zero gesture obbligatorie.

---

## 11. Priorità di implementazione consigliata

### Fase 1 — Foundation UX

1. Search Ranking 2.0  
2. Layout Presets  
3. Theme Editor visuale  
4. Places Pane  
5. Recent Hub 2.0

### Fase 2 — Power without complexity

6. Favorites 2.0  
7. Inline Quick Actions  
8. Running App Indicators  
9. Accessibility & Keyboard Pass  
10. Profili e backup configurazione

### Fase 3 — Signature features

11. Provider-based Search  
12. Runner separato  
13. Dashboard mode  
14. App Grid con cartelle custom  
15. Quick Settings bridge leggero

---

## 12. Specifica UI/UX

### 12.1 Regole UX generali

- zero sorprese all’apertura;
- animazioni minime o assenti di default;
- tutte le novità devono avere descrizioni chiare nel pannello impostazioni;
- ogni sezione avanzata deve avere un testo “spiega in una frase cosa cambia”.

### 12.2 Information architecture delle impostazioni

Le impostazioni devono essere riorganizzate in:

- **General**
- **Layout**
- **Appearance**
- **Search**
- **Content**
- **Behavior**
- **Advanced**
- **Profiles**

### 12.3 Discoverability

Ogni pannello deve avere:

- toggle principale;
- preview o descrizione breve;
- pulsante reset sezione;
- link “advanced options” solo quando serve.

---

## 13. Specifica di configurazione

### 13.1 Xfconf

Tutte le impostazioni devono stare nel canale del plugin in Xfconf. Non introdurre un database separato.

### 13.2 Namespace consigliati

- `/search/*`
- `/layout/*`
- `/appearance/*`
- `/content/*`
- `/providers/*`
- `/profiles/*`

### 13.3 Migrazioni

Ogni release che cambia schema deve includere:

- migrazione forward automatica;
- backup della configurazione precedente;
- compatibilità almeno per due release del fork.

---

## 14. Performance budget

### 14.1 Apertura menu

Target:

- apertura percepita immediata;
- nessun blocco visibile del thread UI.

### 14.2 Ricerca

Target:

- aggiornamento risultati a ogni battuta senza lag percepibile su installazioni desktop normali;
- fallback progressivo per provider lenti.

### 14.3 Memoria

- nessun daemon persistente richiesto;
- caching solo in-process.

---

## 15. Accessibilità, input e localizzazione

### 15.1 Localizzazione

Il fork deve preservare il livello tradizionale di localizzazione di Whisker e non introdurre stringhe UI non traducibili.

### 15.2 Input

- tastiera come first-class citizen;
- mouse e touch supportati senza ridurre la precisione del focus;
- IME/input methods non devono rompersi durante la ricerca.

---

## 16. Compatibilità X11 / Wayland

### 16.1 X11

Percorso di qualità primaria.

### 16.2 Wayland

Percorso supportato ma con capability matrix documentata.

### 16.3 Regola di sviluppo

Ogni feature nuova deve essere classificata come:

- X11 + Wayland parity
- X11-first with Wayland fallback
- unavailable on Wayland for now

---

## 17. Test strategy

### 17.1 Test matrix minima

- Xubuntu 26.04 X11
- Xubuntu 26.04 Wayland
- pannello orizzontale top
- pannello orizzontale bottom
- pannello verticale left/right
- single monitor
- dual monitor
- HiDPI

### 17.2 Test funzionali essenziali

- apertura/chiusura con mouse;
- apertura/chiusura con shortcut;
- ricerca app;
- ricerca settings;
- aggiunta/rimozione preferiti;
- drag&drop;
- recenti;
- places;
- profili;
- reset config;
- toggle del pannello autohide.

### 17.3 Test regressivi prioritari

- monitor corretto del popup;
- focus della search entry;
- comportamento Esc/Enter;
- resize del menu;
- position center screen;
- search actions e desktop actions.

---

## 18. Packaging e distribuzione

### 18.1 Build system

Mantenere **Meson**.

### 18.2 Dipendenze

#### Core

- GTK3
- garcon
- libxfce4panel
- libxfce4ui
- libxfce4util
- xfconf

#### Optional

- AccountsService
- gtk-layer-shell

### 18.3 Policy

Le feature modernizzanti non devono trasformare dipendenze opzionali in hard dependency salvo beneficio molto forte e giustificato.

### 18.4 Pacchetti

Per Xubuntu 26.04 suggerita una strategia a doppio pacchetto:

- pacchetto fork installabile affiancato all’upstream per test;
- eventuale package replace solo in una fase successiva.

---

## 19. Decisioni esplicite

### 19.1 Da fare

- evoluzione incrementale del codice nativo;
- forte uso di Xfconf;
- preset e profili;
- provider search architecture;
- modalità moderne opzionali.

### 19.2 Da non fare

- rewrite Python;
- rewrite GTK4 immediato;
- frontend web;
- daemon di indicizzazione obbligatorio;
- feature distro-specifiche nel core.

---

## 20. Deliverable attesi per Codex

### Deliverable A
Documento di architettura interna:

- mappa moduli esistenti;
- punti di estensione;
- dipendenze;
- flussi di apertura, rendering e ricerca.

### Deliverable B
Refactor preparatorio:

- separazione search engine / UI;
- separazione content providers / rendering;
- schema Xfconf versionato.

### Deliverable C
Implementazione Fase 1:

- Search Ranking 2.0
- Layout Presets
- Theme Editor
- Places Pane
- Recent Hub 2.0

### Deliverable D
Test suite e checklist regressiva.

### Deliverable E
Implementazione Fase 2 e poi Fase 3.

---

## 21. Prompt operativo sintetico da dare a Codex

Lavora su un fork di xfce4-whiskermenu-plugin per Xfce 4.20 / Xubuntu 26.04. Mantieni stack C++/GTK3/Meson/Xfce native libraries. Non introdurre frontend web, non fare rewrite in Python, non richiedere daemon esterni obbligatori. Tratta X11 come primary quality path e Wayland come supported-with-fallback path. Conserva Garcon come source of truth per applicazioni/categorie e Xfconf come source of truth per configurazione. Implementa in modo spec-driven le seguenti milestone: 1) separazione backend ricerca/UI, 2) Search Ranking 2.0, 3) Layout Presets, 4) Theme Editor visuale del solo menu, 5) Places Pane, 6) Recent Hub 2.0, 7) Favorites 2.0, 8) provider-based search architecture. Ogni feature deve avere GUI, default sicuri, reset, migrazione config e test regressivi.

---

## 22. Workflow consigliato per Ubuntu + VSCodium + plugin Codex

### 22.1 Workflow raccomandato: Codex + Spec Kit + GitHub review

Per il tuo caso d’uso, il workflow più adatto è:

1. **Spec-first** con una struttura tipo **Spec Kit**
2. **Implementazione locale** in **VSCodium + plugin Codex**
3. **Task lunghi delegati a Codex Cloud**
4. **Review finale su PR** con Codex in GitHub

Questo è il miglior compromesso tra rigore, controllo e velocità.

#### Perché questa è la scelta migliore

- Codex supporta in modo ufficiale **IDE extension**, **CLI**, **cloud delegation** e **review in GitHub**;
- Codex permette di scegliere **approval mode**, **sandbox** e policy di rete, quindi è adatto a un repository di sistema dove vuoi controllo stretto sui comandi;
- **Spec Kit** è oggi un toolkit concreto per **Spec-Driven Development** e dichiara integrazione con **Codex CLI**, con workflow, preset e skill dedicate.

#### Flusso operativo consigliato

##### Fase A — Specification

- scrivi o aggiorna `spec.md`
- genera `plan.md`
- genera `tasks.md`
- definisci criteri di accettazione e test regressivi

##### Fase B — Architettura e refactor locale

In VSCodium:

- usa Codex in modalità **Chat** per capire il codice
- passa a **Agent** per refactor piccoli e verificabili
- mantieni sandbox stretta sul workspace del fork

##### Fase C — Task lunghi o esplorativi

Per lavori come:

- refactor search engine
- separazione provider/UI
- prototipi di dashboard mode

usa **Codex Cloud** dal plugin IDE, così puoi delegare task più lunghi senza bloccare il lavoro locale.

##### Fase D — Review e hardening

- apri una PR per ogni milestone
- chiedi review automatica o manuale con `@codex review`
- usa follow-up mirati su problemi trovati

#### Struttura repo consigliata

```text
/specs/
  001-search-ranking/
    spec.md
    plan.md
    tasks.md
  002-layout-presets/
    spec.md
    plan.md
    tasks.md
/docs/
  architecture/
  ux/
/.codex/
  config.toml
  AGENTS.md
```

### 22.2 Configurazione Codex consigliata per questo progetto

Per un plugin desktop Xfce/C++ il profilo ideale è conservativo:

- sandbox `workspace-write`
- rete disattivata di default
- approval policy prudente
- comandi build/test esplicitamente consentiti

#### Regole pratiche

- consenti sempre lettura del repo;
- consenti build locali del progetto;
- chiedi approvazione per modifiche fuori workspace;
- chiedi approvazione per rete;
- vieta scritture automatiche sulla `.git` salvo casi controllati.

### 22.3 Quando usare Spec Kit

Usa **Spec Kit** come workflow principale se vuoi:

- backlog molto disciplinato;
- tracciabilità feature → piano → task → implementazione;
- prompt e template riusabili;
- sviluppo spec-driven reale, non solo “prompt lunghi”.

### 22.4 Quando usare GSD

**GSD** è interessante come sistema di meta-prompting/context engineering e sviluppo spec-driven, ma per il tuo caso io lo vedrei come **opzione secondaria o sperimentale**, non come workflow principale.

Motivi:

- è più orientato a orchestrazioni lunghe e meta-workflow;
- nel tuo setup il punto forte è già Codex con IDE/CLI/cloud;
- aggiungere GSD all’inizio rischia di introdurre complessità di processo prima ancora che il fork abbia una baseline pulita.

### 22.5 Raccomandazione finale

Per questo progetto sceglierei:

> **Primary workflow:** Codex + Spec Kit + GitHub PR review  
> **Secondary workflow:** GSD solo più avanti, se vorrai una pipeline più orchestrata per task multi-fase e multi-agent

### 22.6 Routine concreta settimanale

#### Per ogni feature

1. crea cartella spec dedicata
2. scrivi spec funzionale e criteri di accettazione
3. chiedi a Codex analisi architetturale del codice coinvolto
4. fai refactor minimo preparatorio
5. implementa in branch separato
6. build + test locale su Ubuntu/Xubuntu
7. apri PR
8. esegui `@codex review`
9. correggi regressioni
10. merge solo dopo checklist X11/Wayland minima

---

## 23. Conclusione

La direzione giusta non è “fare Whisker come GNOME” o “fare Whisker come KDE”. La direzione giusta è:

> **preservare il carattere di Xfce, adottare la fluidità di GNOME, assorbire la personalizzazione di KDE, e rendere tutto gestibile da GUI.**

Per il processo di sviluppo, la scelta più solida e coerente col tuo stack è:

> **Codex in VSCodium + Codex CLI/Cloud + Spec Kit come impalcatura spec-driven + review finale in GitHub con Codex.**

