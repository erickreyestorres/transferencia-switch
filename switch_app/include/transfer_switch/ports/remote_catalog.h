#ifndef TRANSFER_SWITCH_PORTS_REMOTE_CATALOG_H
#define TRANSFER_SWITCH_PORTS_REMOTE_CATALOG_H

#include <stddef.h>

#include "transfer_switch/domain/status.h"

typedef struct {
    void *context;
    TsStatus (*fetch)(void *context, char *buffer, size_t capacity, size_t *received);
} TsRemoteCatalogPort;

#endif
