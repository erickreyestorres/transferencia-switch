#ifndef TRANSFER_SWITCH_DOMAIN_STATUS_H
#define TRANSFER_SWITCH_DOMAIN_STATUS_H

typedef enum {
    TS_OK = 0,
    TS_INVALID_ARGUMENT,
    TS_INVALID_CATALOG,
    TS_BUFFER_TOO_SMALL,
    TS_TRANSPORT_ERROR,
    TS_PROTOCOL_ERROR
} TsStatus;

const char *ts_status_message(TsStatus status);

#endif
