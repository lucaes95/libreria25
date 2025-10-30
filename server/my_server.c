#include "commands.h"
#include "init.h"
#include "init_data.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// FIX: Devi includere sqlite3.h per usare le sue funzioni e flag
#ifndef SQLITE_OPEN_SERIALIZED
#define SQLITE_OPEN_SERIALIZED 0x00040000
#endif

#include <sqlite3.h>

#define PORT 12345 // Porta socket
#define MAXLINE 1024
#define MAX_CLIENTS 100 // Limite clienti online nello stesso momento
#define MAX_CART 3      // Limite libri nel carrello
#define MAX_PRESTITI 3  // LIMITE PRESTITI PER UTENTE

ClientSession clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t libri_mutex = PTHREAD_MUTEX_INITIALIZER;


sqlite3 *db = NULL;

// --- Gestione sessioni client ---
int add_client(int fd) {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (!clients[i].active) {
      clients[i].active = 1;
      clients[i].fd = fd;
      clients[i].username[0] = '\0';
      pthread_mutex_unlock(&clients_mutex);
      return i;
    }
  }
  pthread_mutex_unlock(&clients_mutex);
  return -1; // troppi client
}

void remove_client(int index) {
  pthread_mutex_lock(&clients_mutex);
  clients[index].active = 0;
  clients[index].fd = -1;
  clients[index].username[0] = '\0';
  pthread_mutex_unlock(&clients_mutex);
}

void *client_thread(void *arg) {
  int index = *(int *)arg;
  free(arg);
  int fd = clients[index].fd;
  char buf[MAXLINE];

  // Inizializzo la sessione
  pthread_mutex_lock(&clients_mutex);
  clients[index].cart_size = 0;
  clients[index].logged_in = 0; // <-- Usa il nuovo flag
  memset(clients[index].cart, 0, sizeof(clients[index].cart));
  memset(clients[index].username, 0, sizeof(clients[index].username));
  pthread_mutex_unlock(&clients_mutex);

  while (1) {
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0)
      break; // Client disconnesso

    buf[n] = '\0';

    // --- 3. SEZIONE COMANDI COMPLETAMENTE SOSTITUITA ---

    // Tutta la tua vecchia logica if/else/strcmp sparisce
    // e viene sostituita da una singola chiamata:
    int should_exit = process_command(index, buf);

    if (should_exit) {
      // La funzione ha gestito un LOGOUT, usciamo
      break;
    }
    // --- FINE SOSTITUZIONE ---

    memset(buf, 0, sizeof(buf)); // pulizia buffer
  }

  close(fd);
  remove_client(index);
  return NULL;
}

// --- Main ---
int main() {

  // FIX: (CRITICO)
  // Non puoi usare sqlite3_open() in un programma multi-thread
  // che condivide la connessione 'db'. Questo corromperà il DB.
  // DEVI usare sqlite3_open_v2 e il flag SQLITE_OPEN_SERIALIZED.
  // Questo flag FA PARTE della libreria sqlite3, non è una
  // libreria "extra". È il modo corretto in C per usare
  // sqlite3 con pthread.
  if (sqlite3_open_v2("libreria.db", &db,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                          SQLITE_OPEN_SERIALIZED,
                      NULL) != SQLITE_OK) {
    fprintf(stderr, "Errore apertura DB (thread-safe): %s\n",
            sqlite3_errmsg(db));
    return 1;
  }

  /* --- La tua riga originale (ERRATA) è stata rimossa ---
  if (sqlite3_open("libreria.db", &db) != SQLITE_OK) {
    fprintf(stderr, "Errore apertura DB: %s\n", sqlite3_errmsg(db));
    return 1;
  }
  */

  // Inizializza le tabelle usando l'handle appena aperto
  init_db();
  populate_db_from_json("data/data.json");

  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in servaddr = {0};
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(PORT);

  int opt = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
  listen(listenfd, MAX_CLIENTS);

  printf("Server avviato su porta %d\n", PORT);

  while (1) {
    struct sockaddr_in cli;
    socklen_t clilen = sizeof(cli);
    int fd = accept(listenfd, (struct sockaddr *)&cli, &clilen);

    int *index = malloc(sizeof(int));
    *index = add_client(fd);
    if (*index == -1) {
      send(fd, "SERVER_FULL\n", 12, 0);
      close(fd);
      free(index);
      continue;
    }

    pthread_t tid;
    pthread_create(&tid, NULL, client_thread, index);
    pthread_detach(tid);
  }

  sqlite3_close(db);
  return 0;
}
