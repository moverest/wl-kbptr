#ifndef __LOG_H_INCLUDED__
#define __LOG_H_INCLUDED__

#include <stdio.h>

#define LOG_ERR(msg, ...) \
    fprintf(stderr, "\x1b[31merr:\x1b[0m " msg "\n", ##__VA_ARGS__)

#define LOG_WARN(msg, ...) \
    fprintf(stderr, "\x1b[33mwarn:\x1b[0m " msg "\n", ##__VA_ARGS__)

#define LOG_INFO(msg, ...) \
    fprintf(stderr, "\x1b[34minfo:\x1b[0m " msg "\n", ##__VA_ARGS__)

#ifdef DEBUG
#define LOG_DEBUG(msg, ...) \
    fprintf(stderr, "\x1b[35mdebug:\x1b[0m " msg "\n", ##__VA_ARGS__)
#else
#define LOG_DEBUG(msg, ...) ((void)0)
#endif

#endif
