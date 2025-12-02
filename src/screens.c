#include "screens.h"
#include "input.h"
#include "json_display.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>


// PDCurses en Windows usa códigos distintos a ncurses
// macros para teclas de flecha en ambos
#define IS_KEY_UP(ch) ((ch) == KEY_UP || (ch) == 450)
#define IS_KEY_DOWN(ch) ((ch) == KEY_DOWN || (ch) == 456)
#define IS_KEY_LEFT(ch) ((ch) == KEY_LEFT || (ch) == 452)
#define IS_KEY_RIGHT(ch) ((ch) == KEY_RIGHT || (ch) == 454)
#define IS_KEY_PPAGE(ch) ((ch) == KEY_PPAGE || (ch) == 451) // Re Pág
#define IS_KEY_NPAGE(ch) ((ch) == KEY_NPAGE || (ch) == 457) // Av Pág

app_state_t *app_state_new(void) {
  app_state_t *state = calloc(1, sizeof(app_state_t));
  if (!state) {
    return NULL;
  }

  state->mongo_ctx = mongo_context_new();
  if (!state->mongo_ctx) {
    free(state);
    return NULL;
  }

  state->current_screen = SCREEN_CONNECTION;
  state->previous_screen = SCREEN_CONNECTION;
  state->uri_buffer[0] = '\0';
  state->databases = NULL;
  state->db_count = 0;
  state->db_selected = 0;
  state->collections = NULL;
  state->coll_count = 0;
  state->coll_selected = 0;
  state->current_db[0] = '\0';
  state->current_collection[0] = '\0';
  state->documents = NULL;
  state->doc_count = 0;
  state->doc_page = 0;
  state->doc_per_page = 10;
  state->doc_selected = 0;
  state->doc_scroll_offset = 0;
  state->total_documents = 0;
  state->filter_json[0] = '\0';
  state->current_filter = NULL;
  state->message[0] = '\0';
  state->message_type = MSG_INFO;
  state->show_message = false;
  state->should_quit = false;

  // empezar con URI vacío
  state->uri_buffer[0] = '\0';

  return state;
}

void app_state_free(app_state_t *state) {
  if (!state) {
    return;
  }

  if (state->mongo_ctx) {
    mongo_context_free(state->mongo_ctx);
  }

  if (state->databases) {
    free_string_array(&state->databases, state->db_count);
  }

  if (state->collections) {
    free_string_array(&state->collections, state->coll_count);
  }

  if (state->documents) {
    mongo_free_documents(state->documents, state->doc_count);
    state->documents = NULL;
  }

  if (state->current_filter) {
    bson_destroy(state->current_filter);
  }

  free(state);
}

void app_set_message(app_state_t *state, const char *message, msg_type_t type) {
  if (!state || !message) {
    return;
  }

  safe_strncpy(state->message, message, sizeof(state->message));
  state->message_type = type;
  state->show_message = true;
}

screen_id_t screen_connection(app_state_t *state) {
  clear();

  int height, width;
  tui_get_size(&height, &width);

  WINDOW *win = newwin(12, 70, (height - 12) / 2, (width - 70) / 2);
  keypad(win, TRUE);

  tui_draw_box(win, "MongoDB Connection");

  mvwprintw(win, 2, 2, "MongoDB URI:");

  mvwprintw(win, 7, 2, "Examples:");
  mvwprintw(win, 8, 4, "mongodb://localhost:27017");
  mvwprintw(win, 9, 4,
            "mongodb://user:pass@localhost:27017/db?authSource=admin");

  tui_draw_status(win, "Type URI | ENTER: Connect | ESC: Quit | DELETE: Clear");

  // dibujar URI actual si existe
  if (state->uri_buffer[0] != '\0') {
    wmove(win, 3, 2);
    wclrtoeol(win);
    mvwprintw(win, 3, 2, "%s", state->uri_buffer);
  }

  wrefresh(win);

  // obtener input directo
  wmove(win, 3, 2);
  bool got_input =
      input_get_string(win, 3, 2, state->uri_buffer, sizeof(state->uri_buffer),
                       state->uri_buffer);

  if (!got_input) {
    // User pressed ESC
    state->should_quit = true;
    delwin(win);
    return SCREEN_QUIT;
  }

  // Validate URI
  if (is_empty_string(state->uri_buffer)) {
    tui_show_message(win, 5, "Error: URI cannot be empty", MSG_ERROR);
    wrefresh(win);
    napms(2000);
    delwin(win);
    return SCREEN_CONNECTION;
  }

  // Attempt connection
  tui_show_message(win, 5, "Connecting...", MSG_INFO);
  wrefresh(win);

  if (mongo_connect(state->mongo_ctx, state->uri_buffer)) {
    app_set_message(state, "Connected successfully!", MSG_SUCCESS);
    delwin(win);
    return SCREEN_DATABASE_LIST;
  } else {
    char err_msg[256];
    snprintf(err_msg, sizeof(err_msg), "Connection failed: %s",
             mongo_get_error(state->mongo_ctx));
    tui_show_message(win, 5, err_msg, MSG_ERROR);
    wrefresh(win);
    napms(4000);
    delwin(win);
    return SCREEN_CONNECTION;
  }
}

screen_id_t screen_database_list(app_state_t *state) {
  clear();

  // Load databases
  if (state->databases) {
    free_string_array(&state->databases, state->db_count);
  }

  state->databases = mongo_list_databases(state->mongo_ctx, &state->db_count);
  if (!state->databases) {
    app_set_message(state, "Failed to list databases", MSG_ERROR);
    return SCREEN_CONNECTION;
  }

  if (state->db_count == 0) {
    app_set_message(state, "No databases found", MSG_WARNING);
  }

  WINDOW *win = newwin(LINES, COLS, 0, 0);
  keypad(win, TRUE);

  tui_draw_box(win, "Select Database");

  mvwprintw(win, 1, 2, "Databases (%d):", state->db_count);
  tui_draw_hline(win, 2, 1, COLS - 2);

  if (state->show_message) {
    tui_show_message(win, LINES - 3, state->message, state->message_type);
    state->show_message = false;
  }

  tui_draw_status(
      win, "UP/DOWN: Navigate | ENTER: Select | Q: Disconnect | F1: Help");

  int selected = state->db_selected;
  if (selected >= state->db_count) {
    selected = 0;
  }

  int scroll_offset = 0;
  int visible_lines = LINES - 7;

  int ch;
  bool redraw = true;

  while (true) {
    // Draw databases when needed
    if (redraw) {
      // Clear entire window and redraw everything
      wclear(win);
      tui_draw_box(win, "Select Database");
      tui_draw_status(
          win, "UP/DOWN: Navigate | ENTER: Select | Q: Disconnect | F1: Help");

      mvwprintw(win, 1, 2, "Databases (%d):", state->db_count);
      tui_draw_hline(win, 2, 1, COLS - 2);

      if (state->show_message) {
        tui_show_message(win, LINES - 3, state->message, state->message_type);
        state->show_message = false;
      }

      // Adjust scroll offset
      if (selected < scroll_offset) {
        scroll_offset = selected;
      } else if (selected >= scroll_offset + visible_lines) {
        scroll_offset = selected - visible_lines + 1;
      }

      // Draw databases
      for (int i = scroll_offset;
           i < state->db_count && i < scroll_offset + visible_lines; i++) {
        int y = 3 + (i - scroll_offset);

        if (i == selected) {
          wattron(win, COLOR_PAIR(COLOR_PAIR_SELECTED));
          mvwprintw(win, y, 2, " > %s", state->databases[i]);
          wattroff(win, COLOR_PAIR(COLOR_PAIR_SELECTED));
        } else {
          mvwprintw(win, y, 2, "   %s", state->databases[i]);
        }
      }

      wrefresh(win);
      redraw = false;
    }

    // Wait for input
    ch = wgetch(win);

    if (IS_KEY_UP(ch) && selected > 0) {
      selected--;
      redraw = true;
    } else if (IS_KEY_DOWN(ch) && selected < state->db_count - 1) {
      selected++;
      redraw = true;
    } else if (ch == '\n' || ch == KEY_ENTER || ch == 10 || ch == 13) {
      state->db_selected = selected;
      safe_strncpy(state->current_db, state->databases[selected],
                   sizeof(state->current_db));
      delwin(win);
      return SCREEN_COLLECTION_LIST;
    } else if (ch == 'q' || ch == 'Q') {
      mongo_disconnect(state->mongo_ctx);
      delwin(win);
      return SCREEN_CONNECTION;
    } else if (ch == KEY_F(1)) {
      delwin(win);
      return SCREEN_HELP;
    }
  }

  delwin(win);
  return SCREEN_QUIT;
}

screen_id_t screen_collection_list(app_state_t *state) {
  clear();

  // Load collections
  if (state->collections) {
    free_string_array(&state->collections, state->coll_count);
  }

  state->collections = mongo_list_collections(
      state->mongo_ctx, state->current_db, &state->coll_count);
  if (!state->collections) {
    app_set_message(state, "Failed to list collections", MSG_ERROR);
    return SCREEN_DATABASE_LIST;
  }

  if (state->coll_count == 0) {
    app_set_message(state, "No collections found in this database",
                    MSG_WARNING);
  }

  WINDOW *win = newwin(LINES, COLS, 0, 0);
  keypad(win, TRUE);

  char title[128];
  snprintf(title, sizeof(title), "Database: %s - Select Collection",
           state->current_db);
  tui_draw_box(win, title);

  mvwprintw(win, 1, 2, "Collections (%d):", state->coll_count);
  tui_draw_hline(win, 2, 1, COLS - 2);

  if (state->show_message) {
    tui_show_message(win, LINES - 3, state->message, state->message_type);
    state->show_message = false;
  }

  tui_draw_status(win, "UP/DOWN: Navigate | ENTER: Select | C: Create | D: "
                       "Delete | B: Back | Q: Disconnect | F1: Help");

  int selected = state->coll_selected;
  if (selected >= state->coll_count) {
    selected = 0;
  }

  int scroll_offset = 0;
  int visible_lines = LINES - 7;

  int ch;
  bool redraw = true;

  while (true) {
    // Draw collections when needed
    if (redraw) {
      // Clear entire window and redraw everything
      wclear(win);
      tui_draw_box(win, title);
      tui_draw_status(win, "UP/DOWN: Navigate | ENTER: Select | C: Create | D: "
                           "Delete | B: Back | Q: Disconnect | F1: Help");

      mvwprintw(win, 1, 2, "Collections (%d):", state->coll_count);
      tui_draw_hline(win, 2, 1, COLS - 2);

      if (state->show_message) {
        tui_show_message(win, LINES - 3, state->message, state->message_type);
        state->show_message = false;
      }

      // Adjust scroll offset
      if (selected < scroll_offset) {
        scroll_offset = selected;
      } else if (selected >= scroll_offset + visible_lines) {
        scroll_offset = selected - visible_lines + 1;
      }

      // Draw collections
      for (int i = scroll_offset;
           i < state->coll_count && i < scroll_offset + visible_lines; i++) {
        int y = 3 + (i - scroll_offset);

        if (i == selected) {
          wattron(win, COLOR_PAIR(COLOR_PAIR_SELECTED));
          mvwprintw(win, y, 2, " > %s", state->collections[i]);
          wattroff(win, COLOR_PAIR(COLOR_PAIR_SELECTED));
        } else {
          mvwprintw(win, y, 2, "   %s", state->collections[i]);
        }
      }

      wrefresh(win);
      redraw = false;
    }

    // Wait for input
    ch = wgetch(win);

    if (IS_KEY_UP(ch) && selected > 0) {
      selected--;
      redraw = true;
    } else if (IS_KEY_DOWN(ch) && selected < state->coll_count - 1) {
      selected++;
      redraw = true;
    } else if ((ch == '\n' || ch == KEY_ENTER || ch == 10 || ch == 13) &&
               state->coll_count > 0) {
      state->coll_selected = selected;
      safe_strncpy(state->current_collection, state->collections[selected],
                   sizeof(state->current_collection));
      state->doc_page = 0;
      delwin(win);
      return SCREEN_DOCUMENT_VIEWER;
    } else if (ch == 'c' || ch == 'C') {
      // Create new collection
      char coll_name[256] = {0};
      if (input_text_single("Create Collection", "Collection name:", coll_name,
                            sizeof(coll_name),
                            "Enter the name for the new collection")) {
        if (!is_empty_string(coll_name)) {
          if (mongo_create_collection(state->mongo_ctx, state->current_db,
                                      coll_name)) {
            app_set_message(state, "Collection created successfully!",
                            MSG_SUCCESS);
            delwin(win);
            return SCREEN_COLLECTION_LIST;
          } else {
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg),
                     "Failed to create collection: %s",
                     mongo_get_error(state->mongo_ctx));
            app_set_message(state, err_msg, MSG_ERROR);
            redraw = true;
          }
        }
      }
      redraw = true;
    } else if ((ch == 'd' || ch == 'D') && state->coll_count > 0) {
      // Delete selected collection
      char confirm_msg[256];
      snprintf(confirm_msg, sizeof(confirm_msg),
               "Are you sure you want to delete collection '%s'?",
               state->collections[selected]);

      if (tui_confirm("Delete Collection", confirm_msg)) {
        if (mongo_drop_collection(state->mongo_ctx, state->current_db,
                                  state->collections[selected])) {
          app_set_message(state, "Collection deleted successfully!",
                          MSG_SUCCESS);
          delwin(win);
          return SCREEN_COLLECTION_LIST;
        } else {
          char err_msg[256];
          snprintf(err_msg, sizeof(err_msg), "Failed to delete collection: %s",
                   mongo_get_error(state->mongo_ctx));
          app_set_message(state, err_msg, MSG_ERROR);
          redraw = true;
        }
      }
      redraw = true;
    } else if (ch == 'b' || ch == 'B') {
      delwin(win);
      return SCREEN_DATABASE_LIST;
    } else if (ch == 'q' || ch == 'Q') {
      mongo_disconnect(state->mongo_ctx);
      delwin(win);
      return SCREEN_CONNECTION;
    } else if (ch == KEY_F(1)) {
      delwin(win);
      return SCREEN_HELP;
    }
  }

  delwin(win);
  return SCREEN_QUIT;
}

bool load_documents(app_state_t *state) {
  if (!state) {
    return false;
  }

  // Free previous documents
  if (state->documents) {
    mongo_free_documents(state->documents, state->doc_count);
    state->documents = NULL;
    state->doc_count = 0;
  }

  // Get total count
  state->total_documents =
      mongo_count_documents(state->mongo_ctx, state->current_db,
                            state->current_collection, state->current_filter);

  if (state->total_documents < 0) {
    return false;
  }

  // Load current page
  int skip = state->doc_page * state->doc_per_page;
  state->documents = mongo_find_documents(
      state->mongo_ctx, state->current_db, state->current_collection,
      state->current_filter, skip, state->doc_per_page, &state->doc_count);

  return state->documents != NULL || state->total_documents == 0;
}

screen_id_t screen_document_viewer(app_state_t *state) {
  clear();

  // Load documents
  if (!load_documents(state)) {
    app_set_message(state, "Failed to load documents", MSG_ERROR);
    return SCREEN_COLLECTION_LIST;
  }

  if (state->doc_count == 0) {
    app_set_message(state, "No documents in this collection", MSG_WARNING);
  }

  WINDOW *win = newwin(LINES, COLS, 0, 0);
  keypad(win, TRUE);

  char title[256];
  snprintf(title, sizeof(title), "%s.%s - Documents", state->current_db,
           state->current_collection);
  tui_draw_box(win, title);

  int total_pages =
      (state->total_documents + state->doc_per_page - 1) / state->doc_per_page;
  if (total_pages == 0)
    total_pages = 1;

  tui_draw_status(win, "UP/DOWN: Select | PgUp/PgDn: Page | I: Insert | E: "
                       "Edit | D: Delete | B: Back | R: Refresh");

  // Ensure selected index is valid
  if (state->doc_selected >= state->doc_count) {
    state->doc_selected = state->doc_count > 0 ? state->doc_count - 1 : 0;
  }

  bool redraw = true;
  int ch;

  while (true) {
    if (redraw) {
      // Clear entire window and redraw everything
      wclear(win);
      tui_draw_box(win, title);
      tui_draw_status(win, "UP/DOWN: Select | PgUp/PgDn: Page | I: Insert | E: "
                           "Edit | D: Delete | B: Back | R: Refresh");

      char info[128];
      snprintf(info, sizeof(info), "Total: %lld | Page %d/%d | Selected: %d/%d",
               state->total_documents, state->doc_page + 1, total_pages,
               state->doc_selected + 1, state->doc_count);
      mvwprintw(win, 1, 2, "%s", info);
      tui_draw_hline(win, 2, 1, COLS - 2);

      if (state->show_message) {
        tui_show_message(win, LINES - 3, state->message, state->message_type);
        state->show_message = false;
      }

      // Display documents with selection highlight
      int y = 3;
      int max_doc_lines = 8; // Lines per document

      for (int i = 0; i < state->doc_count && y < LINES - 4; i++) {
        if (y >= LINES - 4)
          break;

        char doc_header[64];
        int doc_num = i + 1 + (state->doc_page * state->doc_per_page);

        // Highlight selected document
        if (i == state->doc_selected) {
          wattron(win, COLOR_PAIR(COLOR_PAIR_SELECTED));
          snprintf(doc_header, sizeof(doc_header), " > Document %d: [SELECTED]",
                   doc_num);
          mvwprintw(win, y++, 2, "%-*s", COLS - 4, doc_header);
          wattroff(win, COLOR_PAIR(COLOR_PAIR_SELECTED));
        } else {
          snprintf(doc_header, sizeof(doc_header), "   Document %d:", doc_num);
          mvwprintw(win, y++, 2, "%s", doc_header);
        }

        char *json = json_format_bson(state->documents[i]);
        if (json) {
          json_display_string(win, json, y, 4, max_doc_lines, COLS - 6, 0);
          y += max_doc_lines;
          bson_free(json);
        }

        if (i < state->doc_count - 1 && y < LINES - 4) {
          tui_draw_hline(win, y++, 1, COLS - 2);
        }
      }

      wrefresh(win);
      redraw = false;
    }

    // Wait for input
    ch = wgetch(win);

    if (IS_KEY_UP(ch) && state->doc_selected > 0) {
      state->doc_selected--;
      redraw = true;
    } else if (IS_KEY_DOWN(ch) && state->doc_selected < state->doc_count - 1) {
      state->doc_selected++;
      redraw = true;
    } else if (IS_KEY_NPAGE(ch) && state->doc_page < total_pages - 1) {
      state->doc_page++;
      state->doc_selected = 0;
      delwin(win);
      return SCREEN_DOCUMENT_VIEWER;
    } else if (IS_KEY_PPAGE(ch) && state->doc_page > 0) {
      state->doc_page--;
      state->doc_selected = 0;
      delwin(win);
      return SCREEN_DOCUMENT_VIEWER;
    } else if (ch == 'i' || ch == 'I') {
      delwin(win);
      return SCREEN_DOCUMENT_INSERT;
    } else if ((ch == 'e' || ch == 'E') && state->doc_count > 0) {
      // Edit selected document - format with line breaks for readability
      char json_buffer[INPUT_MAX_LENGTH * 4];
      if (!json_format_bson_editable(state->documents[state->doc_selected],
                                     json_buffer, sizeof(json_buffer))) {
        app_set_message(state, "Failed to format document", MSG_ERROR);
        redraw = true;
        continue;
      }

      // Show editor
      bool edited =
          input_text_editor("Edit Document", json_buffer, sizeof(json_buffer),
                            "Edit the JSON. Press F2 to save, ESC to cancel.");

      if (edited && !is_empty_string(json_buffer)) {
        // Parse edited JSON
        bson_error_t error;
        bson_t *updated_doc = mongo_json_to_bson(json_buffer, &error);

        if (!updated_doc) {
          char err_msg[256];
          snprintf(err_msg, sizeof(err_msg), "Invalid JSON: %s", error.message);
          app_set_message(state, err_msg, MSG_ERROR);
          redraw = true;
          continue;
        }

        // Use _id as filter to update the specific document
        bson_t *filter = bson_new();
        bson_iter_t iter;
        if (bson_iter_init_find(&iter, state->documents[state->doc_selected],
                                "_id")) {
          BCON_APPEND(filter, "_id", BCON_OID(bson_iter_oid(&iter)));
        } else {
          // No _id found, use entire document as filter (risky!)
          bson_destroy(filter);
          filter = bson_copy(state->documents[state->doc_selected]);
        }

        // Create update operation ($set)
        bson_t *update = BCON_NEW("$set", "{", "}");
        bson_iter_t new_iter;
        if (bson_iter_init(&new_iter, updated_doc)) {
          bson_t child;
          BSON_APPEND_DOCUMENT_BEGIN(update, "$set", &child);
          while (bson_iter_next(&new_iter)) {
            const char *key = bson_iter_key(&new_iter);
            // Skip _id field (cannot be modified)
            if (strcmp(key, "_id") == 0)
              continue;

            bson_value_t value;
            bson_value_copy(bson_iter_value(&new_iter), &value);
            bson_append_value(&child, key, -1, &value);
            bson_value_destroy(&value);
          }
          bson_append_document_end(update, &child);
        }

        // Perform update
        long long modified = mongo_update_documents(
            state->mongo_ctx, state->current_db, state->current_collection,
            filter, update, false);

        bson_destroy(filter);
        bson_destroy(update);
        bson_destroy(updated_doc);

        if (modified > 0) {
          app_set_message(state, "Document updated successfully!", MSG_SUCCESS);
          delwin(win);
          return SCREEN_DOCUMENT_VIEWER;
        } else {
          app_set_message(state, "Failed to update document", MSG_ERROR);
          redraw = true;
        }
      } else {
        redraw = true;
      }
    } else if ((ch == 'd' || ch == 'D') && state->doc_count > 0) {
      // Confirm and delete selected document
      if (tui_confirm("Delete Document",
                      "Are you sure you want to delete this document?")) {
        // Use _id as filter for deletion (handle any type)
        bson_t *filter = bson_new();
        bson_iter_t iter;

        if (bson_iter_init_find(&iter, state->documents[state->doc_selected],
                                "_id")) {
          // Copy _id value (works for any BSON type)
          bson_value_t id_value;
          bson_value_copy(bson_iter_value(&iter), &id_value);
          bson_append_value(filter, "_id", -1, &id_value);
          bson_value_destroy(&id_value);
        } else {
          // No _id found, use entire document as filter (fallback)
          bson_destroy(filter);
          filter = bson_copy(state->documents[state->doc_selected]);
        }

        long long deleted =
            mongo_delete_documents(state->mongo_ctx, state->current_db,
                                   state->current_collection, filter, false);
        bson_destroy(filter);

        if (deleted > 0) {
          app_set_message(state, "Document deleted successfully!", MSG_SUCCESS);
          // Adjust selection if needed
          if (state->doc_selected >= state->doc_count - 1) {
            state->doc_selected = state->doc_count - 2;
            if (state->doc_selected < 0)
              state->doc_selected = 0;
          }
          delwin(win);
          return SCREEN_DOCUMENT_VIEWER;
        } else {
          char err_msg[256];
          snprintf(err_msg, sizeof(err_msg), "Delete failed: %s",
                   mongo_get_error(state->mongo_ctx));
          app_set_message(state, err_msg, MSG_ERROR);
          redraw = true;
        }
      }
      redraw = true;
    } else if (ch == 'b' || ch == 'B') {
      delwin(win);
      return SCREEN_COLLECTION_LIST;
    } else if (ch == 'r' || ch == 'R') {
      delwin(win);
      return SCREEN_DOCUMENT_VIEWER;
    } else if (ch == KEY_F(1)) {
      state->previous_screen = SCREEN_DOCUMENT_VIEWER;
      delwin(win);
      return SCREEN_HELP;
    } else if (ch == 'q' || ch == 'Q') {
      mongo_disconnect(state->mongo_ctx);
      delwin(win);
      return SCREEN_CONNECTION;
    }
  }

  delwin(win);
  return SCREEN_QUIT;
}

screen_id_t screen_document_insert(app_state_t *state) {
  char json_buffer[INPUT_MAX_LENGTH * 4] = "{\n  \n}";

  bool result = input_text_editor(
      "Insert Document", json_buffer, sizeof(json_buffer),
      "Enter JSON document. Press F2 to save, ESC to cancel.");

  if (result && !is_empty_string(json_buffer)) {
    bson_error_t error;
    bson_t *doc = mongo_json_to_bson(json_buffer, &error);

    if (!doc) {
      char err_msg[256];
      snprintf(err_msg, sizeof(err_msg), "Invalid JSON: %s", error.message);
      app_set_message(state, err_msg, MSG_ERROR);
    } else {
      if (mongo_insert_document(state->mongo_ctx, state->current_db,
                                state->current_collection, doc)) {
        app_set_message(state, "Document inserted successfully!", MSG_SUCCESS);
      } else {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Insert failed: %s",
                 mongo_get_error(state->mongo_ctx));
        app_set_message(state, err_msg, MSG_ERROR);
      }
      bson_destroy(doc);
    }
  }

  return SCREEN_DOCUMENT_VIEWER;
}

screen_id_t screen_help(app_state_t *state) {
  clear();

  WINDOW *win = newwin(LINES, COLS, 0, 0);
  keypad(win, TRUE);

  tui_draw_box(win, "Help - MongoDB TUI Client");

  int y = 2;
  mvwprintw(win, y++, 2, "Navigation:");
  mvwprintw(win, y++, 4, "UP/DOWN       - Navigate lists");
  mvwprintw(win, y++, 4, "ENTER         - Select item");
  mvwprintw(win, y++, 4, "B             - Go back");
  mvwprintw(win, y++, 4, "Q             - Quit/Disconnect");
  y++;

  mvwprintw(win, y++, 2, "Document Viewer:");
  mvwprintw(win, y++, 4, "PgUp/PgDn     - Navigate pages");
  mvwprintw(win, y++, 4, "I             - Insert document");
  mvwprintw(win, y++, 4, "R             - Refresh");
  mvwprintw(win, y++, 4, "F             - Filter (not implemented)");
  y++;

  mvwprintw(win, y++, 2, "Insert Document:");
  mvwprintw(win, y++, 4, "F2            - Save document");
  mvwprintw(win, y++, 4, "ESC           - Cancel");
  y++;

  mvwprintw(win, y++, 2, "General:");
  mvwprintw(win, y++, 4, "F1            - Show this help");
  mvwprintw(win, y++, 4, "ESC           - Cancel/Back");
  y++;

  mvwprintw(win, y++, 2, "MongoDB TUI Client v1.0");
  mvwprintw(win, y++, 2, "Press any key to return...");

  tui_draw_status(win, "Press any key to return");

  wrefresh(win);
  wgetch(win);

  delwin(win);
  return state->previous_screen;
}
