#ifndef TRANSFER_SWITCH_APPLICATION_LIST_REMOTE_FILES_H
#define TRANSFER_SWITCH_APPLICATION_LIST_REMOTE_FILES_H

#include <stddef.h>

#include "transfer_switch/domain/catalog.h"
#include "transfer_switch/ports/remote_catalog.h"

TsStatus ts_list_remote_files(
    const TsRemoteCatalogPort *port,
    char *buffer,
    size_t capacity,
    TsFileCatalog *catalog
);

#endif
