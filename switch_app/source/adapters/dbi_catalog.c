#include "transfer_switch/adapters/dbi_catalog.h"

#include <stdint.h>
#include <string.h>

#include "transfer_switch/domain/catalog.h"

enum {
    DBI_HEADER_SIZE = 16,
    DBI_FILE_REQUEST_SIZE = 16,
    DBI_REQUEST = 0,
    DBI_RESPONSE = 1,
    DBI_ACK = 2,
    DBI_EXIT = 0,
    DBI_FILE_RANGE = 2,
    DBI_LIST = 3
};

static void write_u32_le(unsigned char *destination, uint32_t value) {
    destination[0] = (unsigned char)(value & 0xffu);
    destination[1] = (unsigned char)((value >> 8) & 0xffu);
    destination[2] = (unsigned char)((value >> 16) & 0xffu);
    destination[3] = (unsigned char)((value >> 24) & 0xffu);
}

static void write_u64_le(unsigned char *destination, uint64_t value) {
    for (size_t index = 0; index < 8; ++index) {
        destination[index] = (unsigned char)((value >> (index * 8)) & 0xffu);
    }
}

static uint32_t read_u32_le(const unsigned char *source) {
    return (uint32_t)source[0]
        | ((uint32_t)source[1] << 8)
        | ((uint32_t)source[2] << 16)
        | ((uint32_t)source[3] << 24);
}

static void make_header(unsigned char *header, uint32_t type, uint32_t command, uint32_t size) {
    memcpy(header, "DBI0", 4);
    write_u32_le(header + 4, type);
    write_u32_le(header + 8, command);
    write_u32_le(header + 12, size);
}

static TsStatus read_exact(TsByteTransport *transport, void *buffer, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        size_t transferred = 0;
        TsStatus status = transport->read(
            transport->context,
            (unsigned char *)buffer + offset,
            size - offset,
            &transferred
        );
        if (status != TS_OK) {
            return status;
        }
        if (transferred == 0 || transferred > size - offset) {
            return TS_TRANSPORT_ERROR;
        }
        offset += transferred;
    }
    return TS_OK;
}

static TsStatus write_exact(TsByteTransport *transport, const void *buffer, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        size_t transferred = 0;
        TsStatus status = transport->write(
            transport->context,
            (const unsigned char *)buffer + offset,
            size - offset,
            &transferred
        );
        if (status != TS_OK) {
            return status;
        }
        if (transferred == 0 || transferred > size - offset) {
            return TS_TRANSPORT_ERROR;
        }
        offset += transferred;
    }
    return TS_OK;
}

static TsStatus validate_header(
    const unsigned char *header,
    uint32_t expected_type,
    uint32_t expected_command
) {
    if (memcmp(header, "DBI0", 4) != 0) {
        return TS_PROTOCOL_ERROR;
    }
    if (read_u32_le(header + 4) != expected_type || read_u32_le(header + 8) != expected_command) {
        return TS_PROTOCOL_ERROR;
    }
    return TS_OK;
}

static void disconnect(TsDbiCatalogAdapter *adapter) {
    if (adapter != NULL && adapter->connected) {
        adapter->transport->close(adapter->transport->context);
        adapter->connected = false;
    }
}

static TsStatus ensure_connected(TsDbiCatalogAdapter *adapter) {
    TsByteTransport *transport;
    TsStatus status;

    if (adapter == NULL || adapter->transport == NULL) {
        return TS_INVALID_ARGUMENT;
    }
    if (adapter->connected) {
        return TS_OK;
    }
    transport = adapter->transport;
    if (transport->open == NULL || transport->close == NULL || transport->read == NULL || transport->write == NULL) {
        return TS_INVALID_ARGUMENT;
    }
    status = transport->open(transport->context);
    if (status == TS_OK) {
        adapter->connected = true;
    }
    return status;
}

static TsStatus fetch_catalog(void *context, char *buffer, size_t capacity, size_t *received) {
    TsDbiCatalogAdapter *adapter = context;
    TsByteTransport *transport;
    unsigned char header[DBI_HEADER_SIZE];
    uint32_t payload_size;
    TsStatus status;

    if (adapter == NULL || buffer == NULL || capacity == 0 || received == NULL) {
        return TS_INVALID_ARGUMENT;
    }
    status = ensure_connected(adapter);
    if (status != TS_OK) {
        return status;
    }
    transport = adapter->transport;
    *received = 0;

    make_header(header, DBI_REQUEST, DBI_LIST, 0);
    status = write_exact(transport, header, sizeof(header));
    if (status != TS_OK) {
        goto fail;
    }
    status = read_exact(transport, header, sizeof(header));
    if (status != TS_OK) {
        goto fail;
    }
    status = validate_header(header, DBI_RESPONSE, DBI_LIST);
    if (status != TS_OK) {
        goto fail;
    }

    payload_size = read_u32_le(header + 12);
    if ((size_t)payload_size >= capacity) {
        status = TS_BUFFER_TOO_SMALL;
        goto fail;
    }
    if (payload_size > 0) {
        make_header(header, DBI_ACK, DBI_LIST, 0);
        status = write_exact(transport, header, sizeof(header));
        if (status != TS_OK) {
            goto fail;
        }
        status = read_exact(transport, buffer, payload_size);
        if (status != TS_OK) {
            goto fail;
        }
    }
    *received = payload_size;
    return TS_OK;

fail:
    disconnect(adapter);
    return status;
}

static TsStatus read_file_range(
    void *context,
    const char *name,
    uint64_t offset,
    size_t size,
    void *buffer,
    size_t capacity,
    size_t *received
) {
    TsDbiCatalogAdapter *adapter = context;
    TsByteTransport *transport;
    unsigned char header[DBI_HEADER_SIZE];
    unsigned char request[DBI_FILE_REQUEST_SIZE + TS_FILE_NAME_CAPACITY];
    const size_t name_length = name == NULL ? 0 : strlen(name);
    const size_t request_size = DBI_FILE_REQUEST_SIZE + name_length;
    TsStatus status;

    if (adapter == NULL || name == NULL || name_length == 0 || name_length >= TS_FILE_NAME_CAPACITY
        || buffer == NULL || received == NULL || size == 0 || size > capacity || size > UINT32_MAX) {
        return TS_INVALID_ARGUMENT;
    }
    status = ensure_connected(adapter);
    if (status != TS_OK) {
        return status;
    }
    transport = adapter->transport;
    *received = 0;

    make_header(header, DBI_REQUEST, DBI_FILE_RANGE, (uint32_t)request_size);
    status = write_exact(transport, header, sizeof(header));
    if (status != TS_OK) {
        goto fail;
    }
    status = read_exact(transport, header, sizeof(header));
    if (status != TS_OK) {
        goto fail;
    }
    status = validate_header(header, DBI_ACK, DBI_FILE_RANGE);
    if (status != TS_OK) {
        goto fail;
    }

    write_u32_le(request, (uint32_t)size);
    write_u64_le(request + 4, offset);
    write_u32_le(request + 12, (uint32_t)name_length);
    memcpy(request + DBI_FILE_REQUEST_SIZE, name, name_length);
    status = write_exact(transport, request, request_size);
    if (status != TS_OK) {
        goto fail;
    }

    status = read_exact(transport, header, sizeof(header));
    if (status != TS_OK) {
        goto fail;
    }
    status = validate_header(header, DBI_RESPONSE, DBI_FILE_RANGE);
    if (status != TS_OK || read_u32_le(header + 12) != (uint32_t)size) {
        status = TS_PROTOCOL_ERROR;
        goto fail;
    }

    make_header(header, DBI_ACK, DBI_FILE_RANGE, 0);
    status = write_exact(transport, header, sizeof(header));
    if (status != TS_OK) {
        goto fail;
    }
    status = read_exact(transport, buffer, size);
    if (status != TS_OK) {
        goto fail;
    }
    *received = size;
    return TS_OK;

fail:
    disconnect(adapter);
    return status;
}

void ts_dbi_catalog_adapter_init(
    TsDbiCatalogAdapter *adapter,
    TsByteTransport *transport,
    TsRemoteCatalogPort *port
) {
    if (adapter == NULL || port == NULL) {
        return;
    }
    adapter->transport = transport;
    adapter->connected = false;
    port->context = adapter;
    port->fetch = fetch_catalog;
}

void ts_dbi_file_adapter_init(TsDbiCatalogAdapter *adapter, TsRemoteFilePort *port) {
    if (adapter == NULL || port == NULL) {
        return;
    }
    port->context = adapter;
    port->read_range = read_file_range;
}

TsStatus ts_dbi_catalog_adapter_close(TsDbiCatalogAdapter *adapter) {
    unsigned char header[DBI_HEADER_SIZE];
    TsStatus status;

    if (adapter == NULL || adapter->transport == NULL) {
        return TS_INVALID_ARGUMENT;
    }
    if (!adapter->connected) {
        return TS_OK;
    }

    make_header(header, DBI_REQUEST, DBI_EXIT, 0);
    status = write_exact(adapter->transport, header, sizeof(header));
    if (status == TS_OK) {
        status = read_exact(adapter->transport, header, sizeof(header));
    }
    if (status == TS_OK) {
        status = validate_header(header, DBI_RESPONSE, DBI_EXIT);
    }
    disconnect(adapter);
    return status;
}
