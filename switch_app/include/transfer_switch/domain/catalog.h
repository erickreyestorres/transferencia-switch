#ifndef TRANSFER_SWITCH_DOMAIN_CATALOG_H
#define TRANSFER_SWITCH_DOMAIN_CATALOG_H

#include <stddef.h>

#include "transfer_switch/domain/status.h"

#define TS_FILE_NAME_CAPACITY 512u

typedef struct {
    char *data;
    size_t length;
    size_t file_count;
} TsFileCatalog;

TsStatus ts_catalog_parse(TsFileCatalog *catalog, char *data, size_t length);
TsStatus ts_catalog_copy_name(
    const TsFileCatalog *catalog,
    size_t index,
    char *destination,
    size_t capacity
);

#endif
