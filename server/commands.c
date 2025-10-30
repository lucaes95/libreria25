#include "commands.h" // La nostra nuova intestazione

// Include necessari per le funzioni di questo file
#include <sqlite3.h> // Includi questo
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// Includi i tuoi header per le funzioni DB
#include "db.h"
// #include "init.h" (se serve)

#define MAXLINE 1024
#define MAX_PRESTITI 3 // LIMITE PRESTITI PER UTENTE

// --- Definizione Tipi Interni ---
// (Questi sono "privati" per commands.c)

typedef void (*CommandHandlerFunc)(int index, const char *args);

typedef struct {
  const char *cmd_name;
  CommandHandlerFunc handler;
  int requires_login;
} Command;

// --- 1. Implementazione degli Handler (static) ---
// (static = visibili solo dentro questo file)

/**
 * Gestisce la registrazione di un nuovo utente.
 * Comando: REGISTER <user> <pass>
 */
static void handle_register(int index, const char *args) {
  int fd = clients[index].fd;
  char user[64], pass[64];
  memset(user, 0, sizeof(user));
  memset(pass, 0, sizeof(pass));

  if (sscanf(args, "%63s %63s", user, pass) < 2) {
    send(fd, "REGISTER_FAIL\n", 14, 0); // Sintassi errata
    return;
  }

  if (register_user(user, pass) == 0)
    send(fd, "REGISTER_OK\n", 12, 0);
  else
    send(fd, "REGISTER_FAIL\n", 14, 0);
}

/**
 * Gestisce il login di un utente.
 * Comando: LOGIN <user> <pass>
 */
static void handle_login(int index, const char *args) {
  int fd = clients[index].fd;
  char user[64], pass[64];
  memset(user, 0, sizeof(user));
  memset(pass, 0, sizeof(pass));

  if (sscanf(args, "%63s %63s", user, pass) < 2) {
    send(fd, "LOGIN_FAIL\n", 11, 0); // Sintassi errata
    return;
  }

  // Controllo speciale per il libraio
  if (strcmp(user, "libraio") == 0 && strcmp(pass, "libraio") == 0) {
    pthread_mutex_lock(&clients_mutex);
    clients[index].logged_in = 1;
    clients[index].is_librarian = 1;
    strncpy(clients[index].username, user, sizeof(clients[index].username) - 1);
    pthread_mutex_unlock(&clients_mutex);

    send(fd, "LOGIN_LIBRARIAN\n", 17, 0);
    return;
  }

  if (login_user(user, pass)) {
    // Utente valido: aggiorna lo stato della sessione
    pthread_mutex_lock(&clients_mutex);
    clients[index].logged_in = 1;
    strncpy(clients[index].username, user, sizeof(clients[index].username) - 1);
    pthread_mutex_unlock(&clients_mutex);

    send(fd, "LOGIN_OK\n", 9, 0);
  } else {
    send(fd, "LOGIN_FAIL\n", 11, 0);
  }
}

/**
 * Gestisce il logout di un utente.
 * Comando: LOGOUT
 */
static void handle_logout(int index, const char *args) {
  int fd = clients[index].fd;

  // Pulisce lo stato della sessione (carrello, login)
  pthread_mutex_lock(&clients_mutex);
  clients[index].logged_in = 0;
  clients[index].cart_size = 0;
  memset(clients[index].cart, 0, sizeof(clients[index].cart));
  memset(clients[index].username, 0,
         sizeof(clients[index].username)); // Pulisce anche username
  pthread_mutex_unlock(&clients_mutex);

  send(fd, "BYE\n", 4, 0);
  // La disconnessione (break) avverrà in client_thread
  // grazie al valore di ritorno di process_command
}

/**
 * Invia al client la lista di tutti i libri.
 * Comando: LIST_BOOKS
 */
static void handle_list_books(int index, const char *args) {
  int fd = clients[index].fd;
  select_list_libri(fd);
  send(fd, "END_LIST\n", 9, 0);
}

/**
 * Aggiunge un libro al carrello della sessione.
 * Comando: ADD_CART <codLibro>
 */
static void handle_add_cart(int index, const char *args) {
  int fd = clients[index].fd;
  int codLibro;

  if (sscanf(args, "%d", &codLibro) == 1) {
    pthread_mutex_lock(&clients_mutex);

    int duplicate = 0;
    for (int i = 0; i < clients[index].cart_size; i++) {
      if (clients[index].cart[i] == codLibro) {
        duplicate = 1;
        break;
      }
    }

    if (duplicate) {
      send(fd, "CART_ALREADY\n", strlen("CART_ALREADY\n"), 0);
    } else if (clients[index].cart_size < MAX_CART) {
      clients[index].cart[clients[index].cart_size++] = codLibro;
      send(fd, "CART_ADDED\n", strlen("CART_ADDED\n"), 0);
    } else {
      send(fd, "CART_FULL\n", strlen("CART_FULL\n"), 0);
    }

    pthread_mutex_unlock(&clients_mutex);
  } else {
    send(fd, "CART_FAIL\n", strlen("CART_FAIL\n"), 0);
  }
}

/**
 * Invia al client la lista dei libri nel suo carrello.
 * Comando: LIST_CART
 */
static void handle_list_cart(int index, const char *args) {
  int fd = clients[index].fd;

  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < clients[index].cart_size; i++) {
    char linebuf[64];
    snprintf(linebuf, sizeof(linebuf), "%d\n", clients[index].cart[i]);
    send(fd, linebuf, strlen(linebuf), 0);
  }
  pthread_mutex_unlock(&clients_mutex);

  send(fd, "END_LIST\n", strlen("END_LIST\n"), 0);
}

/**
 * Rimuove un libro dal carrello della sessione.
 * Comando: REMOVE_CART <codLibro>
 */
static void handle_remove_cart(int index, const char *args) {
  int fd = clients[index].fd;
  int codLibro;

  if (sscanf(args, "%d", &codLibro) == 1) {
    int found = 0;
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < clients[index].cart_size; i++) {
      if (clients[index].cart[i] == codLibro) {
        // Trovato: usa la tecnica "swap-with-last"
        int last_idx = clients[index].cart_size - 1;
        if (i != last_idx) {
          clients[index].cart[i] = clients[index].cart[last_idx];
        }
        clients[index].cart[last_idx] = 0; // Pulisce lo slot
        clients[index].cart_size--;
        found = 1;
        break;
      }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (found)
      send(fd, "REMOVE_OK\n", 10, 0);
    else
      send(fd, "REMOVE_NOT_FOUND\n", 18, 0);
  } else {
    send(fd, "REMOVE_FAIL\n", 12, 0); // Argomento non valido
  }
}

/**
 * Esegue il checkout: trasforma il carrello in prestiti.
 * Comando: CHECKOUT
 */
static void handle_checkout(int index, const char *args) {
  int fd = clients[index].fd;
  int local_count = 0;
  int temp_cart[MAX_CART];
  char local_user[64];

  // 1. Copia dati sessione e svuota carrello
  pthread_mutex_lock(&clients_mutex);
  local_count = clients[index].cart_size;
  if (local_count == 0) {
    pthread_mutex_unlock(&clients_mutex);
    send(fd, "CHECKOUT_EMPTY\n", 15, 0);
    return;
  }
  strncpy(local_user, clients[index].username, sizeof(local_user) - 1);
  local_user[sizeof(local_user) - 1] = '\0';
  memcpy(temp_cart, clients[index].cart, sizeof(temp_cart));
  clients[index].cart_size = 0;
  memset(clients[index].cart, 0, sizeof(clients[index].cart));
  pthread_mutex_unlock(&clients_mutex);

  // 2. Controllo limite prestiti
  int prestiti_attuali = get_numero_prestiti(local_user);
  if (prestiti_attuali == -1) {
    send(fd, "CHECKOUT_FAIL_DB_ERROR\n", 23, 0);
    return;
  } else if (prestiti_attuali + local_count > MAX_PRESTITI) {
    send(fd, "CHECKOUT_FAIL_LIMIT\n", 20, 0);
    return;
  }

  // 3. Esegui transazione
  int success = 1;
  char *err_msg = 0;
  if (sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, &err_msg) != SQLITE_OK) {
    fprintf(stderr, "Failed to begin transaction: %s\n", err_msg);
    sqlite3_free(err_msg);
    send(fd, "CHECKOUT_FAIL\n", 14, 0);
    return;
  }

  for (int i = 0; i < local_count; i++) {
    int codLibro = temp_cart[i];
    int copie = get_copie(codLibro); // Legge le copie disponibili
    if (copie <= 0 || decrementa_copie(codLibro) != 0 ||
        insert_prestito(local_user, codLibro) != 0) {
      success = 0;
      break;
    }
  }

  if (success) {
    sqlite3_exec(db, "COMMIT;", 0, 0, 0);
    send(fd, "CHECKOUT_OK\n", 12, 0);
  } else {
    sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
    send(fd, "CHECKOUT_FAIL_NOT_AVAILABLE\n", 28, 0);
  }
}

/**
 * Invia al client la lista dei SUOI prestiti.
 * Comando: LIST_PRESTITI
 */
static void handle_list_prestiti(int index, const char *args) {
  int fd = clients[index].fd;
  char local_user[64];

  // Prende l'username dalla sessione (più sicuro)
  pthread_mutex_lock(&clients_mutex);
  strncpy(local_user, clients[index].username, sizeof(local_user) - 1);
  local_user[sizeof(local_user) - 1] = '\0';
  pthread_mutex_unlock(&clients_mutex);

  // Invia i prestiti per quell'utente
  select_prestiti_utente(fd, local_user);
  send(fd, "END_LIST\n", 9, 0);
}

static void handle_get_utente_prestito(int index, const char *args) {
  int fd = clients[index].fd;
  int codPrestito;

  if (sscanf(args, "%d", &codPrestito) != 1) {
    send(fd, "ERROR_INVALID_ARGS\n", 19, 0);
    return;
  }

  char utente[64];
  if (get_utente_da_prestito(codPrestito, utente, sizeof(utente)) == 0) {
    char msg[128];
    snprintf(msg, sizeof(msg), "OK;%s\n", utente);
    send(fd, msg, strlen(msg), 0);
  } else {
    send(fd, "ERROR_UTENTE_NOT_FOUND\n", 24, 0);
  }
}

/**
 * Restituisce al LIBRAIO la lista completa dei libri e, se in prestito,
 * i dettagli del prestito (utente, data inizio, data fine).
 * Comando: LIST_ALL_PRESTITI
 */
static void handle_list_all_prestiti(int index, const char *args) {
  int fd = clients[index].fd;

  pthread_mutex_lock(&clients_mutex);
  int is_librarian = clients[index].is_librarian;
  pthread_mutex_unlock(&clients_mutex);

  if (!is_librarian) {
    send(fd, "PERMISSION_DENIED\n", 18, 0);
    return;
  }

  const char *sql = "SELECT P.codPrestito, L.codLibro, L.titolo, L.autore, "
                    "LI.copie AS copie_disponibili, "
                    "(SELECT COUNT(*) FROM Prestito P WHERE P.codLibro = "
                    "L.codLibro) AS copie_in_prestito, "
                    "P.nicknameUtente, P.dataInizio, P.dataFine "
                    "FROM Libro L "
                    "JOIN Libreria LI ON L.codLibro = LI.codLibro "
                    "LEFT JOIN Prestito P ON L.codLibro = P.codLibro "
                    "ORDER BY L.codLibro;";

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
    char buffer[1024];
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      int codPrestito = sqlite3_column_int(stmt, 0);
      int codLibro = sqlite3_column_int(stmt, 1);
      const char *titolo = (const char *)sqlite3_column_text(stmt, 2);
      const char *autore = (const char *)sqlite3_column_text(stmt, 3);
      int copie_disponibili = sqlite3_column_int(stmt, 4);
      int copie_in_prestito = sqlite3_column_int(stmt, 5);
      const char *utente = (const char *)sqlite3_column_text(stmt, 6);
      const char *dataInizio = (const char *)sqlite3_column_text(stmt, 7);
      const char *dataFine = (const char *)sqlite3_column_text(stmt, 8);

      snprintf(buffer, sizeof(buffer), "%d;%d;%s;%s;%d;%d;%s;%s;%s;\n",
               codPrestito, codLibro, titolo ? titolo : "",
               autore ? autore : "", copie_disponibili, copie_in_prestito,
               utente ? utente : "—", dataInizio ? dataInizio : "—",
               dataFine ? dataFine : "—");

      send(fd, buffer, strlen(buffer), 0);
    }
    sqlite3_finalize(stmt);
    send(fd, "END_LIST\n", 9, 0);
  } else {
    send(fd, "ERROR_LIST_ALL_PRESTITI\n", 24, 0);
  }
}

static void handle_list_msg(int index, const char *args) {
  int fd = clients[index].fd;
  char username[64];
  if (sscanf(args, "%63s", username) != 1) {
    send(fd, "ERROR_ARGS\n", 11, 0);
    return;
  }

  const char *sql = "SELECT codMsg, codPrestito, testo, data FROM Msg WHERE "
                    "nicknameUtente=? ORDER BY codMsg DESC;";
  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    char buffer[512];
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      int codMsg = sqlite3_column_int(stmt, 0);
      int codPrestito = sqlite3_column_int(stmt, 1);
      const char *testo = (const char *)sqlite3_column_text(stmt, 2);
      const char *data = (const char *)sqlite3_column_text(stmt, 3);
      snprintf(buffer, sizeof(buffer), "%d;%d;%s;%s;\n", codMsg, codPrestito,
               testo ? testo : "", data ? data : "");
      send(fd, buffer, strlen(buffer), 0);
    }
    sqlite3_finalize(stmt);
    send(fd, "END_LIST\n", 9, 0);
  } else {
    send(fd, "LIST_MSG_FAIL\n", 14, 0);
  }
}

static void handle_delete_msg(int index, const char *args) {
  int fd = clients[index].fd;
  int codMsg;
  if (sscanf(args, "%d", &codMsg) != 1) {
    send(fd, "DELETE_MSG_FAIL\n", 16, 0);
    return;
  }

  sqlite3_stmt *stmt;
  const char *sql = "DELETE FROM Msg WHERE codMsg=?;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, codMsg);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE)
      send(fd, "DELETE_MSG_OK\n", 14, 0);
    else
      send(fd, "DELETE_MSG_FAIL\n", 16, 0);
  } else {
    send(fd, "DELETE_MSG_FAIL\n", 16, 0);
  }
}

/**
 * Gestisce la restituzione di un libro.
 * Comando: RETURN <codLibro>
 */
static void handle_return(int index, const char *args) {
  int fd = clients[index].fd;
  int codLibro;

  if (sscanf(args, "%d", &codLibro) != 1) {
    send(fd, "RETURN_FAIL\n", 13, 0); // Argomento non valido
    return;
  }

  // 1. Prendi l'username dalla sessione
  char local_user[64];
  pthread_mutex_lock(&clients_mutex);
  strncpy(local_user, clients[index].username, sizeof(local_user) - 1);
  local_user[sizeof(local_user) - 1] = '\0';
  pthread_mutex_unlock(&clients_mutex);

  // 2. Esegui la transazione
  int success = 1;
  char *err_msg = 0;

  if (sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, &err_msg) != SQLITE_OK) {
    fprintf(stderr, "Failed to begin return transaction: %s\n", err_msg);
    sqlite3_free(err_msg);
    success = 0;
  }

  // 3. Prova a rimuovere il prestito
  if (success) {
    if (remove_prestito(local_user, codLibro) != 0) {
      success = 0; // Fallito (es. prestito non esistente)
    }
  }

  // 4. Prova a incrementare le copie
  if (success) {
    if (incrementa_copie(codLibro) != 0) {
      success = 0; // Fallito (es. libro non trovato)
    }
  }

  // 5. Finalizza
  if (success) {
    sqlite3_exec(db, "COMMIT;", 0, 0, 0);
    send(fd, "RETURN_OK\n", 11, 0);
  } else {
    sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
    send(fd, "RETURN_FAIL\n", 13, 0);
  }
}

/**
 * Permette al LIBRAIO di inviare un messaggio a un utente.
 * Comando: SEND_MSG <nicknameUtente> <codPrestito> <testo>
 */
static void handle_send_msg(int index, const char *args) {

  printf("📩 handle_send_msg args = '%s'\n", args);

  int fd = clients[index].fd;

  // Verifica che sia il libraio
  pthread_mutex_lock(&clients_mutex);
  int is_librarian = clients[index].is_librarian;
  pthread_mutex_unlock(&clients_mutex);

  if (!is_librarian) {
    send(fd, "PERMISSION_DENIED\n", 18, 0);
    return;
  }

  char user[64];
  int codPrestito;
  char testo[300];

  // Estrai parametri
  if (sscanf(args, "%63s %d %[^\n]", user, &codPrestito, testo) < 3) {
    send(fd, "SEND_MSG_FAIL_SYNTAX\n", 22, 0);
    return;
  }

  // Prepara statement SQLite
  sqlite3_stmt *stmt;
  const char *sql =
      "INSERT INTO Msg (codPrestito, nicknameUtente, testo, data) "
      "VALUES (?1, ?2, ?3, datetime('now'));";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Errore prepare SEND_MSG: %s\n", sqlite3_errmsg(db));
    send(fd, "SEND_MSG_FAIL_DB\n", 17, 0);
    return;
  }

  sqlite3_bind_int(stmt, 1, codPrestito);
  sqlite3_bind_text(stmt, 2, user, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, testo, -1, SQLITE_STATIC);

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  printf("👉 user='%s', codPrestito=%d, testo='%s'\n", user, codPrestito,
         testo);

  if (rc == SQLITE_DONE)
    send(fd, "SEND_MSG_OK\n", 12, 0);
  else
    send(fd, "SEND_MSG_FAIL\n", 14, 0);
}

// --- 2. La Dispatch Table (static const) ---

static const Command command_table[] = {
    // Comandi Pubblici (requires_login = 0)
    {"REGISTER", handle_register, 0},
    {"LOGIN", handle_login, 0},
    {"LIST_BOOKS", handle_list_books, 0},
    {"ADD_CART", handle_add_cart, 0},
    {"LIST_CART", handle_list_cart, 0},
    {"REMOVE_CART", handle_remove_cart, 0},

    // Comandi Privati (requires_login = 1)
    {"LOGOUT", handle_logout, 1},
    {"CHECKOUT", handle_checkout, 1},
    {"LIST_PRESTITI", handle_list_prestiti, 1},
    {"RETURN", handle_return, 1},
    {"SEND_MSG", handle_send_msg, 1},
    {"GET_UTENTE_PRESTITO", handle_get_utente_prestito, 1},
    {"LIST_ALL_PRESTITI", handle_list_all_prestiti, 1},
    {"LIST_MSG", handle_list_msg, 1},
    {"DELETE_MSG", handle_delete_msg, 1},

    // Terminatore della tabella
    {NULL, NULL, 0}};

// --- 3. Funzione Pubblica di Smistamento ---

/**
 * Analizza il buffer ricevuto dal client ed esegue il comando
 * corrispondente.
 */
int process_command(int index, char *buf) {
  int fd = clients[index].fd;

  // Rimuovi newline (il buffer originale viene modificato)
  buf[strcspn(buf, "\r\n")] = 0;

  // Copia per strtok (perché strtok modifica la stringa)
  char line[MAXLINE];
  strncpy(line, buf, MAXLINE);
  line[MAXLINE - 1] = '\0'; // Assicura terminatore

  char *cmd = strtok(line, " ");
  char *args = strtok(NULL, ""); // Il resto della linea

  if (cmd == NULL)
    return 0; // Riga vuota, continua
  if (args == NULL)
    args = ""; // Comando senza argomenti

  // Cerca nella tabella
  for (int i = 0; command_table[i].cmd_name != NULL; i++) {
    if (strcmp(cmd, command_table[i].cmd_name) == 0) {

      // Controlla permessi (se serve login)
      if (command_table[i].requires_login && !clients[index].logged_in) {
        send(fd, "NOT_LOGGED\n", 11, 0);
      } else {
        // Esegui handler
        command_table[i].handler(index, args);
      }

      // Se era LOGOUT, ritorna 1 per dire al thread di uscire
      return (strcmp(cmd, "LOGOUT") == 0);
    }
  }

  // Se siamo qui, comando non trovato
  send(fd, "UNKNOWN_CMD\n", 12, 0);
  return 0; // Continua
}