#include "mongo_ops.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void mongo_init(void) {
    mongoc_init();
}

void mongo_cleanup(void) {
    mongoc_cleanup();
}

mongo_context_t *mongo_context_new(void) {
    mongo_context_t *ctx = calloc(1, sizeof(mongo_context_t));
    if (!ctx) {
        return NULL;
    }

    ctx->client = NULL;
    ctx->uri = NULL;
    ctx->current_db = NULL;
    ctx->current_collection = NULL;
    ctx->connected = false;
    ctx->error_message[0] = '\0';

    return ctx;
}

void mongo_context_free(mongo_context_t *ctx) {
    if (!ctx) {
        return;
    }

    mongo_disconnect(ctx);

    if (ctx->current_db) {
        free(ctx->current_db);
    }
    if (ctx->current_collection) {
        free(ctx->current_collection);
    }

    free(ctx);
}

bool mongo_connect(mongo_context_t *ctx, const char *uri_string) {
    if (!ctx || !uri_string) {
        return false;
    }

    // desconectar si ya está conectado
    if (ctx->connected) {
        mongo_disconnect(ctx);
    }

    bson_error_t error;

    // parsear URI
    ctx->uri = mongoc_uri_new_with_error(uri_string, &error);
    if (!ctx->uri) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Invalid URI: %s", error.message);
        return false;
    }

    // configurar timeouts ANTES de crear el cliente (5 segundos c/u)
    mongoc_uri_set_option_as_int32(ctx->uri, MONGOC_URI_SERVERSELECTIONTIMEOUTMS, 5000);
    mongoc_uri_set_option_as_int32(ctx->uri, MONGOC_URI_CONNECTTIMEOUTMS, 5000);
    mongoc_uri_set_option_as_int32(ctx->uri, MONGOC_URI_SOCKETTIMEOUTMS, 5000);

    // crear cliente
    ctx->client = mongoc_client_new_from_uri(ctx->uri);
    if (!ctx->client) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to create client");
        mongoc_uri_destroy(ctx->uri);
        ctx->uri = NULL;
        return false;
    }

    // configurar nombre de app
    mongoc_client_set_appname(ctx->client, "MongoDB-TUI");

    // configurar versión de API de errores
    mongoc_client_set_error_api(ctx->client, MONGOC_ERROR_API_VERSION_2);

    // probar conexión con ping
    if (!mongo_ping(ctx)) {
        mongo_disconnect(ctx);
        return false;
    }

    ctx->connected = true;
    snprintf(ctx->error_message, sizeof(ctx->error_message), "Connected successfully");

    return true;
}

void mongo_disconnect(mongo_context_t *ctx) {
    if (!ctx) {
        return;
    }

    if (ctx->client) {
        mongoc_client_destroy(ctx->client);
        ctx->client = NULL;
    }

    if (ctx->uri) {
        mongoc_uri_destroy(ctx->uri);
        ctx->uri = NULL;
    }

    ctx->connected = false;
}

bool mongo_ping(mongo_context_t *ctx) {
    if (!ctx || !ctx->client) {
        return false;
    }

    bson_t *command = BCON_NEW("ping", BCON_INT32(1));
    bson_t reply;
    bson_error_t error;

    bool success = mongoc_client_command_simple(
        ctx->client, "admin", command, NULL, &reply, &error
    );

    if (!success) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Ping failed: %s", error.message);
    }

    bson_destroy(command);
    bson_destroy(&reply);

    return success;
}

char **mongo_list_databases(mongo_context_t *ctx, int *count) {
    if (!ctx || !ctx->client || !count) {
        if (ctx) {
            snprintf(ctx->error_message, sizeof(ctx->error_message),
                     "Invalid context or not connected");
        }
        return NULL;
    }

    *count = 0;
    bson_error_t error;

    // obtener nombre de BD del URI si está
    const char *uri_db = NULL;
    if (ctx->uri) {
        uri_db = mongoc_uri_get_database(ctx->uri);
    }

    // traer nombres de bases de datos
    char **db_names = mongoc_client_get_database_names_with_opts(ctx->client, NULL, &error);
    if (!db_names) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to list databases: %s", error.message);
        return NULL;
    }

    // contar bases de datos
    int db_count = 0;
    while (db_names[db_count]) {
        db_count++;
    }

    // ver si la BD del URI ya está en la lista
    bool uri_db_in_list = false;
    if (uri_db && uri_db[0] != '\0') {
        for (int i = 0; i < db_count; i++) {
            if (strcmp(db_names[i], uri_db) == 0) {
                uri_db_in_list = true;
                break;
            }
        }
    }

    // calcular conteo final (agregar 1 si la BD del URI no está)
    int final_count = db_count;
    if (uri_db && uri_db[0] != '\0' && !uri_db_in_list) {
        final_count++;
    }

    // crear nuestro propio array
    char **result = malloc(final_count * sizeof(char *));
    if (!result) {
        bson_strfreev(db_names);
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Memory allocation failed");
        return NULL;
    }

    int result_idx = 0;

    // agregar BD del URI primero si no está en lista
    if (uri_db && uri_db[0] != '\0' && !uri_db_in_list) {
        result[result_idx] = str_dup(uri_db);
        if (!result[result_idx]) {
            free(result);
            bson_strfreev(db_names);
            snprintf(ctx->error_message, sizeof(ctx->error_message),
                     "Memory allocation failed");
            return NULL;
        }
        result_idx++;
    }

    // copiar el resto de nombres de BD
    for (int i = 0; i < db_count; i++) {
        result[result_idx] = str_dup(db_names[i]);
        if (!result[result_idx]) {
            // liberar lo que ya creamos
            for (int j = 0; j < result_idx; j++) {
                free(result[j]);
            }
            free(result);
            bson_strfreev(db_names);
            snprintf(ctx->error_message, sizeof(ctx->error_message),
                     "Memory allocation failed");
            return NULL;
        }
        result_idx++;
    }

    bson_strfreev(db_names);
    *count = final_count;

    return result;
}

char **mongo_list_collections(mongo_context_t *ctx, const char *db_name, int *count) {
    if (!ctx || !ctx->client || !db_name || !count) {
        if (ctx) {
            snprintf(ctx->error_message, sizeof(ctx->error_message),
                     "Invalid parameters");
        }
        return NULL;
    }

    *count = 0;
    bson_error_t error;

    mongoc_database_t *database = mongoc_client_get_database(ctx->client, db_name);
    if (!database) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to access database: %s", db_name);
        return NULL;
    }

    // traer nombres de colecciones
    char **coll_names = mongoc_database_get_collection_names_with_opts(database, NULL, &error);
    if (!coll_names) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to list collections: %s", error.message);
        mongoc_database_destroy(database);
        return NULL;
    }

    // contar colecciones
    int coll_count = 0;
    while (coll_names[coll_count]) {
        coll_count++;
    }

    // crear nuestro propio array
    char **result = malloc(coll_count * sizeof(char *));
    if (!result) {
        bson_strfreev(coll_names);
        mongoc_database_destroy(database);
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Memory allocation failed");
        return NULL;
    }

    // copiar nombres de colecciones
    for (int i = 0; i < coll_count; i++) {
        result[i] = str_dup(coll_names[i]);
        if (!result[i]) {
            for (int j = 0; j < i; j++) {
                free(result[j]);
            }
            free(result);
            bson_strfreev(coll_names);
            mongoc_database_destroy(database);
            snprintf(ctx->error_message, sizeof(ctx->error_message),
                     "Memory allocation failed");
            return NULL;
        }
    }

    bson_strfreev(coll_names);
    mongoc_database_destroy(database);
    *count = coll_count;

    return result;
}

bool mongo_create_collection(mongo_context_t *ctx, const char *db_name,
                             const char *collection_name) {
    if (!ctx || !ctx->client || !db_name || !collection_name) {
        if (ctx) {
            snprintf(ctx->error_message, sizeof(ctx->error_message),
                     "Invalid parameters");
        }
        return false;
    }

    mongoc_database_t *database = mongoc_client_get_database(ctx->client, db_name);
    if (!database) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to access database: %s", db_name);
        return false;
    }

    bson_error_t error;
    mongoc_collection_t *collection = mongoc_database_create_collection(
        database, collection_name, NULL, &error
    );

    bool success = (collection != NULL);

    if (!success) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to create collection: %s", error.message);
    } else {
        ctx->error_message[0] = '\0';
        mongoc_collection_destroy(collection);
    }

    mongoc_database_destroy(database);
    return success;
}

bool mongo_drop_collection(mongo_context_t *ctx, const char *db_name,
                           const char *collection_name) {
    if (!ctx || !ctx->client || !db_name || !collection_name) {
        if (ctx) {
            snprintf(ctx->error_message, sizeof(ctx->error_message),
                     "Invalid parameters");
        }
        return false;
    }

    mongoc_collection_t *collection = mongoc_client_get_collection(
        ctx->client, db_name, collection_name
    );

    if (!collection) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to access collection: %s.%s", db_name, collection_name);
        return false;
    }

    bson_error_t error;
    bool success = mongoc_collection_drop(collection, &error);

    if (!success) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to drop collection: %s", error.message);
    } else {
        ctx->error_message[0] = '\0';
    }

    mongoc_collection_destroy(collection);
    return success;
}

long long mongo_count_documents(mongo_context_t *ctx, const char *db_name,
                                const char *collection_name, const bson_t *filter) {
    if (!ctx || !ctx->client || !db_name || !collection_name) {
        if (ctx) {
            snprintf(ctx->error_message, sizeof(ctx->error_message),
                     "Invalid parameters");
        }
        return -1;
    }

    mongoc_collection_t *collection = mongoc_client_get_collection(
        ctx->client, db_name, collection_name
    );

    if (!collection) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to access collection: %s.%s", db_name, collection_name);
        return -1;
    }

    bson_error_t error;
    const bson_t *query = filter ? filter : bson_new();

    int64_t count = mongoc_collection_count_documents(
        collection, query, NULL, NULL, NULL, &error
    );

    if (!filter) {
        bson_destroy((bson_t *)query);
    }

    mongoc_collection_destroy(collection);

    if (count < 0) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Count failed: %s", error.message);
        return -1;
    }

    return count;
}

bson_t **mongo_find_documents(mongo_context_t *ctx, const char *db_name,
                              const char *collection_name, const bson_t *filter,
                              int skip, int limit, int *count) {
    if (!ctx || !ctx->client || !db_name || !collection_name || !count) {
        if (ctx) {
            snprintf(ctx->error_message, sizeof(ctx->error_message),
                     "Invalid parameters");
        }
        return NULL;
    }

    *count = 0;

    mongoc_collection_t *collection = mongoc_client_get_collection(
        ctx->client, db_name, collection_name
    );

    if (!collection) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to access collection: %s.%s", db_name, collection_name);
        return NULL;
    }

    // armar opciones de query
    bson_t opts;
    bson_init(&opts);
    if (skip > 0) {
        BSON_APPEND_INT32(&opts, "skip", skip);
    }
    if (limit > 0) {
        BSON_APPEND_INT32(&opts, "limit", limit);
    }

    const bson_t *query = filter ? filter : bson_new();

    // ejecutar find
    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
        collection, query, &opts, NULL
    );

    if (!cursor) {
        if (!filter) {
            bson_destroy((bson_t *)query);
        }
        bson_destroy(&opts);
        mongoc_collection_destroy(collection);
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to create cursor");
        return NULL;
    }

    // contar resultados primero
    const bson_t *doc;
    int doc_count = 0;
    while (mongoc_cursor_next(cursor, &doc)) {
        doc_count++;
    }

    // ver si hay errores del cursor
    bson_error_t error;
    if (mongoc_cursor_error(cursor, &error)) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Cursor error: %s", error.message);
        mongoc_cursor_destroy(cursor);
        if (!filter) {
            bson_destroy((bson_t *)query);
        }
        bson_destroy(&opts);
        mongoc_collection_destroy(collection);
        return NULL;
    }

    if (doc_count == 0) {
        mongoc_cursor_destroy(cursor);
        if (!filter) {
            bson_destroy((bson_t *)query);
        }
        bson_destroy(&opts);
        mongoc_collection_destroy(collection);
        return NULL;
    }

    // crear array para documentos
    bson_t **documents = malloc(doc_count * sizeof(bson_t *));
    if (!documents) {
        mongoc_cursor_destroy(cursor);
        if (!filter) {
            bson_destroy((bson_t *)query);
        }
        bson_destroy(&opts);
        mongoc_collection_destroy(collection);
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Memory allocation failed");
        return NULL;
    }

    // re-ejecutar query porque el cursor ya se consumió
    mongoc_cursor_destroy(cursor);
    cursor = mongoc_collection_find_with_opts(collection, query, &opts, NULL);

    // copiar documentos
    int idx = 0;
    while (mongoc_cursor_next(cursor, &doc) && idx < doc_count) {
        documents[idx] = bson_copy(doc);
        if (!documents[idx]) {
            // liberar lo que ya creamos
            for (int i = 0; i < idx; i++) {
                bson_destroy(documents[i]);
            }
            free(documents);
            mongoc_cursor_destroy(cursor);
            if (!filter) {
                bson_destroy((bson_t *)query);
            }
            bson_destroy(&opts);
            mongoc_collection_destroy(collection);
            snprintf(ctx->error_message, sizeof(ctx->error_message),
                     "Failed to copy document");
            return NULL;
        }
        idx++;
    }

    mongoc_cursor_destroy(cursor);
    if (!filter) {
        bson_destroy((bson_t *)query);
    }
    bson_destroy(&opts);
    mongoc_collection_destroy(collection);

    *count = doc_count;
    return documents;
}

bool mongo_insert_document(mongo_context_t *ctx, const char *db_name,
                           const char *collection_name, const bson_t *document) {
    if (!ctx || !ctx->client || !db_name || !collection_name || !document) {
        if (ctx) {
            snprintf(ctx->error_message, sizeof(ctx->error_message),
                     "Invalid parameters");
        }
        return false;
    }

    mongoc_collection_t *collection = mongoc_client_get_collection(
        ctx->client, db_name, collection_name
    );

    if (!collection) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to access collection: %s.%s", db_name, collection_name);
        return false;
    }

    bson_error_t error;
    bool success = mongoc_collection_insert_one(
        collection, document, NULL, NULL, &error
    );

    if (!success) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Insert failed: %s", error.message);
    }

    mongoc_collection_destroy(collection);
    return success;
}

long long mongo_update_documents(mongo_context_t *ctx, const char *db_name,
                                 const char *collection_name, const bson_t *filter,
                                 const bson_t *update, bool update_many) {
    if (!ctx || !ctx->client || !db_name || !collection_name || !filter || !update) {
        if (ctx) {
            snprintf(ctx->error_message, sizeof(ctx->error_message),
                     "Invalid parameters");
        }
        return -1;
    }

    mongoc_collection_t *collection = mongoc_client_get_collection(
        ctx->client, db_name, collection_name
    );

    if (!collection) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to access collection: %s.%s", db_name, collection_name);
        return -1;
    }

    bson_error_t error;
    bson_t reply;
    bool success;

    if (update_many) {
        success = mongoc_collection_update_many(
            collection, filter, update, NULL, &reply, &error
        );
    } else {
        success = mongoc_collection_update_one(
            collection, filter, update, NULL, &reply, &error
        );
    }

    long long modified_count = 0;

    if (success) {
        bson_iter_t iter;
        if (bson_iter_init_find(&iter, &reply, "modifiedCount")) {
            modified_count = bson_iter_int64(&iter);
        }
    } else {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Update failed: %s", error.message);
        modified_count = -1;
    }

    bson_destroy(&reply);
    mongoc_collection_destroy(collection);

    return modified_count;
}

long long mongo_delete_documents(mongo_context_t *ctx, const char *db_name,
                                 const char *collection_name, const bson_t *filter,
                                 bool delete_many) {
    if (!ctx || !ctx->client || !db_name || !collection_name || !filter) {
        if (ctx) {
            snprintf(ctx->error_message, sizeof(ctx->error_message),
                     "Invalid parameters");
        }
        return -1;
    }

    mongoc_collection_t *collection = mongoc_client_get_collection(
        ctx->client, db_name, collection_name
    );

    if (!collection) {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Failed to access collection: %s.%s", db_name, collection_name);
        return -1;
    }

    bson_error_t error;
    bson_t reply;
    bool success;

    if (delete_many) {
        success = mongoc_collection_delete_many(
            collection, filter, NULL, &reply, &error
        );
    } else {
        success = mongoc_collection_delete_one(
            collection, filter, NULL, &reply, &error
        );
    }

    long long deleted_count = 0;

    if (success) {
        bson_iter_t iter;
        if (bson_iter_init_find(&iter, &reply, "deletedCount")) {
            // manejar tanto int32 como int64
            bson_type_t type = bson_iter_type(&iter);
            if (type == BSON_TYPE_INT32) {
                deleted_count = bson_iter_int32(&iter);
            } else if (type == BSON_TYPE_INT64) {
                deleted_count = bson_iter_int64(&iter);
            }
        }

        // limpiar mensaje de error si anduvo
        if (deleted_count == 0) {
            snprintf(ctx->error_message, sizeof(ctx->error_message),
                     "No documents matched the filter");
        } else {
            ctx->error_message[0] = '\0';
        }
    } else {
        snprintf(ctx->error_message, sizeof(ctx->error_message),
                 "Delete failed: %s", error.message);
        deleted_count = -1;
    }

    bson_destroy(&reply);
    mongoc_collection_destroy(collection);

    return deleted_count;
}

void mongo_free_documents(bson_t **documents, int count) {
    if (!documents) {
        return;
    }

    for (int i = 0; i < count; i++) {
        if (documents[i]) {
            bson_destroy(documents[i]);
        }
    }

    free(documents);
}

bson_t *mongo_json_to_bson(const char *json_string, bson_error_t *error) {
    if (!json_string) {
        return NULL;
    }

    bson_error_t local_error;
    bson_error_t *err = error ? error : &local_error;

    bson_t *bson = bson_new_from_json((const uint8_t *)json_string, -1, err);

    return bson;
}

const char *mongo_get_error(mongo_context_t *ctx) {
    if (!ctx) {
        return "Invalid context";
    }

    return ctx->error_message;
}
