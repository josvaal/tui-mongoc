#include "input.h"
#include "tui.h"
#include "utils.h"
#include <string.h>
#include <ctype.h>

bool input_get_string(WINDOW *win, int y, int x, char *buffer, int max_length, const char *initial) {
    if (!win || !buffer || max_length <= 0) {
        return false;
    }

    // copiar valor inicial si hay
    int pos = 0;
    if (initial) {
        safe_strncpy(buffer, initial, max_length);
        pos = strlen(buffer);
    } else {
        buffer[0] = '\0';
    }

    wmove(win, y, x);
    wclrtoeol(win);

    if (pos > 0) {
        mvwprintw(win, y, x, "%s", buffer);
    }

    // mostrar cursor bien visible
    curs_set(2);
    wmove(win, y, x + pos);
    wrefresh(win);

    int ch;
    while ((ch = wgetch(win)) != ERR) {
        if (ch == '\n' || ch == KEY_ENTER) {
            // enviar
            break;
        } else if (ch == 27) {  // ESC
            // cancelar
            curs_set(0);
            return false;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            // borrar caracter
            if (pos > 0) {
                pos--;
                buffer[pos] = '\0';
                wmove(win, y, x);
                wclrtoeol(win);
                mvwprintw(win, y, x, "%s", buffer);
                wrefresh(win);
            }
        } else if (ch == KEY_DC) {
            // tecla delete - borrar todo
            pos = 0;
            buffer[0] = '\0';
            wmove(win, y, x);
            wclrtoeol(win);
            wrefresh(win);
        } else if (isprint(ch) && pos < max_length - 1) {
            // agregar caracter
            buffer[pos++] = ch;
            buffer[pos] = '\0';
            waddch(win, ch);
            wrefresh(win);
        }
    }

    curs_set(0);  // ocultar cursor
    return true;
}

// redibujar buffer y posicionar cursor
static void redraw_buffer(WINDOW *win, int start_y, int start_x, int height, int width,
                         const char *buffer, int pos) {
    // limpiar área de input
    for (int i = 0; i < height; i++) {
        mvwhline(win, start_y + i, start_x, ' ', width);
    }

    // redibujar contenido
    int l = 0, c = 0;
    int buffer_len = strlen(buffer);
    for (int i = 0; i < buffer_len && l < height; i++) {
        if (buffer[i] == '\n') {
            l++;
            c = 0;
        } else {
            mvwaddch(win, start_y + l, start_x + c, buffer[i]);
            c++;
            if (c >= width) {
                l++;
                c = 0;
            }
        }
    }

    // calcular posición del cursor
    int line = 0, col = 0;
    for (int i = 0; i < pos; i++) {
        if (buffer[i] == '\n') {
            line++;
            col = 0;
        } else {
            col++;
            if (col >= width) {
                line++;
                col = 0;
            }
        }
    }

    // posicionar cursor y refrescar
    wmove(win, start_y + line, start_x + col);
    wrefresh(win);
    doupdate();  // forzar actualización para PDCurses
}

bool input_get_multiline(WINDOW *win, int start_y, int start_x, int height, int width,
                         char *buffer, int max_length) {
    if (!win || !buffer || max_length <= 0) {
        return false;
    }

    // calcular posición inicial del buffer
    int pos = strlen(buffer);
    int line = 0;
    int col = 0;

    // calcular línea y columna inicial
    for (int i = 0; i < pos; i++) {
        if (buffer[i] == '\n') {
            line++;
            col = 0;
        } else {
            col++;
            if (col >= width) {
                line++;
                col = 0;
            }
        }
    }

    // activar cursor visible
    curs_set(2);

    // asegurar que se actualice el cursor (importante para PDCurses en Windows)
    leaveok(win, FALSE);

    // dibujo inicial
    redraw_buffer(win, start_y, start_x, height, width, buffer, pos);

    // actualización extra para que el cursor se vea en Windows
    wmove(win, start_y + line, start_x + col);
    wrefresh(win);
    doupdate();

    // macros para códigos de teclas multiplataforma
    #define IS_KEY_UP(ch)    ((ch) == KEY_UP || (ch) == 450)
    #define IS_KEY_DOWN(ch)  ((ch) == KEY_DOWN || (ch) == 456)
    #define IS_KEY_LEFT(ch)  ((ch) == KEY_LEFT || (ch) == 452)
    #define IS_KEY_RIGHT(ch) ((ch) == KEY_RIGHT || (ch) == 454)

    int ch;
    while ((ch = wgetch(win)) != ERR) {
        if (ch == 27) {  // ESC
            curs_set(0);
            return false;
        } else if (ch == KEY_F(2)) {  // F2 para guardar
            break;
        } else if (IS_KEY_LEFT(ch)) {
            // mover cursor izq
            if (pos > 0) {
                pos--;
                // recalcular línea y col
                line = 0;
                col = 0;
                for (int i = 0; i < pos; i++) {
                    if (buffer[i] == '\n') {
                        line++;
                        col = 0;
                    } else {
                        col++;
                        if (col >= width) {
                            line++;
                            col = 0;
                        }
                    }
                }
                curs_set(2);
                wmove(win, start_y + line, start_x + col);
                wrefresh(win);
                doupdate();  // forzar actualización para PDCurses
            }
        } else if (IS_KEY_RIGHT(ch)) {
            // mover cursor der
            if (pos < strlen(buffer)) {
                pos++;
                // recalcular línea y col
                line = 0;
                col = 0;
                for (int i = 0; i < pos; i++) {
                    if (buffer[i] == '\n') {
                        line++;
                        col = 0;
                    } else {
                        col++;
                        if (col >= width) {
                            line++;
                            col = 0;
                        }
                    }
                }
                curs_set(2);
                wmove(win, start_y + line, start_x + col);
                wrefresh(win);
                doupdate();  // forzar actualización para PDCurses
            }
        } else if (IS_KEY_UP(ch)) {
            // mover cursor arriba
            if (line > 0) {
                // buscar inicio de línea actual
                int line_start = pos;
                while (line_start > 0 && buffer[line_start - 1] != '\n') {
                    line_start--;
                }

                if (line_start > 0) {
                    // buscar inicio de línea anterior
                    int prev_line_start = line_start - 2;
                    while (prev_line_start > 0 && buffer[prev_line_start - 1] != '\n') {
                        prev_line_start--;
                    }

                    // ir a la misma columna en línea anterior
                    int target_col = col;
                    pos = prev_line_start;
                    int curr_col = 0;
                    while (pos < line_start - 1 && buffer[pos] != '\n' && curr_col < target_col) {
                        pos++;
                        curr_col++;
                    }

                    // recalcular línea y col
                    line = 0;
                    col = 0;
                    for (int i = 0; i < pos; i++) {
                        if (buffer[i] == '\n') {
                            line++;
                            col = 0;
                        } else {
                            col++;
                        }
                    }
                    curs_set(2);
                    wmove(win, start_y + line, start_x + col);
                    wrefresh(win);
                    doupdate();  // forzar actualización para PDCurses
                }
            }
        } else if (IS_KEY_DOWN(ch)) {
            // mover cursor abajo
            int buffer_len = strlen(buffer);
            // buscar fin de línea actual
            int line_end = pos;
            while (line_end < buffer_len && buffer[line_end] != '\n') {
                line_end++;
            }

            if (line_end < buffer_len) {
                // ir a línea siguiente
                int next_line_start = line_end + 1;
                int target_col = col;
                pos = next_line_start;
                int curr_col = 0;
                while (pos < buffer_len && buffer[pos] != '\n' && curr_col < target_col) {
                    pos++;
                    curr_col++;
                }

                // recalcular línea y col
                line = 0;
                col = 0;
                for (int i = 0; i < pos; i++) {
                    if (buffer[i] == '\n') {
                        line++;
                        col = 0;
                    } else {
                        col++;
                    }
                }
                curs_set(2);
                wmove(win, start_y + line, start_x + col);
                wrefresh(win);
                doupdate();  // forzar actualización para PDCurses
            }
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (pos > 0) {
                // borrar caracter y mover todo a la izq
                memmove(&buffer[pos - 1], &buffer[pos], strlen(buffer) - pos + 1);
                pos--;

                // recalcular línea y col
                line = 0;
                col = 0;
                for (int i = 0; i < pos; i++) {
                    if (buffer[i] == '\n') {
                        line++;
                        col = 0;
                    } else {
                        col++;
                        if (col >= width) {
                            line++;
                            col = 0;
                        }
                    }
                }

                // redibujar con cursor visible
                redraw_buffer(win, start_y, start_x, height, width, buffer, pos);
                curs_set(2);
            }
        } else if (ch == '\n' || ch == KEY_ENTER) {
            if (strlen(buffer) < max_length - 1) {
                // insertar salto de línea
                memmove(&buffer[pos + 1], &buffer[pos], strlen(buffer) - pos + 1);
                buffer[pos] = '\n';
                pos++;
                line++;
                col = 0;

                // redibujar con cursor visible
                redraw_buffer(win, start_y, start_x, height, width, buffer, pos);
                curs_set(2);
            }
        } else if (isprint(ch) && strlen(buffer) < max_length - 1) {
            // insertar caracter en posición actual
            memmove(&buffer[pos + 1], &buffer[pos], strlen(buffer) - pos + 1);
            buffer[pos] = ch;
            pos++;
            col++;

            if (col >= width) {
                line++;
                col = 0;
            }

            // redibujar con cursor visible
            redraw_buffer(win, start_y, start_x, height, width, buffer, pos);
            curs_set(2);
        }
    }

    #undef IS_KEY_UP
    #undef IS_KEY_DOWN
    #undef IS_KEY_LEFT
    #undef IS_KEY_RIGHT

    curs_set(0);
    return true;
}

bool input_text_single(const char *title, const char *prompt, char *buffer,
                       int max_length, const char *instructions) {
    int height = 8;
    int width = 60;
    int start_y, start_x;

    tui_get_size(&start_y, &start_x);
    start_y = (start_y - height) / 2;
    start_x = (start_x - width) / 2;

    WINDOW *win = newwin(height, width, start_y, start_x);
    if (!win) {
        return false;
    }

    keypad(win, TRUE);
    tui_draw_box(win, title);

    int current_y = 1;

    if (instructions) {
        mvwprintw(win, current_y++, 2, "%s", instructions);
        tui_draw_hline(win, current_y++, 1, width - 2);
    }

    if (prompt) {
        mvwprintw(win, current_y++, 2, "%s", prompt);
    }

    mvwprintw(win, height - 2, 2, "ENTER: Save | ESC: Cancel");
    wrefresh(win);

    bool result = input_get_string(win, current_y, 2, buffer, max_length, buffer);

    delwin(win);
    touchwin(stdscr);
    refresh();

    return result;
}

bool input_text_editor(const char *title, char *buffer, int max_length, const char *instructions) {
    int height = 20;
    int width = 70;
    int start_y, start_x;

    tui_get_size(&start_y, &start_x);
    start_y = (start_y - height) / 2;
    start_x = (start_x - width) / 2;

    WINDOW *win = newwin(height, width, start_y, start_x);
    if (!win) {
        return false;
    }

    keypad(win, TRUE);
    tui_draw_box(win, title);

    if (instructions) {
        mvwprintw(win, 1, 2, "%s", instructions);
        tui_draw_hline(win, 2, 1, width - 2);
    }

    int input_height = height - 5;
    int input_width = width - 4;
    int input_y = instructions ? 3 : 2;

    mvwprintw(win, height - 2, 2, "F2: Save | ESC: Cancel");
    wrefresh(win);

    bool result = input_get_multiline(win, input_y, 2, input_height, input_width, buffer, max_length);

    delwin(win);
    touchwin(stdscr);
    refresh();

    return result;
}

int input_get_menu_selection(WINDOW *win, int y, int x, const char **items, int count,
                              int *selected_idx) {
    if (!win || !items || count <= 0 || !selected_idx) {
        return -1;
    }

    int current = *selected_idx;
    if (current < 0 || current >= count) {
        current = 0;
    }

    int ch;
    while ((ch = wgetch(win)) != ERR) {
        // Clear previous selection
        for (int i = 0; i < count; i++) {
            if (i == current) {
                wattron(win, COLOR_PAIR(COLOR_PAIR_SELECTED));
                mvwprintw(win, y + i, x, " > %s", items[i]);
                wattroff(win, COLOR_PAIR(COLOR_PAIR_SELECTED));
            } else {
                mvwprintw(win, y + i, x, "   %s", items[i]);
            }
        }
        wrefresh(win);

        if (ch == KEY_UP && current > 0) {
            current--;
        } else if (ch == KEY_DOWN && current < count - 1) {
            current++;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            *selected_idx = current;
            return current;
        } else if (ch == 27) {  // ESC
            return -1;
        }
    }

    return -1;
}
