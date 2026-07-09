#ifndef TRANSFER_SWITCH_ADAPTERS_DBI_CATALOG_H
#define TRANSFER_SWITCH_ADAPTERS_DBI_CATALOG_H

#include <stdbool.h>

#include "transfer_switch/ports/byte_transport.h"
#include "transfer_switch/ports/remote_catalog.h"
#include "transfer_switch/ports/remote_file.h"

typedef struct {
    TsByteTransport *transport;
    bool connected;
} TsDbiCatalogAdapter;

void ts_dbi_catalog_adapter_init(
    TsDbiCatalogAdapter *adapter,
    TsByteTransport *transport,
    TsRemoteCatalogPort *port
);
void ts_dbi_file_adapter_init(TsDbiCatalogAdapter *adapter, TsRemoteFilePort *port);
TsStatus ts_dbi_catalog_adapter_close(TsDbiCatalogAdapter *adapter);

#endif
