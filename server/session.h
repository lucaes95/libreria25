#ifndef SESSION_H
#define SESSION_H

#include <stddef.h>

// Forward declaration: layout definito in commands.h
typedef struct ClientSession ClientSession;

/**
 * Inizializza il registry con capacità iniziale (es. 128).
 * Restituisce 0 su successo, -1 su errore.
 */
int sessions_init(size_t initial_capacity);

/** Libera tutte le risorse del registry. */
void sessions_shutdown(void);

/**
 * Crea una nuova sessione per il fd dato.
 * Ritorna un index >=0 su successo, -1 su errore.
 * NOTA: non fa lock interno; aspettati che il chiamante gestisca la sincronizzazione
 * se necessario (puoi usare il tuo clients_mutex).
 */
int session_create(int fd);

/**
 * Ritorna il puntatore alla sessione (o NULL se index invalido/non attivo).
 * NON fa lock.
 */
ClientSession* session_get(int index);

/**
 * Distrugge la sessione a index (se attiva) e libera la memoria.
 * NON fa lock.
 */
void session_destroy(int index);

/** Ritorna la capacità corrente (debug/metriche). */
size_t sessions_capacity(void);

#endif
