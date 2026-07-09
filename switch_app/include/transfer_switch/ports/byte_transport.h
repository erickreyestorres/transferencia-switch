#ifndef TRANSFER_SWITCH_PORTS_BYTE_TRANSPORT_H
#define TRANSFER_SWITCH_PORTS_BYTE_TRANSPORT_H

#include <stddef.h>

#include "transfer_switch/domain/status.h"

typedef struct {
    void *context;
    TsStatus (*open)(void *context);
    void (*close)(void *context);
    TsStatus (*read)(void *context, void *buffer, size_t size, size_t *transferred);
    TsStatus (*write)(void *context, const void *buffer, size_t size, size_t *transferred);
} TsByteTransport;

#endif
