#ifndef INPUT_H
#define INPUT_H

// header de curses multiplataforma
#ifdef _WIN32
#include <curses.h> // PDCurses en Windows
#else
#include <ncurses.h> // ncurses en Unix
#endif

#include <stdbool.h>

#define INPUT_MAX_LENGTH 1024

// obtener input de una línea
bool input_get_string(WINDOW *win, int y, int x, char *buffer, int max_length,
                      const char *initial);

// mostrar diálogo de input simple
bool input_text_single(const char *title, const char *prompt, char *buffer,
                       int max_length, const char *instructions);

// obtener input multilínea
bool input_get_multiline(WINDOW *win, int start_y, int start_x, int height,
                         int width, char *buffer, int max_length);

// mostrar editor de texto
bool input_text_editor(const char *title, char *buffer, int max_length,
                       const char *instructions);

// obtener selección de menú
int input_get_menu_selection(WINDOW *win, int y, int x, const char **items,
                             int count, int *selected_idx);

#endif // INPUT_H
