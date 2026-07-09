#ifndef TRANSFER_SWITCH_APPLICATION_PROBE_REMOTE_FILE_H
#define TRANSFER_SWITCH_APPLICATION_PROBE_REMOTE_FILE_H

#include <stddef.h>

#include "transfer_switch/domain/catalog.h"
#include "transfer_switch/ports/remote_file.h"

typedef struct {
    char name[TS_FILE_NAME_CAPACITY];
    unsigned char first_byte;
} TsFileProbe;

TsStatus ts_probe_remote_file(
    const TsRemoteFilePort *port,
    const TsFileCatalog *catalog,
    size_t file_index,
    TsFileProbe *probe
);

#endif
