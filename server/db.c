#include "db.h"
#include "init.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>


extern pthread_mutex_t libri_mutex;


// --- Funzioni DB ---
// TODO : Probabile mutex lock anche in fase di registrazione.
int register_user(const char *username, const char *password) {
  sqlite3_stmt *stmt;
  const char *sql = "INSERT INTO Utente(nickname, password) VALUES(?1, ?2);";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    return -1;
  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return (rc == SQLITE_DONE) ? 0 : -1;
}

int login_user(const char *username, const char *password) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT password FROM Utente WHERE nickname=?1;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    return 0;
  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
  int rc = sqlite3_step(stmt);
  int ok = 0;
  if (rc == SQLITE_ROW) {
    const unsigned char *db_pass = sqlite3_column_text(stmt, 0);
    if (strcmp((const char *)db_pass, password) == 0)
      ok = 1;
  }
  sqlite3_finalize(stmt);
  return ok;
}

void select_list_libri(int fd) {
  const char *sql = "SELECT L.codLibro, L.titolo, L.autore, Li.copie "
                    "FROM Libro L JOIN Libreria Li ON L.codLibro = Li.codLibro "
                    "WHERE Li.copie > 0;";

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
    char buffer[1024];
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      int codLibro = sqlite3_column_int(stmt, 0);
      const char *titolo = (const char *)sqlite3_column_text(stmt, 1);
      const char *autore = (const char *)sqlite3_column_text(stmt, 2);
      int copie = sqlite3_column_int(stmt, 3);

      snprintf(buffer, sizeof(buffer), "%d;%s;%s;%d\n", codLibro, titolo,
               autore, copie);
      send(fd, buffer, strlen(buffer), 0);
    }

    sqlite3_finalize(stmt);
  } else {
    send(fd, "ERROR_LIST_BOOKS\n", 17, 0);
  }
}

void select_libro(int fd, int codLibro) {
  const char *sql = "SELECT Libro.codLibro, titolo, autore, copie "
                    "FROM Libro JOIN Libreria USING(codLibro) "
                    "WHERE Libro.codLibro = ?;";

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_int(stmt, 1, codLibro); // sostituisce il ?

    char buffer[1024];
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      int codLibroRes = sqlite3_column_int(stmt, 0);
      const char *titolo = (const char *)sqlite3_column_text(stmt, 1);
      const char *autore = (const char *)sqlite3_column_text(stmt, 2);
      int copie = sqlite3_column_int(stmt, 3);

      snprintf(buffer, sizeof(buffer), "%d;%s;%s;%d\n", codLibroRes, titolo,
               autore, copie);
      send(fd, buffer, strlen(buffer), 0);
    }
    sqlite3_finalize(stmt);
  } else {
    send(fd, "ERROR_LIST_BOOKS\n", 17, 0);
  }
}

// Decrementa di 1 le copie disponibili
int decrementa_copie(int codLibro) {
  pthread_mutex_lock(&libri_mutex); // 🔒 SEZIONE CRITICA

  const char *sql =
      "UPDATE libreria SET copie = copie - 1 WHERE codLibro = ? AND copie > 0;";
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    pthread_mutex_unlock(&libri_mutex);
    return -1;
  }

  sqlite3_bind_int(stmt, 1, codLibro);
  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  pthread_mutex_unlock(&libri_mutex); // 🔓 FINE SEZIONE CRITICA

  // Se nessuna riga è stata aggiornata (nessuna copia disponibile)
  if (rc == SQLITE_DONE && sqlite3_changes(db) == 0)
    return 1; // codice 1 = nessuna copia disponibile

  return (rc == SQLITE_DONE) ? 0 : -1; // 0 = OK, -1 = errore DB
}

// Incrementa di 1 le copie disponibili
int incrementa_copie(int codLibro) {
    int result = -1;
    const char *sql = "UPDATE libreria SET copie = copie + 1 WHERE codLibro = ?;";
    sqlite3_stmt *stmt;

    pthread_mutex_lock(&libri_mutex); // 🔒 Protezione inizio

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, codLibro);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE) {
            result = 0;
        }
        sqlite3_finalize(stmt);
    }

    pthread_mutex_unlock(&libri_mutex); // 🔓 Fine protezione

    return result;
}

/**
 * Restituisce il numero di copie disponibili per un libro.
 * Ritorna:
 *   - numero di copie >= 0 se ok
 *   - -1 se errore SQL
 */
int get_copie(int codLibro) {
    int copie = -1;
    const char *sql = "SELECT copie FROM libreria WHERE codLibro = ?;";
    sqlite3_stmt *stmt;

    pthread_mutex_lock(&libri_mutex); // 🔒 Protezione accesso DB

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, codLibro);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            copie = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    pthread_mutex_unlock(&libri_mutex); // 🔓 Fine protezione

    return copie;
}


// Inserisce un prestito
int insert_prestito(const char *username, int codLibro) {
  const char *sql = "INSERT INTO Prestito (codLibro, nicknameUtente, "
                    "dataInizio, dataFine) VALUES (?, ?, ?, ?);";
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK)
    return -1;

  time_t now = time(NULL);
  time_t end = now + 90 * 24 * 3600;

  struct tm tm_now, tm_end;
  localtime_r(&now, &tm_now);
  localtime_r(&end, &tm_end);

  char now_str[11]; // dd/MM/yyyy + '\0'
  char end_str[11];

  strftime(now_str, sizeof(now_str), "%d/%m/%Y", &tm_now);
  strftime(end_str, sizeof(end_str), "%d/%m/%Y", &tm_end);

  sqlite3_bind_int(stmt, 1, codLibro);
  sqlite3_bind_text(stmt, 2, username, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, now_str, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, end_str, -1, SQLITE_TRANSIENT);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return (rc == SQLITE_DONE) ? 0 : -1;
}

// Rimuove un prestito di un utente da un libro specifico
int remove_prestito(const char *username, int codLibro) {
  const char *sql =
      "DELETE FROM Prestito WHERE nicknameUtente = ? AND codLibro = ?;";
  sqlite3_stmt *stmt;
  int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK)
    return -1;

  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, codLibro);

  rc = sqlite3_step(stmt);
  int changes =
      sqlite3_changes(db); // quante righe sono state effettivamente cancellate
  sqlite3_finalize(stmt);

  return (rc == SQLITE_DONE && changes > 0) ? 0 : -1;
}

// Seleziona prestiti dell'utente loggato
void select_prestiti_utente(int fd, const char *username) {
  const char *sql = "SELECT P.codPrestito, P.codLibro, L.titolo, L.autore, "
                    "P.dataInizio, P.dataFine "
                    "FROM Prestito P "
                    "JOIN Libro L ON P.codLibro = L.codLibro "
                    "WHERE P.nicknameUtente = ?";

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    char buffer[1024];
    int found = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
      found = 1;

      int codPrestito = sqlite3_column_int(stmt, 0);
      int codLibro = sqlite3_column_int(stmt, 1);
      const char *titolo = (const char *)sqlite3_column_text(stmt, 2);
      const char *autore = (const char *)sqlite3_column_text(stmt, 3);
      const char *dataInizio = (const char *)sqlite3_column_text(stmt, 4);
      const char *dataFine = (const char *)sqlite3_column_text(stmt, 5);

      snprintf(buffer, sizeof(buffer), "%d;%d;%s;%s;%s;%s\n", codPrestito,
               codLibro, titolo, autore, dataInizio, dataFine);
      send(fd, buffer, strlen(buffer), 0);
    }
    sqlite3_finalize(stmt);
    // 🔹 aggiungi sempre un terminatore per il client
    if (!found) {
      send(fd, "END_LIST\n", 9, 0);
    }
  } else {
    send(fd, "ERROR_LIST_PRESTITI\n", 20, 0);
  }
}

int get_utente_da_prestito(int codPrestito, char *buffer, size_t bufsize) {
    const char *sql = "SELECT nicknameUtente FROM Prestito WHERE codPrestito = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
        return -1;

    sqlite3_bind_int(stmt, 1, codPrestito);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *utente = (const char *)sqlite3_column_text(stmt, 0);
        snprintf(buffer, bufsize, "%s", utente);
        sqlite3_finalize(stmt);
        return 0; // trovato
    }

    sqlite3_finalize(stmt);
    return -1; // non trovato
}

/**
 * Conta il numero di prestiti attivi per un dato utente.
 * Ritorna il numero di prestiti (es. 0, 1, 2...)
 * Ritorna -1 in caso di errore SQL.
 */
int get_numero_prestiti(const char *username) {

  /* * Query molto più semplice:
   * 1. Usa COUNT(*) per contare le righe.
   * 2. Seleziona SOLO dalla tabella Prestito (non serve la JOIN con Libro).
   * 3. Filtra per nicknameUtente.
   */
  const char *sql = "SELECT COUNT(*) FROM Prestito WHERE nicknameUtente = ?;";

  sqlite3_stmt *stmt;
  int count = 0; // Inizializza il contatore a 0

  // Prepara la query
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Errore SQL (prepare) in get_numero_prestiti: %s\n",
            sqlite3_errmsg(db));
    return -1; // Errore
  }

  // Associa l'username al '?' (placeholder)
  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);

  // Esegui la query
  int rc = sqlite3_step(stmt);

  if (rc == SQLITE_ROW) {
    // La query ha prodotto un risultato (il conteggio)
    count = sqlite3_column_int(stmt, 0); // Leggi il numero dalla prima colonna
  } else {
    // Errore durante l'esecuzione
    fprintf(stderr, "Errore SQL (step) in get_numero_prestiti: %s\n",
            sqlite3_errmsg(db));
    count = -1; // Segnala errore
  }

  // Rilascia le risorse
  sqlite3_finalize(stmt);

  return count;
}