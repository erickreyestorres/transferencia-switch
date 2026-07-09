#include "transfer_switch/application/probe_remote_file.h"

TsStatus ts_probe_remote_file(
    const TsRemoteFilePort *port,
    const TsFileCatalog *catalog,
    size_t file_index,
    TsFileProbe *probe
) {
    size_t received = 0;
    TsStatus status;

    if (port == NULL || port->read_range == NULL || catalog == NULL || probe == NULL) {
        return TS_INVALID_ARGUMENT;
    }

    status = ts_catalog_copy_name(
        catalog,
        file_index,
        probe->name,
        sizeof(probe->name)
    );
    if (status != TS_OK) {
        return status;
    }

    status = port->read_range(
        port->context,
        probe->name,
        0,
        1,
        &probe->first_byte,
        sizeof(probe->first_byte),
        &received
    );
    if (status != TS_OK) {
        return status;
    }
    return received == 1 ? TS_OK : TS_PROTOCOL_ERROR;
}
