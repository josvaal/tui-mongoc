#ifndef MONGO_OPS_H
#define MONGO_OPS_H

#include <mongoc/mongoc.h>
#include <stdbool.h>

// estructura de contexto de mongo
typedef struct {
  mongoc_client_t *client;
  mongoc_uri_t *uri;
  char *current_db;
  char *current_collection;
  bool connected;
  char error_message[512];
} mongo_context_t;

// inicializar librería de mongo
void mongo_init(void);

// limpiar librería de mongo
void mongo_cleanup(void);

// crear contexto de mongo
mongo_context_t *mongo_context_new(void);

// liberar contexto de mongo
void mongo_context_free(mongo_context_t *ctx);

// conectar a mongo con URI
bool mongo_connect(mongo_context_t *ctx, const char *uri_string);

// desconectar de mongo
void mongo_disconnect(mongo_context_t *ctx);

// probar si la conexión anda
bool mongo_ping(mongo_context_t *ctx);

// listar bases de datos
char **mongo_list_databases(mongo_context_t *ctx, int *count);

// listar colecciones
char **mongo_list_collections(mongo_context_t *ctx, const char *db_name,
                              int *count);

// crear colección
bool mongo_create_collection(mongo_context_t *ctx, const char *db_name,
                             const char *collection_name);

// eliminar colección
bool mongo_drop_collection(mongo_context_t *ctx, const char *db_name,
                           const char *collection_name);

// contar documentos
long long mongo_count_documents(mongo_context_t *ctx, const char *db_name,
                                const char *collection_name,
                                const bson_t *filter);

// buscar documentos
bson_t **mongo_find_documents(mongo_context_t *ctx, const char *db_name,
                              const char *collection_name, const bson_t *filter,
                              int skip, int limit, int *count);

// insertar documento
bool mongo_insert_document(mongo_context_t *ctx, const char *db_name,
                           const char *collection_name, const bson_t *document);

// actualizar documentos
long long mongo_update_documents(mongo_context_t *ctx, const char *db_name,
                                 const char *collection_name,
                                 const bson_t *filter, const bson_t *update,
                                 bool update_many);

// eliminar documentos
long long mongo_delete_documents(mongo_context_t *ctx, const char *db_name,
                                 const char *collection_name,
                                 const bson_t *filter, bool delete_many);

// liberar array de documentos
void mongo_free_documents(bson_t **documents, int count);

// parsear JSON a BSON
bson_t *mongo_json_to_bson(const char *json_string, bson_error_t *error);

// obtener último error
const char *mongo_get_error(mongo_context_t *ctx);

#endif // MONGO_OPS_H
