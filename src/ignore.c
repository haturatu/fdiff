#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "ignore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <bsd/string.h>

static char *trim_whitespace(char *s) {
    if (!s) return s;
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) *(--end) = '\0';
    return s;
}

int ignore_load(const char *path, IgnoreList *ignore) {
    if (!ignore) return -1;
    ignore->patterns = NULL;
    ignore->count = 0;
    ignore->root = NULL;

    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        return -1;
    }
    ignore->root = strdup(cwd);
    if (!ignore->root) return -1;

    FILE *f = fopen(path, "r");
    if (!f) {
        
        return 0;
    }

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        char *ln = trim_whitespace(line);
        if (!ln || ln[0] == '\0' || ln[0] == '#') continue;

        IgnorePattern pat = {0};
        pat.is_regex = false;
        pat.negated = false;
        pat.dir_only = false;
        pat.anchored = false;
        pat.matches_component = false;
        pat.pattern = NULL;

        if (ln[0] == '!') {
            pat.negated = true;
            ln++;
            ln = trim_whitespace(ln);
            if (!ln || ln[0] == '\0') continue;
        }

        size_t L = strlen(ln);
        
        if (L > 0 && ln[L-1] == '/') {
            pat.dir_only = true;
            ln[L-1] = '\0';
            L--;
        }

        if (L > 0 && ln[0] == '/') {
            pat.anchored = true;
            
            ln++;
            ln = trim_whitespace(ln);
        }

        
        if (strncmp(ln, "re:", 3) == 0) {
            const char *r = ln + 3;
            pat.is_regex = true;
            pat.pattern = strdup(r);
            if (!pat.pattern) goto oom;
            if (regcomp(&pat.regex, pat.pattern, REG_NOSUB | REG_EXTENDED) != 0) {
                free(pat.pattern);
                continue; 
            }
        } else {
            
            pat.pattern = strdup(ln);
            if (!pat.pattern) goto oom;
            
            bool has_slash = strchr(ln, '/') != NULL;
            bool has_glob = (strchr(ln, '*') || strchr(ln, '?') || strstr(ln, "["));
            if (!has_slash && !has_glob) pat.matches_component = true;
        }

        
        IgnorePattern *tmp = realloc(ignore->patterns, (ignore->count + 1) * sizeof(IgnorePattern));
        if (!tmp) {
            free(pat.pattern);
            goto oom;
        }
        ignore->patterns = tmp;
        ignore->patterns[ignore->count++] = pat;
    }

    fclose(f);
    return 0;

oom:
    fclose(f);
    for (size_t i = 0; i < ignore->count; i++) {
        free(ignore->patterns[i].pattern);
        if (ignore->patterns[i].is_regex) regfree(&ignore->patterns[i].regex);
    }
    free(ignore->patterns);
    free(ignore->root);
    ignore->patterns = NULL;
    ignore->count = 0;
    ignore->root = NULL;
    return -1;
}


bool ignore_match(const IgnoreList *ignore, const char *relpath, int is_dir) {
    if (!ignore || ignore->count == 0) return false;
    if (!relpath) return false;

    
    char tmp[PATH_MAX];
    if (strlen(relpath) >= sizeof(tmp)) return false;
    strlcpy(tmp, relpath, sizeof(tmp));

    
    bool matched = false;
    for (size_t i = 0; i < ignore->count; i++) {
        IgnorePattern *p = &ignore->patterns[i];
        bool this_match = false;

        if (p->is_regex) {
            
            if (regexec(&p->regex, tmp, 0, NULL, 0) == 0) this_match = true;
        } else if (p->matches_component) {
            
            if (strcmp(p->pattern, ".") == 0 && strcmp(tmp, ".") == 0) this_match = true;
            char *tok = tmp;
            while (tok) {
                char *slash = strchr(tok, '/');
                size_t len = slash ? (size_t)(slash - tok) : strlen(tok);
                if (len == strlen(p->pattern) && strncmp(tok, p->pattern, len) == 0) {
                    this_match = true;
                    break;
                }
                if (!slash) break;
                tok = slash + 1;
            }
            
            if (this_match && p->anchored) {
                if (strncmp(tmp, p->pattern, strlen(p->pattern)) != 0) this_match = false;
            }
        } else {
            
            
            if (p->anchored) {
                if (fnmatch(p->pattern, tmp, FNM_PATHNAME) == 0) this_match = true;
            } else {
                if (fnmatch(p->pattern, tmp, FNM_PATHNAME) == 0) {
                    this_match = true;
                } else {
                    char *s = tmp;
                    while ((s = strchr(s, '/')) != NULL) {
                        s++; 
                        if (fnmatch(p->pattern, s, FNM_PATHNAME) == 0) {
                            this_match = true;
                            break;
                        }
                    }
                }
            }
        }

        if (this_match) {
            if (p->dir_only && !is_dir) {
                this_match = false;
            }
        }

        if (this_match) {
            matched = !p->negated;
        }
    }

    return matched;
}

void ignore_free(IgnoreList *ignore) {
    if (!ignore) return;
    for (size_t i = 0; i < ignore->count; i++) {
        if (ignore->patterns[i].pattern) free(ignore->patterns[i].pattern);
        if (ignore->patterns[i].is_regex) regfree(&ignore->patterns[i].regex);
    }
    free(ignore->patterns);
    ignore->patterns = NULL;
    ignore->count = 0;
    if (ignore->root) free(ignore->root);
    ignore->root = NULL;
}

