#include "init.h"
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>

void init_db(void) {
    char *err_msg = NULL;

    const char *sql =
        "CREATE TABLE IF NOT EXISTS Utente ("
        "  nickname TEXT PRIMARY KEY,"
        "  password TEXT NOT NULL"
        ");"

        "CREATE TABLE IF NOT EXISTS Libro ("
        "  codLibro INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  titolo TEXT NOT NULL,"
        "  autore TEXT NOT NULL"
        ");"

        "CREATE TABLE IF NOT EXISTS Libreria ("
        "  codLibro INTEGER NOT NULL,"
        "  copie INTEGER NOT NULL CHECK(copie >= 0),"
        "  PRIMARY KEY (codLibro),"
        "  FOREIGN KEY (codLibro) REFERENCES Libro(codLibro) ON DELETE CASCADE"
        ");"

        "CREATE TABLE IF NOT EXISTS Prestito ("
        "  codPrestito INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  codLibro INTEGER NOT NULL,"
        "  nicknameUtente TEXT NOT NULL,"
        "  dataInizio TEXT NOT NULL,"
        "  dataFine TEXT NOT NULL,"
        "  UNIQUE(codLibro, nicknameUtente),"
        "  FOREIGN KEY (codLibro) REFERENCES Libro(codLibro) ON DELETE CASCADE,"
        "  FOREIGN KEY (nicknameUtente) REFERENCES Utente(nickname) ON DELETE CASCADE"
        ");"

        "CREATE TABLE IF NOT EXISTS Msg ("
        "  codMsg INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  codPrestito INTEGER NOT NULL,"
        "  nicknameUtente TEXT NOT NULL,"
        "  testo TEXT NOT NULL CHECK(length(testo) <= 300),"
        "  data TEXT NOT NULL,"
        "  FOREIGN KEY (codPrestito) REFERENCES Prestito(codPrestito) ON DELETE CASCADE,"
        "  FOREIGN KEY (nicknameUtente) REFERENCES Utente(nickname) ON DELETE CASCADE"
        ");";

    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Errore creazione tabelle: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        exit(1);
    }

    // ✅ Inserisci il libraio se non esiste già
    const char *check_sql = "SELECT COUNT(*) FROM Utente WHERE nickname='libraio';";
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, check_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Errore prepare check libraio: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }

    rc = sqlite3_step(stmt);
    int exists = 0;
    if (rc == SQLITE_ROW)
        exists = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (!exists) {
        const char *insert_sql = "INSERT INTO Utente (nickname, password) VALUES ('libraio', 'libraio');";
        rc = sqlite3_exec(db, insert_sql, 0, 0, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Errore inserimento libraio: %s\n", err_msg);
            sqlite3_free(err_msg);
        } else {
            printf("Utente 'libraio' aggiunto al database.\n");
        }
    } else {
        printf("Utente 'libraio' già presente nel database.\n");
    }
}

