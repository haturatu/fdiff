#ifndef FDIFF_STORE_H
#define FDIFF_STORE_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *path;      
    uint64_t hash;   
    uint64_t size;
    uint64_t mtime;
    uint64_t dev;
    uint64_t ino;
} FileRecord;

int store_load(const char *path, FileRecord **records, size_t *count);
int store_save(const char *path, FileRecord *records, size_t count);
void store_free(FileRecord *records, size_t count);

#endif

