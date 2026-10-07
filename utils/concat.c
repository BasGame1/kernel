#include <stddef.h>

char* concat(char *hardcoded_text, char *reason) {
    static char combination[256];
    size_t i = 0;
    while (*hardcoded_text && i < sizeof(combination) - 1) {
        combination[i++] = *hardcoded_text++;
    }
    while (*reason && i < sizeof(combination) - 1) {
        combination[i++] = *reason++;
    }
    combination[i] = '\0';
    return combination;
}// concat
