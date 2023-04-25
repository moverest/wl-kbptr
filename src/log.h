#ifndef __SURFACE_LOG_H_INCLUDED__
#define __SURFACE_LOG_H_INCLUDED__

#include <stdio.h>

#define LOG_ERR(msg, ...) \
    fprintf(stderr, "\x1b[31merr:\x1b[0m " msg "\n", ##__VA_ARGS__)

#endif
