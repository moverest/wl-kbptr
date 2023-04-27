#include <string.h>

int min(int a, int b) {
    return a < b ? a : b;
}

int max(int a, int b) {
    return a > b ? a : b;
}

int find_str(char **strs, size_t len, char *to_find) {
    int matched_i = -1;
    for (int i = 0; i < len; i++) {
        if (strcmp(to_find, strs[i]) == 0) {
            matched_i = i;
            break;
        }
    }

    return matched_i;
}
