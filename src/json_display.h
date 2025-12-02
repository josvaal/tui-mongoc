#ifndef JSON_DISPLAY_H
#define JSON_DISPLAY_H

#include <mongoc/mongoc.h>

// header de curses multiplataforma
#ifdef _WIN32
#include <curses.h> // PDCurses en Windows
#else
#include <ncurses.h> // ncurses en Unix
#endif

// formatear BSON a JSON
char *json_format_bson(const bson_t *doc);

// formatear BSON a JSON editable
bool json_format_bson_editable(const bson_t *doc, char *buffer,
                               size_t buffer_size);

// mostrar documento BSON con colores
int json_display_document(WINDOW *win, const bson_t *doc, int start_y,
                          int start_x, int max_height, int max_width);

// mostrar JSON con colores
int json_display_string(WINDOW *win, const char *json, int start_y, int start_x,
                        int max_height, int max_width, int scroll_offset);

// calcular líneas totales del JSON
int json_count_lines(const char *json, int max_width);

// inicializar colores para JSON
void json_init_colors(void);

// constantes de colores para JSON
#define JSON_COLOR_KEY 1
#define JSON_COLOR_STRING 2
#define JSON_COLOR_NUMBER 3
#define JSON_COLOR_BOOLEAN 4
#define JSON_COLOR_NULL 5
#define JSON_COLOR_BRACKET 6

#endif // JSON_DISPLAY_H
