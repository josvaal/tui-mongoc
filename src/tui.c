#include "tui.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>


bool tui_init(void) {
  // inicializar ncurses
  initscr();

  // ver si anduvo
  if (stdscr == NULL) {
    return false;
  }

  // configurar ncurses
  cbreak();             // sin buffer de línea
  noecho();             // no mostrar teclas
  keypad(stdscr, TRUE); // activar teclas de función y flechas
  curs_set(1);          // mostrar cursor

  // inicializar colores si hay
  if (has_colors()) {
    start_color();
    use_default_colors();

    // inicializar pares de colores
    init_pair(COLOR_PAIR_NORMAL, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_PAIR_HEADER, COLOR_BLACK, COLOR_CYAN);
    init_pair(COLOR_PAIR_SELECTED, COLOR_BLACK, COLOR_WHITE);
    init_pair(COLOR_PAIR_STATUS, COLOR_WHITE, COLOR_BLUE);
    init_pair(COLOR_PAIR_ERROR, COLOR_WHITE, COLOR_RED);
    init_pair(COLOR_PAIR_SUCCESS, COLOR_WHITE, COLOR_GREEN);
    init_pair(COLOR_PAIR_WARNING, COLOR_BLACK, COLOR_YELLOW);
    init_pair(COLOR_PAIR_INFO, COLOR_WHITE, COLOR_CYAN);
  }

  // limpiar pantalla
  clear();
  refresh();

  return true;
}

void tui_cleanup(void) {
  if (stdscr) {
    endwin();
  }
}

void tui_draw_box(WINDOW *win, const char *title) {
  if (!win) {
    return;
  }

  box(win, 0, 0);

  if (title) {
    int max_y, max_x;
    getmaxyx(win, max_y, max_x);
    (void)max_y; // Unused

    int title_len = strlen(title);
    int title_x = (max_x - title_len - 4) / 2;

    if (title_x > 0) {
      mvwprintw(win, 0, title_x, "[ %s ]", title);
    }
  }
}

void tui_draw_hline(WINDOW *win, int y, int x, int width) {
  if (!win) {
    return;
  }

  mvwhline(win, y, x, ACS_HLINE, width);
}

void tui_draw_status(WINDOW *win, const char *message) {
  if (!win || !message) {
    return;
  }

  int max_y, max_x;
  getmaxyx(win, max_y, max_x);

  // dibujar barra de estado abajo
  wattron(win, COLOR_PAIR(COLOR_PAIR_STATUS));
  mvwhline(win, max_y - 1, 0, ' ', max_x);
  mvwprintw(win, max_y - 1, 1, "%s", message);
  wattroff(win, COLOR_PAIR(COLOR_PAIR_STATUS));
}

void tui_show_message(WINDOW *win, int y, const char *message,
                      msg_type_t type) {
  if (!win || !message) {
    return;
  }

  int max_y, max_x;
  getmaxyx(win, max_y, max_x);
  (void)max_y; // Unused

  int color_pair;
  switch (type) {
  case MSG_ERROR:
    color_pair = COLOR_PAIR_ERROR;
    break;
  case MSG_SUCCESS:
    color_pair = COLOR_PAIR_SUCCESS;
    break;
  case MSG_WARNING:
    color_pair = COLOR_PAIR_WARNING;
    break;
  case MSG_INFO:
  default:
    color_pair = COLOR_PAIR_INFO;
    break;
  }

  wattron(win, COLOR_PAIR(color_pair));
  mvwhline(win, y, 1, ' ', max_x - 2);
  mvwprintw(win, y, 2, "%s", message);
  wattroff(win, COLOR_PAIR(color_pair));
}

bool tui_confirm(const char *title, const char *message) {
  int height = 7;
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

  mvwprintw(win, 2, 2, "%s", message);
  mvwprintw(win, 4, 2, "Press 'Y' to confirm or 'N' to cancel");

  wrefresh(win);

  int ch;
  bool result = false;

  while ((ch = wgetch(win)) != ERR) {
    if (ch == 'Y' || ch == 'y') {
      result = true;
      break;
    } else if (ch == 'N' || ch == 'n' || ch == 27) { // ESC
      result = false;
      break;
    }
  }

  delwin(win);
  touchwin(stdscr);
  refresh();

  return result;
}

void tui_clear_line(WINDOW *win, int y) {
  if (!win) {
    return;
  }

  int max_y, max_x;
  getmaxyx(win, max_y, max_x);
  (void)max_y; // Unused

  mvwhline(win, y, 1, ' ', max_x - 2);
}

void tui_get_size(int *height, int *width) {
  getmaxyx(stdscr, *height, *width);
}

void tui_draw_centered(WINDOW *win, int y, const char *text) {
  if (!win || !text) {
    return;
  }

  int max_y, max_x;
  getmaxyx(win, max_y, max_x);
  (void)max_y; // Unused

  int text_len = strlen(text);
  int x = (max_x - text_len) / 2;

  if (x > 0) {
    mvwprintw(win, y, x, "%s", text);
  }
}
