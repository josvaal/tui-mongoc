#ifndef TUI_H
#define TUI_H

// header de curses multiplataforma
#ifdef _WIN32
#include <curses.h> // PDCurses en Windows
#else
#include <ncurses.h> // ncurses en Unix
#endif

#include <stdbool.h>

// IDs de pantallas
typedef enum {
  SCREEN_CONNECTION,
  SCREEN_DATABASE_LIST,
  SCREEN_COLLECTION_LIST,
  SCREEN_DOCUMENT_VIEWER,
  SCREEN_DOCUMENT_INSERT,
  SCREEN_DOCUMENT_EDIT,
  SCREEN_DOCUMENT_DELETE,
  SCREEN_FILTER,
  SCREEN_HELP,
  SCREEN_QUIT
} screen_id_t;

// tipos de mensajes
typedef enum { MSG_INFO, MSG_SUCCESS, MSG_WARNING, MSG_ERROR } msg_type_t;

// inicializar TUI
bool tui_init(void);

// limpiar y restaurar terminal
void tui_cleanup(void);

// dibujar caja con título
void tui_draw_box(WINDOW *win, const char *title);

// dibujar línea horizontal
void tui_draw_hline(WINDOW *win, int y, int x, int width);

// dibujar barra de estado
void tui_draw_status(WINDOW *win, const char *message);

// mostrar mensaje con color
void tui_show_message(WINDOW *win, int y, const char *message, msg_type_t type);

// mostrar diálogo de confirmación
bool tui_confirm(const char *title, const char *message);

// limpiar línea
void tui_clear_line(WINDOW *win, int y);

// obtener tamaño de terminal
void tui_get_size(int *height, int *width);

// dibujar texto centrado
void tui_draw_centered(WINDOW *win, int y, const char *text);

// constantes de colores
#define COLOR_PAIR_NORMAL 1
#define COLOR_PAIR_HEADER 2
#define COLOR_PAIR_SELECTED 3
#define COLOR_PAIR_STATUS 4
#define COLOR_PAIR_ERROR 5
#define COLOR_PAIR_SUCCESS 6
#define COLOR_PAIR_WARNING 7
#define COLOR_PAIR_INFO 8

#endif // TUI_H
