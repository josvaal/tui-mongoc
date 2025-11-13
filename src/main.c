#include "mongo_ops.h"
#include "tui.h"
#include "screens.h"
#include "json_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

// estado global para manejar señales
static volatile bool g_running = true;

void signal_handler(int signum) {
    (void)signum;
    g_running = false;
}

void setup_signal_handlers(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    // inicializar mongo
    mongo_init();

    // configurar handlers de señales
    setup_signal_handlers();

    // inicializar TUI
    if (!tui_init()) {
        fprintf(stderr, "Failed to initialize TUI\n");
        mongo_cleanup();
        return 1;
    }

    // inicializar colores JSON
    json_init_colors();

    // crear estado de la app
    app_state_t *state = app_state_new();
    if (!state) {
        tui_cleanup();
        mongo_cleanup();
        fprintf(stderr, "Failed to create application state\n");
        return 1;
    }

    // loop principal
    screen_id_t next_screen = SCREEN_CONNECTION;

    while (g_running && !state->should_quit) {
        // guardar pantalla anterior para la ayuda
        state->previous_screen = state->current_screen;
        state->current_screen = next_screen;

        switch (next_screen) {
            case SCREEN_CONNECTION:
                next_screen = screen_connection(state);
                break;

            case SCREEN_DATABASE_LIST:
                next_screen = screen_database_list(state);
                break;

            case SCREEN_COLLECTION_LIST:
                next_screen = screen_collection_list(state);
                break;

            case SCREEN_DOCUMENT_VIEWER:
                next_screen = screen_document_viewer(state);
                break;

            case SCREEN_DOCUMENT_INSERT:
                next_screen = screen_document_insert(state);
                break;

            case SCREEN_HELP:
                next_screen = screen_help(state);
                break;

            case SCREEN_QUIT:
            default:
                g_running = false;
                break;
        }

        // ver si hubo interrupción
        if (!g_running) {
            break;
        }
    }

    // limpiar todo
    app_state_free(state);
    tui_cleanup();
    mongo_cleanup();

    printf("MongoDB TUI Client terminated.\n");

    return 0;
}
