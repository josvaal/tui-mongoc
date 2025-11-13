#include "json_display.h"
#include <string.h>
#include <ctype.h>

void json_init_colors(void) {
    if (!has_colors()) {
        return;
    }

    init_pair(JSON_COLOR_KEY, COLOR_CYAN, COLOR_BLACK);
    init_pair(JSON_COLOR_STRING, COLOR_GREEN, COLOR_BLACK);
    init_pair(JSON_COLOR_NUMBER, COLOR_YELLOW, COLOR_BLACK);
    init_pair(JSON_COLOR_BOOLEAN, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(JSON_COLOR_NULL, COLOR_RED, COLOR_BLACK);
    init_pair(JSON_COLOR_BRACKET, COLOR_WHITE, COLOR_BLACK);
}

char *json_format_bson(const bson_t *doc) {
    if (!doc) {
        return NULL;
    }

    size_t length;
    // usar JSON extendido relajado para mejor legibilidad
    char *json = bson_as_relaxed_extended_json(doc, &length);

    return json;
}

bool json_format_bson_editable(const bson_t *doc, char *buffer, size_t buffer_size) {
    if (!doc || !buffer || buffer_size == 0) {
        return false;
    }

    size_t length;
    char *json = bson_as_relaxed_extended_json(doc, &length);
    if (!json) {
        return false;
    }

    // pretty-print simple: agregar saltos de línea
    size_t out_pos = 0;
    bool in_string = false;
    bool escape_next = false;

    buffer[0] = '\0';

    for (size_t i = 0; i < length && out_pos < buffer_size - 10; i++) {
        char c = json[i];

        // rastrear si estamos en string
        if (c == '\\' && in_string) {
            escape_next = true;
            buffer[out_pos++] = c;
            continue;
        }

        if (c == '"' && !escape_next) {
            in_string = !in_string;
        }

        escape_next = false;

        // agregar caracter
        buffer[out_pos++] = c;

        // agregar saltos de línea (solo fuera de strings)
        if (!in_string) {
            if (c == '{' || c == '[') {
                buffer[out_pos++] = '\n';
                buffer[out_pos++] = ' ';
                buffer[out_pos++] = ' ';
            } else if (c == ',') {
                buffer[out_pos++] = '\n';
                buffer[out_pos++] = ' ';
                buffer[out_pos++] = ' ';
            } else if (c == '}' || c == ']') {
                // insertar salto antes del cierre
                if (out_pos > 1 && buffer[out_pos - 2] != '\n') {
                    buffer[out_pos - 1] = '\n';
                    buffer[out_pos++] = c;
                }
            }
        }
    }

    buffer[out_pos] = '\0';
    bson_free(json);

    return true;
}

int json_count_lines(const char *json, int max_width) {
    if (!json) {
        return 0;
    }

    int lines = 0;
    int col = 0;

    for (const char *p = json; *p; p++) {
        if (*p == '\n') {
            lines++;
            col = 0;
        } else {
            col++;
            if (col >= max_width) {
                lines++;
                col = 0;
            }
        }
    }

    if (col > 0) {
        lines++;
    }

    return lines;
}

static void print_colored_char(WINDOW *win, char ch, int *color, bool *in_string, bool *in_key) {
    // máquina de estados para colores
    if (ch == '"') {
        if (*in_string) {
            wattron(win, COLOR_PAIR(*in_key ? JSON_COLOR_KEY : JSON_COLOR_STRING));
            waddch(win, ch);
            wattroff(win, COLOR_PAIR(*in_key ? JSON_COLOR_KEY : JSON_COLOR_STRING));
            *in_string = false;
            *in_key = false;
        } else {
            *in_string = true;
            // ver si es una key (seguida por :)
            wattron(win, COLOR_PAIR(JSON_COLOR_KEY));
            waddch(win, ch);
            wattroff(win, COLOR_PAIR(JSON_COLOR_KEY));
        }
    } else if (*in_string) {
        wattron(win, COLOR_PAIR(*in_key ? JSON_COLOR_KEY : JSON_COLOR_STRING));
        waddch(win, ch);
        wattroff(win, COLOR_PAIR(*in_key ? JSON_COLOR_KEY : JSON_COLOR_STRING));
    } else if (ch == ':') {
        *in_key = false;
        wattron(win, COLOR_PAIR(JSON_COLOR_BRACKET));
        waddch(win, ch);
        wattroff(win, COLOR_PAIR(JSON_COLOR_BRACKET));
    } else if (strchr("{[}],", ch)) {
        wattron(win, COLOR_PAIR(JSON_COLOR_BRACKET));
        waddch(win, ch);
        wattroff(win, COLOR_PAIR(JSON_COLOR_BRACKET));
    } else if (isdigit(ch) || ch == '-' || ch == '.') {
        wattron(win, COLOR_PAIR(JSON_COLOR_NUMBER));
        waddch(win, ch);
        wattroff(win, COLOR_PAIR(JSON_COLOR_NUMBER));
    } else if (ch == 't' || ch == 'f') {
        // puede ser true/false
        wattron(win, COLOR_PAIR(JSON_COLOR_BOOLEAN));
        waddch(win, ch);
        wattroff(win, COLOR_PAIR(JSON_COLOR_BOOLEAN));
    } else if (ch == 'n') {
        // puede ser null
        wattron(win, COLOR_PAIR(JSON_COLOR_NULL));
        waddch(win, ch);
        wattroff(win, COLOR_PAIR(JSON_COLOR_NULL));
    } else {
        waddch(win, ch);
    }
}

int json_display_string(WINDOW *win, const char *json, int start_y, int start_x,
                        int max_height, int max_width, int scroll_offset) {
    if (!win || !json) {
        return 0;
    }

    int y = start_y;
    int col = 0;
    const char *p = json;
    bool in_string = false;
    bool in_escape = false;
    bool is_key = false;

    while (*p && (y - start_y) < max_height) {
        if (*p == '\n') {
            y++;
            col = 0;
            p++;
            continue;
        }

        if (col >= max_width) {
            y++;
            col = 0;
        }

        wmove(win, y, start_x + col);

        // determinar contexto y aplicar colores
        if (in_escape) {
            wattron(win, COLOR_PAIR(is_key ? JSON_COLOR_KEY : JSON_COLOR_STRING));
            waddch(win, *p);
            wattroff(win, COLOR_PAIR(is_key ? JSON_COLOR_KEY : JSON_COLOR_STRING));
            in_escape = false;
        }
        else if (*p == '\\' && in_string) {
            wattron(win, COLOR_PAIR(is_key ? JSON_COLOR_KEY : JSON_COLOR_STRING));
            waddch(win, *p);
            wattroff(win, COLOR_PAIR(is_key ? JSON_COLOR_KEY : JSON_COLOR_STRING));
            in_escape = true;
        }
        else if (*p == '"') {
            if (in_string) {
                // comilla de cierre
                wattron(win, COLOR_PAIR(is_key ? JSON_COLOR_KEY : JSON_COLOR_STRING));
                waddch(win, *p);
                wattroff(win, COLOR_PAIR(is_key ? JSON_COLOR_KEY : JSON_COLOR_STRING));
                in_string = false;
                // ver si hay dos puntos adelante
                const char *next = p + 1;
                while (*next && isspace(*next)) next++;
                if (*next != ':') {
                    is_key = false;
                }
            } else {
                // comilla de apertura - ver si es key
                const char *next = p + 1;
                const char *temp = next;
                bool found_quote = false;
                // buscar comilla de cierre
                while (*temp && *temp != '\n') {
                    if (*temp == '"' && *(temp-1) != '\\') {
                        found_quote = true;
                        temp++;
                        break;
                    }
                    temp++;
                }
                // ver si sigue con dos puntos
                while (*temp && isspace(*temp)) temp++;
                is_key = (*temp == ':');

                wattron(win, COLOR_PAIR(is_key ? JSON_COLOR_KEY : JSON_COLOR_STRING));
                waddch(win, *p);
                wattroff(win, COLOR_PAIR(is_key ? JSON_COLOR_KEY : JSON_COLOR_STRING));
                in_string = true;
            }
        }
        else if (in_string) {
            wattron(win, COLOR_PAIR(is_key ? JSON_COLOR_KEY : JSON_COLOR_STRING));
            waddch(win, *p);
            wattroff(win, COLOR_PAIR(is_key ? JSON_COLOR_KEY : JSON_COLOR_STRING));
        }
        else if (*p == '{' || *p == '}' || *p == '[' || *p == ']' || *p == ',' || *p == ':') {
            wattron(win, COLOR_PAIR(JSON_COLOR_BRACKET));
            waddch(win, *p);
            wattroff(win, COLOR_PAIR(JSON_COLOR_BRACKET));
        }
        else if (isdigit(*p) || *p == '-' || *p == '+' || *p == '.') {
            wattron(win, COLOR_PAIR(JSON_COLOR_NUMBER));
            waddch(win, *p);
            wattroff(win, COLOR_PAIR(JSON_COLOR_NUMBER));
        }
        else if (strncmp(p, "true", 4) == 0 || strncmp(p, "false", 5) == 0) {
            int len = (*p == 't') ? 4 : 5;
            wattron(win, COLOR_PAIR(JSON_COLOR_BOOLEAN));
            for (int i = 0; i < len && *p; i++, p++, col++) {
                waddch(win, *p);
            }
            wattroff(win, COLOR_PAIR(JSON_COLOR_BOOLEAN));
            p--;
            col--;
        }
        else if (strncmp(p, "null", 4) == 0) {
            wattron(win, COLOR_PAIR(JSON_COLOR_NULL));
            for (int i = 0; i < 4 && *p; i++, p++, col++) {
                waddch(win, *p);
            }
            wattroff(win, COLOR_PAIR(JSON_COLOR_NULL));
            p--;
            col--;
        }
        else {
            waddch(win, *p);
        }

        col++;
        p++;
    }

    return (y - start_y) + 1;
}

int json_display_document(WINDOW *win, const bson_t *doc, int start_y, int start_x,
                          int max_height, int max_width) {
    if (!win || !doc) {
        return 0;
    }

    char *json = json_format_bson(doc);
    if (!json) {
        mvwprintw(win, start_y, start_x, "Error: Could not format document");
        return 1;
    }

    int lines = json_display_string(win, json, start_y, start_x, max_height, max_width, 0);

    bson_free(json);

    return lines;
}
