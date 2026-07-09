#ifndef TRANSFER_SWITCH_ADAPTERS_LIBNX_USB_TRANSPORT_H
#define TRANSFER_SWITCH_ADAPTERS_LIBNX_USB_TRANSPORT_H

#include <stdbool.h>

#include "transfer_switch/ports/byte_transport.h"

typedef struct {
    bool initialized;
} TsLibnxUsbTransport;

void ts_libnx_usb_transport_init(TsLibnxUsbTransport *adapter, TsByteTransport *transport);

#endif
