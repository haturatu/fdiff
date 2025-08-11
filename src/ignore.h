#ifndef FDIFF_IGNORE_H
#define FDIFF_IGNORE_H
#include <stddef.h>
#include <stdbool.h>
#include <regex.h>

typedef struct {
    char *pattern;       
    bool is_regex;
    regex_t regex;       
    bool negated;
    bool dir_only;
    bool anchored;       
    bool matches_component; 
} IgnorePattern;

typedef struct {
    IgnorePattern *patterns;
    size_t count;
    char *root; 
} IgnoreList;

int ignore_load(const char *path, IgnoreList *ignore);
bool ignore_match(const IgnoreList *ignore, const char *relpath, int is_dir);
void ignore_free(IgnoreList *ignore);

#endif

