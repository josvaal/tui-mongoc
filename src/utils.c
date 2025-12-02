#include "utils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


bool safe_strncpy(char *dest, const char *src, size_t size) {
  if (!dest || !src || size == 0) {
    return false;
  }

  strncpy(dest, src, size - 1);
  dest[size - 1] = '\0';

  return strlen(src) < size;
}

char *trim_whitespace(char *str) {
  if (!str) {
    return NULL;
  }

  // quitar espacios al inicio
  while (isspace((unsigned char)*str)) {
    str++;
  }

  if (*str == '\0') {
    return str;
  }

  // quitar espacios al final
  char *end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end)) {
    end--;
  }

  *(end + 1) = '\0';

  return str;
}

bool is_empty_string(const char *str) {
  if (!str) {
    return true;
  }

  while (*str) {
    if (!isspace((unsigned char)*str)) {
      return false;
    }
    str++;
  }

  return true;
}

char *str_dup(const char *str) {
  if (!str) {
    return NULL;
  }

  size_t len = strlen(str);
  char *dup = malloc(len + 1);

  if (dup) {
    memcpy(dup, str, len + 1);
  }

  return dup;
}

void free_string_array(char ***arr, int count) {
  if (!arr || !*arr) {
    return;
  }

  for (int i = 0; i < count; i++) {
    if ((*arr)[i]) {
      free((*arr)[i]);
      (*arr)[i] = NULL;
    }
  }

  free(*arr);
  *arr = NULL;
}

void format_number(long long n, char *buffer, size_t size) {
  if (!buffer || size == 0) {
    return;
  }

  char temp[64];
  snprintf(temp, sizeof(temp), "%lld", n);

  int len = strlen(temp);
  int comma_count = (len - 1) / 3;
  int total_len = len + comma_count;

  if ((size_t)total_len >= size) {
    // buffer muy chico, copiar sin formatear
    safe_strncpy(buffer, temp, size);
    return;
  }

  buffer[total_len] = '\0';

  int src_pos = len - 1;
  int dst_pos = total_len - 1;
  int digit_count = 0;

  while (src_pos >= 0) {
    if (digit_count == 3) {
      buffer[dst_pos--] = ',';
      digit_count = 0;
    }
    buffer[dst_pos--] = temp[src_pos--];
    digit_count++;
  }
}

const char *get_error_message(int error_code) {
  switch (error_code) {
  case 0:
    return "Success";
  case -1:
    return "Connection failed";
  case -2:
    return "Invalid URI";
  case -3:
    return "Query failed";
  case -4:
    return "Insert failed";
  case -5:
    return "Update failed";
  case -6:
    return "Delete failed";
  case -7:
    return "Memory allocation failed";
  case -8:
    return "Invalid input";
  case -9:
    return "Database not found";
  case -10:
    return "Collection not found";
  default:
    return "Unknown error";
  }
}

void center_text(const char *text, int width, char *buffer, size_t size) {
  if (!text || !buffer || size == 0) {
    return;
  }

  int text_len = strlen(text);

  if (text_len >= width) {
    safe_strncpy(buffer, text, size);
    return;
  }

  int padding = (width - text_len) / 2;
  int pos = 0;

  // espacios a la izq
  for (int i = 0; i < padding && pos < (int)size - 1; i++) {
    buffer[pos++] = ' ';
  }

  // agregar texto
  for (int i = 0; i < text_len && pos < (int)size - 1; i++) {
    buffer[pos++] = text[i];
  }

  // espacios a la der
  while (pos < width && pos < (int)size - 1) {
    buffer[pos++] = ' ';
  }

  buffer[pos] = '\0';
}
