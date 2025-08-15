#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>
#include <bsd/string.h>

#include "ignore.h"
#include "store.h"

#define INDEX_DIR ".fdiff"
#define INDEX_FILE ".fdiff/index.bin"
#define IGNORE_FILE ".fdiffignore"

#define EXIT_OK 0
#define EXIT_FAIL 1
#define EXIT_NOFILE 3
#define EXIT_INTERNAL 4
#define EXIT_ALREADY_INITIALIZED 5
#define EXIT_ALREADY_ADDED 6
#define EXIT_DIFF_FOUND 7


static size_t next_capacity(size_t cur) {
    if (cur == 0) return 256;
    return cur * 2;
}


static int compute_file_hash(const char *path, uint64_t *out_hash, uint64_t *out_size) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return -1;
    }
    if (!S_ISREG(st.st_mode)) {
        close(fd);
        return -1;
    }

    if (st.st_size == 0) {
        *out_hash = 0;
        if (out_size) *out_size = 0;
        close(fd);
        return 0;
    }

    const uint64_t FNV_OFFSET = 14695981039346656037ULL;
    const uint64_t FNV_PRIME  = 1099511628211ULL;
    uint64_t h = FNV_OFFSET;

    const size_t BUF_SZ = 1024;
    unsigned char *buf = malloc(BUF_SZ);
    if (!buf) {
        close(fd);
        return -1;
    }

    ssize_t r;
    while ((r = read(fd, buf, BUF_SZ)) > 0) {
        for (ssize_t i = 0; i < r; i++) {
            h ^= (uint64_t)buf[i];
            h *= FNV_PRIME;
        }
    }
    free(buf);
    if (r < 0) {
        close(fd);
        return -1;
    }

    *out_hash = h;
    if (out_size) *out_size = (uint64_t)st.st_size;
    close(fd);
    return 0;
}


static char *normalize_relpath(const char *p) {
    if (!p) return NULL;
    
    size_t L = strlen(p);
    char *buf = malloc(L + 2);
    if (!buf) return NULL;
    strlcpy(buf, p, L+2);
    
    if (buf[0] == '.' && buf[1] == '/') {
        memmove(buf, buf + 2, strlen(buf + 2) + 1);
    }
    
    if (strlen(buf) > 1 && buf[strlen(buf)-1] == '/') {
        buf[strlen(buf)-1] = '\0';
    }
    return buf;
}


static int collect_files(char **start_paths, int nstart, const IgnoreList *ignore, FileRecord **out_list, size_t *out_count) {
    FileRecord *list = NULL;
    size_t count = 0, cap = 0;

    
    char **stack = NULL;
    size_t stack_count = 0, stack_cap = 0;

    for (int i = 0; i < nstart; i++) {
        char *norm = normalize_relpath(start_paths[i]);
        if (!norm) goto err;
        struct stat st;
        if (lstat(start_paths[i], &st) < 0) {
            
            free(norm);
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            
            int is_ig = ignore_match(ignore, norm, 1);
            if (!is_ig) {
                
                if (stack_count + 1 > stack_cap) {
                    size_t nc = next_capacity(stack_cap);
                    char **tmp = realloc(stack, nc * sizeof(char *));
                    if (!tmp) { free(norm); goto err; }
                    stack = tmp; stack_cap = nc;
                }
                stack[stack_count++] = norm;
            } else {
                free(norm);
            }
        } else if (S_ISREG(st.st_mode)) {
            int is_ig = ignore_match(ignore, norm, 0);
            if (!is_ig) {
                if (count + 1 > cap) {
                    size_t nc = next_capacity(cap);
                    FileRecord *tmp = realloc(list, nc * sizeof(FileRecord));
                    if (!tmp) { free(norm); goto err; }
                    list = tmp; cap = nc;
                }
                list[count].path = norm;
                list[count].hash = 0; 
                list[count].size = (uint64_t)st.st_size;
                list[count].mtime = (uint64_t)st.st_mtime;
                list[count].dev = (uint64_t)st.st_dev;
                list[count].ino = (uint64_t)st.st_ino;
                count++;
            } else {
                free(norm);
            }
        } else {
            free(norm);
        }
    }

    
    while (stack_count > 0) {
        char *dirpath = stack[--stack_count];
        DIR *d = opendir(dirpath);
        if (!d) {
            free(dirpath);
            continue;
        }
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
            
            size_t need = strlen(dirpath) + 1 + strlen(de->d_name) + 1;
            char *child = malloc(need);
            if (!child) {
                closedir(d);
                free(dirpath);
                goto err;
            }
            child[0] = '\0';
            strlcpy(child, dirpath, need);
            
            if (strcmp(child, ".") == 0) child[0] = '\0';
            if (child[0] != '\0') {
                strlcat(child, "/", need);
            }
            strlcat(child, de->d_name, need);

            struct stat st;
            if (lstat(child, &st) < 0) {
                free(child);
                continue;
            }
            int is_dir = S_ISDIR(st.st_mode);
            
            char *rel = normalize_relpath(child);
            if (!rel) { free(child); closedir(d); goto err; }

            if (ignore_match(ignore, rel, is_dir)) {
                free(rel);
                free(child);
                continue;
            }

            if (is_dir) {
                
                if (stack_count + 1 > stack_cap) {
                    size_t nc = next_capacity(stack_cap);
                    char **tmp = realloc(stack, nc * sizeof(char *));
                    if (!tmp) { free(rel); free(child); closedir(d); goto err; }
                    stack = tmp; stack_cap = nc;
                }
                stack[stack_count++] = child; 
                free(rel);
            } else if (S_ISREG(st.st_mode)) {
                if (count + 1 > cap) {
                    size_t nc = next_capacity(cap);
                    FileRecord *tmp = realloc(list, nc * sizeof(FileRecord));
                    if (!tmp) { free(rel); free(child); closedir(d); goto err; }
                    list = tmp; cap = nc;
                }
                list[count].path = rel; 
                list[count].hash = 0;
                list[count].size = (uint64_t)st.st_size;
                list[count].mtime = (uint64_t)st.st_mtime;
                list[count].dev = (uint64_t)st.st_dev;
                list[count].ino = (uint64_t)st.st_ino;
                count++;
                free(child);
            } else {
                free(rel);
                free(child);
            }
        }
        closedir(d);
        free(dirpath);
    }

    free(stack);
    *out_list = list;
    *out_count = count;
    return 0;

err:
    for (size_t i = 0; i < count; i++) free(list[i].path);
    free(list);
    if (stack) {
        for (size_t i = 0; i < stack_count; i++) free(stack[i]);
        free(stack);
    }
    return -1;
}


static int cmp_record_path(const void *a, const void *b) {
    const FileRecord *ra = a;
    const FileRecord *rb = b;
    return strcmp(ra->path, rb->path);
}


static ssize_t find_record_idx(FileRecord *records, size_t count, const char *path) {
    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        int c = strcmp(records[mid].path, path);
        if (c == 0) return (ssize_t)mid;
        if (c < 0) lo = mid + 1;
        else hi = mid;
    }
    return -1;
}


static int cmd_init(void) {
    struct stat st;
    if (stat(INDEX_DIR, &st) == 0 && S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Already initialized.\n");
        return EXIT_ALREADY_INITIALIZED;
    }
    if (mkdir(INDEX_DIR, 0755) < 0) {
        perror("mkdir");
        return EXIT_FAIL;
    }

    
    if (store_save(INDEX_FILE, NULL, 0) != 0) {
        fprintf(stderr, "Failed to create index file.\n");
        return EXIT_FAIL;
    }

    
    FILE *f = fopen(IGNORE_FILE, "r");
    if (!f) {
        f = fopen(IGNORE_FILE, "w");
        if (!f) {
            perror("fopen");
            return EXIT_FAIL;
        }
        fprintf(f, ".fdiff\n");
        fclose(f);
    } else {
        fclose(f);
    }

    printf("Initialized empty fdiff\n");
    return EXIT_OK;
}


static int cmd_add(int argc, char *argv[]) {
    struct stat st;
    if (stat(INDEX_FILE, &st) != 0) {
        fprintf(stderr, "Not initialized.\n");
        return EXIT_FAIL;
    }

    IgnoreList ignore = {0};
    if (ignore_load(IGNORE_FILE, &ignore) != 0) {
        fprintf(stderr, "Warning: failed to load ignore file. Continuing.\n");
    }

    FileRecord *old_records = NULL;
    size_t old_count = 0;
    if (store_load(INDEX_FILE, &old_records, &old_count) != 0) {
        
        old_records = NULL;
        old_count = 0;
    }

    
    int nstart = argc - 2;
    char **start_paths = calloc((size_t)nstart, sizeof(char *));
    for (int i = 0; i < nstart; i++) start_paths[i] = argv[2 + i];

    FileRecord *new_records = NULL;
    size_t new_count = 0;
    int rc = collect_files(start_paths, nstart, &ignore, &new_records, &new_count);
    free(start_paths);
    if (rc != 0) {
        ignore_free(&ignore);
        store_free(old_records, old_count);
        return EXIT_FAIL;
    }

    
    if (old_records && old_count > 1) qsort(old_records, old_count, sizeof(FileRecord), cmp_record_path);
    if (new_records && new_count > 1) qsort(new_records, new_count, sizeof(FileRecord), cmp_record_path);

    int added_count = 0;

    for (size_t i = 0; i < new_count; i++) {
        ssize_t idx = -1;
        if (old_records) idx = find_record_idx(old_records, old_count, new_records[i].path);
        if (idx < 0) {
            
            uint64_t h = 0;
            if (new_records[i].size == 0) {
                h = 0;
            } else {
                if (compute_file_hash(new_records[i].path, &h, NULL) != 0) {
                    
                    fprintf(stderr, "Failed to hash %s\n", new_records[i].path);
                    ignore_free(&ignore);
                    store_free(old_records, old_count);
                    store_free(new_records, new_count);
                    return EXIT_FAIL;
                }
            }
            new_records[i].hash = h;
            added_count++;
        } else {
            
            FileRecord *old = &old_records[idx];
            if (old->dev == new_records[i].dev && old->ino == new_records[i].ino) {
                new_records[i].hash = old->hash;
                
            } else if (old->size == new_records[i].size && old->mtime == new_records[i].mtime) {
                new_records[i].hash = old->hash;
            } else {
                
                uint64_t h = 0;
                if (new_records[i].size == 0) {
                    h = 0;
                } else {
                    if (compute_file_hash(new_records[i].path, &h, NULL) != 0) {
                        fprintf(stderr, "Failed to hash %s\n", new_records[i].path);
                        ignore_free(&ignore);
                        store_free(old_records, old_count);
                        store_free(new_records, new_count);
                        return EXIT_FAIL;
                    }
                }
                new_records[i].hash = h;
                if (h != old->hash) added_count++;
            }
        }
    }

    if (added_count == 0) {
        ignore_free(&ignore);
        store_free(old_records, old_count);
        store_free(new_records, new_count);
        return EXIT_ALREADY_ADDED;
    }

    
    if (store_save(INDEX_FILE, new_records, new_count) != 0) {
        ignore_free(&ignore);
        store_free(old_records, old_count);
        store_free(new_records, new_count);
        fprintf(stderr, "Failed to save index\n");
        return EXIT_FAIL;
    }

    ignore_free(&ignore);
    store_free(old_records, old_count);
    store_free(new_records, new_count);

    return EXIT_OK;
}


static int cmd_status(void) {
    struct stat st;
    if (stat(INDEX_FILE, &st) != 0) {
        fprintf(stderr, "Not initialized.\n");
        return EXIT_FAIL;
    }

    IgnoreList ignore = {0};
    if (ignore_load(IGNORE_FILE, &ignore) != 0) {
        fprintf(stderr, "Warning: failed to load ignore file. Continuing.\n");
    }

    FileRecord *old_records = NULL;
    size_t old_count = 0;
    if (store_load(INDEX_FILE, &old_records, &old_count) != 0) {
        fprintf(stderr, "Failed to load index.\n");
        ignore_free(&ignore);
        return EXIT_FAIL;
    }

    
    char *start = ".";
    char *starts[1] = { start };
    FileRecord *new_records = NULL;
    size_t new_count = 0;
    if (collect_files(starts, 1, &ignore, &new_records, &new_count) != 0) {
        ignore_free(&ignore);
        store_free(old_records, old_count);
        return EXIT_FAIL;
    }

    if (old_count > 1) qsort(old_records, old_count, sizeof(FileRecord), cmp_record_path);
    if (new_count > 1) qsort(new_records, new_count, sizeof(FileRecord), cmp_record_path);

    int changed = 0;

    for (size_t i = 0; i < new_count; i++) {
        ssize_t idx = -1;
        if (old_records) idx = find_record_idx(old_records, old_count, new_records[i].path);
        if (idx < 0) {
            printf("Untracked: %s\n", new_records[i].path);
            changed = 1;
        } else {
            FileRecord *old = &old_records[idx];
            if (old->dev == new_records[i].dev && old->ino == new_records[i].ino) {
                
            } else if (old->size == new_records[i].size && old->mtime == new_records[i].mtime) {
                
            } else {
                
                uint64_t h = 0;
                if (new_records[i].size == 0) {
                    h = 0;
                } else {
                    if (compute_file_hash(new_records[i].path, &h, NULL) != 0) {
                        fprintf(stderr, "Failed to hash %s\n", new_records[i].path);
                        ignore_free(&ignore);
                        store_free(old_records, old_count);
                        store_free(new_records, new_count);
                        return EXIT_FAIL;
                    }
                }
                if (h != old->hash) {
                    printf("Modified: %s\n", new_records[i].path);
                    changed = 1;
                }
            }
        }
    }

    
    for (size_t i = 0; i < old_count; i++) {
        ssize_t idx = find_record_idx(new_records, new_count, old_records[i].path);
        if (idx == -1) {
            printf("Deleted: %s\n", old_records[i].path);
            changed = 1;
        }
    }

    ignore_free(&ignore);
    store_free(old_records, old_count);
    store_free(new_records, new_count);

    if (changed) return EXIT_DIFF_FOUND;
    return EXIT_OK;
}

static void print_help(void) {
    printf("fdiff - simple file difference tracker\n\n");
    printf("Usage:\n");
    printf("  fdiff init             Initialize a new fdiff\n");
    printf("  fdiff add <path>...    Add file(s) or directories to tracking\n");
    printf("  fdiff status           Show status of tracked vs current files\n");
    printf("  fdiff help             Show this help message\n\n");
    printf("Notes:\n");
    printf("  - Ignores files matching patterns in .fdiffignore\n");
    printf("  - Default ignore contains: .fdiff\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help();
        return EXIT_FAIL;
    }

    if (strcmp(argv[1], "init") == 0) {
        return cmd_init();
    } else if (strcmp(argv[1], "add") == 0) {
        if (argc < 3) {
            fprintf(stderr, "No path specified to add.\n");
            return EXIT_FAIL;
        }
        return cmd_add(argc, argv);
    } else if (strcmp(argv[1], "status") == 0) {
        return cmd_status();
    } else if (strcmp(argv[1], "help") == 0) {
        print_help();
        return EXIT_OK;
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_help();
        return EXIT_FAIL;
    }
}

