#include "init_data.h"
#include "init.h"   // per avere la dichiarazione di extern sqlite3 *db
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include "lib/cJSON/cJSON.h"  // assicurati di avere la libreria cJSON installata

extern sqlite3 *db; // utilizziamo il db globale aperto nel main

// Funzione per leggere l'intero file JSON
static char *read_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("Errore apertura JSON");
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char *data = malloc(len + 1);
    if (!data) {
        fclose(f);
        return NULL;
    }
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);
    return data;
}

void populate_db_from_json(const char *filename) {
    char *json_data = read_file(filename);
    if (!json_data) return;

    cJSON *root = cJSON_Parse(json_data);
    if (!root) {
        fprintf(stderr, "Errore parsing JSON\n");
        free(json_data);
        return;
    }

    cJSON *libri = cJSON_GetObjectItem(root, "Libro");
    cJSON *libreria = cJSON_GetObjectItem(root, "Libreria");

    if (!libri || !cJSON_IsArray(libri)) {
        fprintf(stderr, "JSON non valido: manca 'Libro'\n");
        goto cleanup;
    }
    if (!libreria || !cJSON_IsArray(libreria)) {
        fprintf(stderr, "JSON non valido: manca 'Libreria'\n");
        goto cleanup;
    }

    int n_libri = cJSON_GetArraySize(libri);

    for (int i = 0; i < n_libri; i++) {
        cJSON *libro = cJSON_GetArrayItem(libri, i);
        int codLibro = cJSON_GetObjectItem(libro, "codLibro")->valueint;
        const char *titolo = cJSON_GetObjectItem(libro, "titolo")->valuestring;
        const char *autore = cJSON_GetObjectItem(libro, "autore")->valuestring;

        // INSERT OR IGNORE Libro
        sqlite3_stmt *stmt;
        const char *sql_libro = "INSERT OR IGNORE INTO Libro (codLibro, titolo, autore) VALUES (?, ?, ?);";

        if (sqlite3_prepare_v2(db, sql_libro, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, codLibro);
            sqlite3_bind_text(stmt, 2, titolo, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, autore, -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                fprintf(stderr, "Errore insert Libro: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
        }

        // INSERT OR IGNORE Libreria
        cJSON *libro_libreria = cJSON_GetArrayItem(libreria, i);
        int copie = cJSON_GetObjectItem(libro_libreria, "copie")->valueint;

        const char *sql_libreria = "INSERT OR IGNORE INTO Libreria (codLibro, copie) VALUES (?, ?);";
        if (sqlite3_prepare_v2(db, sql_libreria, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, codLibro);
            sqlite3_bind_int(stmt, 2, copie);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                fprintf(stderr, "Errore insert Libreria: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);
        }
    }

cleanup:
    cJSON_Delete(root);
    free(json_data);

    printf("Dati inseriti da %s (ignorati se già presenti)\n", filename);
}


