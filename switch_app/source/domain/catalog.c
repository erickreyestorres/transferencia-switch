#include "transfer_switch/domain/catalog.h"

#include <stdbool.h>

TsStatus ts_catalog_parse(TsFileCatalog *catalog, char *data, size_t length) {
    size_t file_count = 0;
    size_t name_length = 0;

    if (catalog == NULL || data == NULL) {
        return TS_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < length; ++index) {
        const unsigned char value = (unsigned char)data[index];
        if (value == '\0') {
            return TS_INVALID_CATALOG;
        }
        if (value == '\n') {
            if (name_length > 0) {
                ++file_count;
                name_length = 0;
            }
            continue;
        }
        if (value < 0x20 && value != '\r' && value != '\t') {
            return TS_INVALID_CATALOG;
        }
        ++name_length;
    }

    if (name_length > 0) {
        ++file_count;
    }

    catalog->data = data;
    catalog->length = length;
    catalog->file_count = file_count;
    return TS_OK;
}

TsStatus ts_catalog_copy_name(
    const TsFileCatalog *catalog,
    size_t index,
    char *destination,
    size_t capacity
) {
    size_t current_index = 0;
    size_t start = 0;

    if (catalog == NULL || catalog->data == NULL || destination == NULL || capacity == 0) {
        return TS_INVALID_ARGUMENT;
    }

    for (size_t position = 0; position <= catalog->length; ++position) {
        const bool at_end = position == catalog->length;
        if (!at_end && catalog->data[position] != '\n') {
            continue;
        }

        size_t name_length = position - start;
        if (name_length > 0 && catalog->data[start + name_length - 1] == '\r') {
            --name_length;
        }
        if (name_length > 0) {
            if (current_index == index) {
                if (name_length >= capacity) {
                    return TS_BUFFER_TOO_SMALL;
                }
                for (size_t offset = 0; offset < name_length; ++offset) {
                    destination[offset] = catalog->data[start + offset];
                }
                destination[name_length] = '\0';
                return TS_OK;
            }
            ++current_index;
        }
        start = position + 1;
    }

    return TS_INVALID_ARGUMENT;
}
