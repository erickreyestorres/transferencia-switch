#include "transfer_switch/application/list_remote_files.h"

TsStatus ts_list_remote_files(
    const TsRemoteCatalogPort *port,
    char *buffer,
    size_t capacity,
    TsFileCatalog *catalog
) {
    size_t received = 0;
    TsStatus status;

    if (port == NULL || port->fetch == NULL || buffer == NULL || capacity == 0 || catalog == NULL) {
        return TS_INVALID_ARGUMENT;
    }

    status = port->fetch(port->context, buffer, capacity, &received);
    if (status != TS_OK) {
        return status;
    }
    if (received >= capacity) {
        return TS_BUFFER_TOO_SMALL;
    }

    buffer[received] = '\0';
    return ts_catalog_parse(catalog, buffer, received);
}
