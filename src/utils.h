#ifndef __UTILS_H_INCLUDED__
#define __UTILS_H_INCLUDED__

#include <stddef.h>
#include <stdint.h>

int max(int a, int b);
int min(int a, int b);
int find_str(char **strs, size_t len, char *to_find);

void cairo_set_source_u32(void *cairo, uint32_t color);

#endif
