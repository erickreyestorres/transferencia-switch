#ifndef TRANSFER_SWITCH_PORTS_REMOTE_FILE_H
#define TRANSFER_SWITCH_PORTS_REMOTE_FILE_H

#include <stddef.h>
#include <stdint.h>

#include "transfer_switch/domain/status.h"

typedef struct {
    void *context;
    TsStatus (*read_range)(
        void *context,
        const char *name,
        uint64_t offset,
        size_t size,
        void *buffer,
        size_t capacity,
        size_t *received
    );
} TsRemoteFilePort;

#endif
