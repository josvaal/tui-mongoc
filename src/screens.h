#ifndef SCREENS_H
#define SCREENS_H

#include "mongo_ops.h"
#include "tui.h"
#include "input.h"
#include <stdbool.h>

// estado de la aplicación
typedef struct {
    mongo_context_t *mongo_ctx;
    screen_id_t current_screen;
    screen_id_t previous_screen;

    // pantalla de conexión
    char uri_buffer[512];

    // navegación de BD/colecciones
    char **databases;
    int db_count;
    int db_selected;

    char **collections;
    int coll_count;
    int coll_selected;

    char current_db[256];
    char current_collection[256];

    // visor de documentos
    bson_t **documents;
    int doc_count;
    int doc_page;
    int doc_per_page;
    int doc_selected;         // documento seleccionado en página actual
    int doc_scroll_offset;
    long long total_documents;

    // filtros
    char filter_json[INPUT_MAX_LENGTH];
    bson_t *current_filter;

    // mensajes
    char message[512];
    msg_type_t message_type;
    bool show_message;

    // flags
    bool should_quit;
} app_state_t;

// crear estado de app
app_state_t *app_state_new(void);

// liberar estado de app
void app_state_free(app_state_t *state);

// pantalla de conexión
screen_id_t screen_connection(app_state_t *state);

// pantalla de lista de bases de datos
screen_id_t screen_database_list(app_state_t *state);

// pantalla de lista de colecciones
screen_id_t screen_collection_list(app_state_t *state);

// pantalla de visor de documentos
screen_id_t screen_document_viewer(app_state_t *state);

// pantalla de insertar documento
screen_id_t screen_document_insert(app_state_t *state);

// pantalla de ayuda
screen_id_t screen_help(app_state_t *state);

// configurar mensaje de app
void app_set_message(app_state_t *state, const char *message, msg_type_t type);

// cargar documentos de la colección actual
bool load_documents(app_state_t *state);

#endif // SCREENS_H
