#include "transfer_switch/adapters/libnx_usb_transport.h"

#include <switch.h>

static TsStatus open_usb(void *context) {
    TsLibnxUsbTransport *adapter = context;
    Result result;

    if (adapter == NULL) {
        return TS_INVALID_ARGUMENT;
    }
    result = usbCommsInitialize();
    if (R_FAILED(result)) {
        return TS_TRANSPORT_ERROR;
    }
    usbCommsSetErrorHandling(false);
    adapter->initialized = true;
    return TS_OK;
}

static void close_usb(void *context) {
    TsLibnxUsbTransport *adapter = context;
    if (adapter != NULL && adapter->initialized) {
        usbCommsExit();
        adapter->initialized = false;
    }
}

static TsStatus read_usb(void *context, void *buffer, size_t size, size_t *transferred) {
    TsLibnxUsbTransport *adapter = context;
    if (adapter == NULL || !adapter->initialized || buffer == NULL || transferred == NULL) {
        return TS_INVALID_ARGUMENT;
    }
    *transferred = usbCommsRead(buffer, size);
    return size == 0 || *transferred > 0 ? TS_OK : TS_TRANSPORT_ERROR;
}

static TsStatus write_usb(void *context, const void *buffer, size_t size, size_t *transferred) {
    TsLibnxUsbTransport *adapter = context;
    if (adapter == NULL || !adapter->initialized || buffer == NULL || transferred == NULL) {
        return TS_INVALID_ARGUMENT;
    }
    *transferred = usbCommsWrite(buffer, size);
    return size == 0 || *transferred > 0 ? TS_OK : TS_TRANSPORT_ERROR;
}

void ts_libnx_usb_transport_init(TsLibnxUsbTransport *adapter, TsByteTransport *transport) {
    if (adapter == NULL || transport == NULL) {
        return;
    }
    adapter->initialized = false;
    transport->context = adapter;
    transport->open = open_usb;
    transport->close = close_usb;
    transport->read = read_usb;
    transport->write = write_usb;
}
