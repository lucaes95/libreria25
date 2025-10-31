#include "settings.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SETTINGS_DIR  "settings"
#define SETTINGS_FILE "settings/num_prestito"
#define DEFAULT_MAX   3

static pthread_mutex_t g_settings_mutex = PTHREAD_MUTEX_INITIALIZER;

static int ensure_dir_exists(const char *path) {
  struct stat st;
  if (stat(path, &st) == 0) {
    if (S_ISDIR(st.st_mode)) return 0;
    return -1; // esiste ma non è directory
  }
  // prova a creare la dir
#ifdef _WIN32
  int rc = mkdir(path);
#else
  int rc = mkdir(path, 0755);
#endif
  return (rc == 0) ? 0 : -1;
}

int settings_get_max_prestiti(void) {
  pthread_mutex_lock(&g_settings_mutex);

  // assicura che la directory esista
  ensure_dir_exists(SETTINGS_DIR);

  FILE *f = fopen(SETTINGS_FILE, "r");
  if (!f) {
    // crea con default
    f = fopen(SETTINGS_FILE, "w");
    if (!f) {
      pthread_mutex_unlock(&g_settings_mutex);
      return DEFAULT_MAX; // fallback silenzioso
    }
    fprintf(f, "%d\n", DEFAULT_MAX);
    fclose(f);
    pthread_mutex_unlock(&g_settings_mutex);
    return DEFAULT_MAX;
  }

  int value = DEFAULT_MAX;
  if (fscanf(f, "%d", &value) != 1 || value <= 0) {
    value = DEFAULT_MAX;
  }
  fclose(f);

  pthread_mutex_unlock(&g_settings_mutex);
  return value;
}

int settings_set_max_prestiti(int value) {
  if (value <= 0) return -1;

  pthread_mutex_lock(&g_settings_mutex);

  if (ensure_dir_exists(SETTINGS_DIR) != 0) {
    pthread_mutex_unlock(&g_settings_mutex);
    return -1;
  }

  FILE *f = fopen(SETTINGS_FILE, "w");
  if (!f) {
    pthread_mutex_unlock(&g_settings_mutex);
    return -1;
  }
  fprintf(f, "%d\n", value);
  fclose(f);

  pthread_mutex_unlock(&g_settings_mutex);
  return 0;
}
