#include "commands.h"
#include "init.h"
#include "init_data.h"
#include "settings.h"
#include "session.h"

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
#define MAX_CLIENTS 100 // solo backlog/listen; il limite reale ora è dinamico
#define MAX_CART 3      // Limite libri nel carrello (per carrello sessione)

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t libri_mutex = PTHREAD_MUTEX_INITIALIZER;

sqlite3 *db = NULL;

// --- Gestione sessioni client (dinamico) ---
static int add_client(int fd) {
  pthread_mutex_lock(&clients_mutex);
  int idx = session_create(fd);   // -1 se OOM
  pthread_mutex_unlock(&clients_mutex);
  return idx;
}

static void remove_client(int index) {
  pthread_mutex_lock(&clients_mutex);
  session_destroy(index);
  pthread_mutex_unlock(&clients_mutex);
}

void *client_thread(void *arg) {
  int index = *(int *)arg;
  free(arg);

  int fd = -1;
  pthread_mutex_lock(&clients_mutex);
  ClientSession *cs = session_get(index);
  if (cs) {
    fd = cs->fd;
    cs->cart_size = 0;
    cs->logged_in = 0;
    cs->is_librarian = 0;
    memset(cs->cart, 0, sizeof(cs->cart));
    memset(cs->username, 0, sizeof(cs->username));
  }
  pthread_mutex_unlock(&clients_mutex);

  if (fd < 0) return NULL; // sessione invalida (difensivo)

  char buf[MAXLINE];

  while (1) {
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0)
      break; // Client disconnesso

    buf[n] = '\0';

    int should_exit = process_command(index, buf);
    if (should_exit) {
      break;
    }

    memset(buf, 0, sizeof(buf)); // pulizia buffer
  }

  close(fd);
  remove_client(index);
  return NULL;
}

// --- Main ---
int main() {
  // Precarica impostazioni (facoltativo, forza la creazione del file/settings)
  settings_get_max_prestiti();

  // Init session manager dinamico
  if (sessions_init(128) != 0) {
    fprintf(stderr, "Errore init session manager\n");
    return 1;
  }

  // Connessione SQLite thread-safe
  if (sqlite3_open_v2("libreria.db", &db,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                          SQLITE_OPEN_SERIALIZED,
                      NULL) != SQLITE_OK) {
    fprintf(stderr, "Errore apertura DB (thread-safe): %s\n",
            sqlite3_errmsg(db));
    sessions_shutdown();
    return 1;
  }

  // Inizializza le tabelle usando l'handle appena aperto
  init_db();
  populate_db_from_json("data/data.json");

  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    perror("socket");
    sqlite3_close(db);
    sessions_shutdown();
    return 1;
  }

  struct sockaddr_in servaddr = {0};
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(PORT);

  int opt = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    perror("bind");
    close(listenfd);
    sqlite3_close(db);
    sessions_shutdown();
    return 1;
  }
  if (listen(listenfd, MAX_CLIENTS) < 0) {
    perror("listen");
    close(listenfd);
    sqlite3_close(db);
    sessions_shutdown();
    return 1;
  }

  printf("Server avviato su porta %d\n", PORT);

  while (1) {
    struct sockaddr_in cli;
    socklen_t clilen = sizeof(cli);
    int fd = accept(listenfd, (struct sockaddr *)&cli, &clilen);
    if (fd < 0) {
      perror("accept");
      continue;
    }

    int *index = malloc(sizeof(int));
    if (!index) {
      send(fd, "SERVER_FULL\n", 12, 0);
      close(fd);
      continue;
    }
    *index = add_client(fd);
    if (*index == -1) {
      send(fd, "SERVER_FULL\n", 12, 0);
      close(fd);
      free(index);
      continue;
    }

    pthread_t tid;
    if (pthread_create(&tid, NULL, client_thread, index) != 0) {
      // Se fallisce il thread, libera la sessione
      close(fd);
      remove_client(*index);
      free(index);
      continue;
    }
    pthread_detach(tid);
  }

  // (In pratica non si arriva qui)
  close(listenfd);
  sqlite3_close(db);
  sessions_shutdown();
  return 0;
}
