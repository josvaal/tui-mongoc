#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stddef.h>

// copiar string seguro con límite
bool safe_strncpy(char *dest, const char *src, size_t size);

// quitar espacios al inicio y final
char *trim_whitespace(char *str);

// ver si el string está vacío
bool is_empty_string(const char *str);

// duplicar un string
char *str_dup(const char *str);

// liberar array de strings
void free_string_array(char ***arr, int count);

// formatear números con comas
void format_number(long long n, char *buffer, size_t size);

// obtener mensaje de error legible
const char *get_error_message(int error_code);

// centrar texto
void center_text(const char *text, int width, char *buffer, size_t size);

#endif // UTILS_H
