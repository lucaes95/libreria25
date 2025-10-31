#ifndef COMMANDS_H
#define COMMANDS_H

#include <pthread.h> // Serve per pthread_mutex_t

#define MAX_CART 3
#define MAX_CLIENTS 100 // opzionale: non è più il limite reale, lasciato per compatibilità

// --- Struttura dati sessione ---
// (Usata da server/commands e definita qui per accesso ai campi)
// DOPO (con tag) ✅
typedef struct ClientSession {
  int active;
  int fd;
  char username[64];
  int cart[MAX_CART];
  int cart_size;
  int logged_in;
  int is_librarian;
} ClientSession;

// --- Dati globali "esterni" ---
// Niente più 'clients[]': le sessioni sono gestite dal session manager dinamico.
extern pthread_mutex_t clients_mutex;
extern struct sqlite3 *db; // Includi sqlite3.h in server.c e commands.c

// --- Funzione Pubblica di Smistamento ---
/**
 * Analizza il buffer ricevuto dal client ed esegue il comando corrispondente.
 * @param index Handle della sessione nel registry dinamico (non più indice di array).
 * @param buf   Buffer del comando (verrà modificato per il parsing).
 * @return 0 per continuare, 1 se il client ha richiesto il logout.
 */
int process_command(int index, char *buf);

#endif // COMMANDS_H
