#ifndef SETTINGS_H
#define SETTINGS_H

// Ritorna il valore corrente del limite prestiti.
// Se il file non esiste, lo crea con default 3.
// Ritorna sempre >=1 in condizioni normali, oppure 3 come fallback.
int settings_get_max_prestiti(void);

// Imposta il valore nel file (solo intero positivo).
// Ritorna 0 su successo, -1 su errore.
int settings_set_max_prestiti(int value);

#endif // SETTINGS_H
