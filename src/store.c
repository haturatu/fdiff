#define _POSIX_C_SOURCE 200809L
#include "store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <inttypes.h>

static int write_all(int fd, const void *buf, size_t count) {
    const unsigned char *p = buf;
    size_t off = 0;
    while (off < count) {
        ssize_t w = write(fd, p + off, count - off);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (size_t)w;
    }
    return 0;
}

int store_save(const char *path, FileRecord *records, size_t count) {
    
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;

    uint64_t cc = (uint64_t)count;
    if (write_all(fd, &cc, sizeof(uint64_t)) != 0) goto err;

    for (size_t i = 0; i < count; i++) {
        uint64_t path_len = (uint64_t)strlen(records[i].path);
        if (write_all(fd, &path_len, sizeof(uint64_t)) != 0) goto err;
        if (write_all(fd, records[i].path, path_len) != 0) goto err;
        if (write_all(fd, &records[i].hash, sizeof(uint64_t)) != 0) goto err;
        if (write_all(fd, &records[i].size, sizeof(uint64_t)) != 0) goto err;
        if (write_all(fd, &records[i].mtime, sizeof(uint64_t)) != 0) goto err;
        if (write_all(fd, &records[i].dev, sizeof(uint64_t)) != 0) goto err;
        if (write_all(fd, &records[i].ino, sizeof(uint64_t)) != 0) goto err;
    }

    if (fsync(fd) != 0) goto err;
    if (close(fd) != 0) return -1;

    if (rename(tmp, path) != 0) return -1;
    return 0;

err:
    close(fd);
    unlink(tmp);
    return -1;
}

int store_load(const char *path, FileRecord **records, size_t *count) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    uint64_t rec_count;
    if (fread(&rec_count, sizeof(uint64_t), 1, f) != 1) {
        fclose(f);
        return -1;
    }

    FileRecord *recs = calloc((size_t)rec_count, sizeof(FileRecord));
    if (!recs) {
        fclose(f);
        return -1;
    }

    for (uint64_t i = 0; i < rec_count; i++) {
        uint64_t path_len;
        if (fread(&path_len, sizeof(uint64_t), 1, f) != 1) goto err;
        char *buf = malloc((size_t)path_len + 1);
        if (!buf) goto err;
        if (fread(buf, 1, (size_t)path_len, f) != path_len) {
            free(buf);
            goto err;
        }
        buf[path_len] = '\0';
        recs[i].path = buf;

        if (fread(&recs[i].hash, sizeof(uint64_t), 1, f) != 1) goto err;
        if (fread(&recs[i].size, sizeof(uint64_t), 1, f) != 1) goto err;
        if (fread(&recs[i].mtime, sizeof(uint64_t), 1, f) != 1) goto err;
        if (fread(&recs[i].dev, sizeof(uint64_t), 1, f) != 1) goto err;
        if (fread(&recs[i].ino, sizeof(uint64_t), 1, f) != 1) goto err;
    }

    fclose(f);
    *records = recs;
    *count = (size_t)rec_count;
    return 0;

err:
    for (uint64_t j = 0; j < rec_count; j++) {
        free(recs[j].path);
    }
    free(recs);
    fclose(f);
    return -1;
}

void store_free(FileRecord *records, size_t count) {
    if (!records) return;
    for (size_t i = 0; i < count; i++) {
        free(records[i].path);
    }
    free(records);
}

