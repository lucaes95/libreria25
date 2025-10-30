
#ifndef DB_H
#define DB_H

#include <sqlite3.h>
#include <stddef.h>

int register_user(const char *username, const char *password);
int login_user(const char *username, const char *password);

void select_libro(int fd, int codLibro);
void select_list_libri(int fd);
void select_prestiti_utente(int fd, const char *username);
int get_utente_da_prestito(int codPrestito, char *buffer, size_t bufsize);

int get_copie(int codLibro);
int get_numero_prestiti(const char *username);

int decrementa_copie(int codLibro);
int incrementa_copie(int codLibro);

int insert_prestito(const char *username, int codLibro);
int remove_prestito(const char *username, int codLibro);

#endif