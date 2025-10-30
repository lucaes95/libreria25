
#ifndef INIT_H
#define INIT_H

#include <sqlite3.h>

extern sqlite3 *db; // dichiarazione globale
void init_db(void);

#endif