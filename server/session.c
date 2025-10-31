// session_mgr.c
#include "session.h"
#include "commands.h"   // per la definizione completa di ClientSession e costanti

#include <stdlib.h>
#include <string.h>

/*
 * Registry dinamico:
 * - g_clients: array dinamico di puntatori a ClientSession (allocate su heap una per una)
 * - g_free_stack: pila (free-list) di indici liberi riutilizzabili
 * - g_cap: capacità dell'array di puntatori
 * - g_size: numero di slot mai usati (0..g_size-1 sono potenzialmente in uso o riutilizzabili)
 * - g_free_top: top index della pila di indici liberi (-1 = vuota)
 */

static ClientSession **g_clients = NULL;
static size_t g_cap = 0;
static size_t g_size = 0;

static int *g_free_stack = NULL;
static int g_free_top = -1;

/* Cresce la capacità raddoppiando (o portando a 128 se zero). */
static int grow_capacity(void) {
    size_t new_cap = (g_cap == 0) ? 128 : (g_cap * 2);

    ClientSession **new_arr = (ClientSession **)realloc(g_clients, new_cap * sizeof(ClientSession *));
    if (!new_arr) return -1;

    // Inizializza i nuovi slot a NULL
    for (size_t i = g_cap; i < new_cap; ++i) new_arr[i] = NULL;
    g_clients = new_arr;

    int *new_free = (int *)realloc(g_free_stack, new_cap * sizeof(int));
    if (!new_free) return -1;
    g_free_stack = new_free;
    // g_free_top resta valido

    g_cap = new_cap;
    return 0;
}

int sessions_init(size_t initial_capacity) {
    // reset stato
    g_clients = NULL;
    g_free_stack = NULL;
    g_cap = g_size = 0;
    g_free_top = -1;

    if (initial_capacity == 0) initial_capacity = 128;

    // arrotonda capacità a potenza di 2 (non obbligatorio, ma comodo)
    size_t cap = 1;
    while (cap < initial_capacity) cap <<= 1;

    g_clients = (ClientSession **)calloc(cap, sizeof(ClientSession *));
    if (!g_clients) return -1;

    g_free_stack = (int *)malloc(cap * sizeof(int));
    if (!g_free_stack) { free(g_clients); g_clients = NULL; return -1; }

    g_cap = cap;
    g_size = 0;
    g_free_top = -1;
    return 0;
}

void sessions_shutdown(void) {
    if (g_clients) {
        for (size_t i = 0; i < g_cap; ++i) {
            if (g_clients[i]) {
                free(g_clients[i]);
                g_clients[i] = NULL;
            }
        }
        free(g_clients);
        g_clients = NULL;
    }
    if (g_free_stack) {
        free(g_free_stack);
        g_free_stack = NULL;
    }
    g_cap = 0;
    g_size = 0;
    g_free_top = -1;
}

static int pop_free_index(void) {
    if (g_free_top < 0) return -1;
    return g_free_stack[g_free_top--];
}

static void push_free_index(int idx) {
    // Assumiamo che la pila sia già capiente quanto g_cap (cresciuta in grow_capacity)
    g_free_stack[++g_free_top] = idx;
}

int session_create(int fd) {
    // 1) Riusa slot libero se disponibile
    int idx = pop_free_index();
    if (idx >= 0) {
        if (!g_clients[idx]) {
            g_clients[idx] = (ClientSession *)malloc(sizeof(ClientSession));
            if (!g_clients[idx]) return -1;
        }
        ClientSession *cs = g_clients[idx];
        memset(cs, 0, sizeof(*cs));
        cs->active = 1;
        cs->fd = fd;
        return idx;
    }

    // 2) Usa un nuovo slot, cresce se necessario
    if (g_size >= g_cap) {
        if (grow_capacity() != 0) return -1;
    }
    idx = (int)g_size++;
    if (!g_clients[idx]) {
        g_clients[idx] = (ClientSession *)malloc(sizeof(ClientSession));
        if (!g_clients[idx]) return -1;
    }
    ClientSession *cs = g_clients[idx];
    memset(cs, 0, sizeof(*cs));
    cs->active = 1;
    cs->fd = fd;
    return idx;
}

ClientSession *session_get(int index) {
    if (index < 0) return NULL;
    size_t i = (size_t)index;
    if (i >= g_cap) return NULL;
    return g_clients[i]; // può essere NULL se mai allocato
}

void session_destroy(int index) {
    if (index < 0) return;
    size_t i = (size_t)index;
    if (i >= g_cap) return;

    ClientSession *cs = g_clients[i];
    if (!cs) return;

    // Pulisci lo stato e marca come non attivo
    memset(cs->username, 0, sizeof(cs->username));
    cs->cart_size = 0;
    memset(cs->cart, 0, sizeof(cs->cart));
    cs->logged_in = 0;
    cs->is_librarian = 0;
    cs->active = 0;
    cs->fd = -1;

    // Rimetti lo slot nella free-list (riutilizzabile)
    push_free_index((int)i);
}

size_t sessions_capacity(void) {
    return g_cap;
}

