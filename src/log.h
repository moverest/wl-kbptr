#include <stdio.h>

#define LOG_ERR(msg, ...) \
    fprintf(stderr, "\x1b[31merr:\x1b[0m " msg "\n", ##__VA_ARGS__)
