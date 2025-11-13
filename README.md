# Libreria – Client/Server
Progetto di discussione esame di laboratorio di sistemi operativi, Informatica, Federico II.
Il sistema vuole simulare una libreria dove i clienti possono registrarsi e loggare per cercare libri disponibili, metterli nel carrello, prenderli in prestito, e restituirli. Inoltre prevede l'accesso del libraio, utente "speciale". Il libraio può osservare sia i libri disponibili che quelli non disponibili, settare un limite ai prestiti degli utenti e in caso di scadenza può inviargli un messaggio per invitare il cliente alla restituzione.

# Vincoli di sviluppo
Realizzare un server C. 
Il requisito principale è la realizzazione di un server C, multi-thread con un socket di comunicazione server/client per il flusso di richieste e risposte tra server/client.

# Compilazione e Esecuzione:
- (Server) Avviare il server tramite il file bash nella sua root, con il comando: 
chmod +x run.sh (settare i permessi per eventuali problemi di accesso)
./run.sh
- (Client) Avviare il client tramite il file bash nella sua root, con il comando:
chmod +x run-client.sh
./run-client.sh

# Guida dipendenze: 
- (Server) Nella dir del server è presente una libreria esterna .cjson che permette l'estrazione dei dati dal file del suo formato. Utilizzata unicamente per estrarre e caricare i libri sul db realizzato con sql tramite libreria sqlite.
- (Client) Il client è realizzato con java quindi necessita di JDK. Il progetto è stato scritto con le seguenti versioni: 

openjdk version "21.0.8" 2025-07-15
OpenJDK Runtime Environment (build 21.0.8+9)
OpenJDK 64-Bit Server VM (build 21.0.8+9, mixed mode, sharing)

Inoltre il client necessita di JavaFx (javafx-sdk-21.0.8) che permette la realizzazione delle interfacce grafiche delle UI.

# Gestione Dati:
- I dati (utenti, prestiti, messaggi, ..) sono memorizzati su un database mysql inizializzato e gestito tramite sqlite.
- I libri sono memorizzati su un file json e all'avvio del server caricati sul database.
- Il limite dei prestiti viene memorizzato su un file e gestito unicamente dalla libreria standard I/0 di c.
