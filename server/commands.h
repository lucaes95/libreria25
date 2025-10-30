#ifndef COMMANDS_H
#define COMMANDS_H

#include <pthread.h> // Serve per pthread_mutex_t

#define MAX_CART 3
#define MAX_CLIENTS 100

// --- Struttura dati sessione ---
// (Spostata qui da server.c)
typedef struct {
  int active;
  int fd;
  char username[64];
  int cart[MAX_CART];
  int cart_size;
  int logged_in;
  int is_librarian;
} ClientSession;

// --- Dati globali "esterni" ---
// Diciamo a questo file che 'clients' e 'db' esistono 
// e che verranno definiti in un altro file (server.c)
extern ClientSession clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;
extern struct sqlite3 *db; // Includi sqlite3.h in server.c e commands.c


// --- Funzione Pubblica di Smistamento ---
/**
 * Analizza il buffer ricevuto dal client ed esegue il comando
 * corrispondente.
 * * @param index L'indice del client nell'array 'clients'.
 * @param buf Il buffer contenente il comando (verrà modificato!).
 * @return 0 per continuare, 1 se il client ha richiesto il logout.
 */
int process_command(int index, char *buf);

#endif // COMMANDS_H