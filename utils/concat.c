#include <string.h>

char* concat(char *hardcoded_text, char *reason)
{
    char *combination = malloc(strlen(hardcoded_text) + strlen(reason) + 1);
    strcpy(combination, hardcoded_text);
    strcat(combination, reason);
    return combination;
}
