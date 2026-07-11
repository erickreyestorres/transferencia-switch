#ifndef TRANSFER_SWITCH_DOMAIN_TRANSFER_RESULT_CATEGORY_H
#define TRANSFER_SWITCH_DOMAIN_TRANSFER_RESULT_CATEGORY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TS_TRANSFER_CATEGORY_OK = 0,
    TS_TRANSFER_CATEGORY_ALREADY_INSTALLED,
    TS_TRANSFER_CATEGORY_XCI_INVALID,
    TS_TRANSFER_CATEGORY_CNMT_OPEN_FAILED,
    TS_TRANSFER_CATEGORY_XCI_NCA_TOO_SMALL,
    TS_TRANSFER_CATEGORY_CANCELLED,
    TS_TRANSFER_CATEGORY_CONNECTION_OR_SLEEP,
    TS_TRANSFER_CATEGORY_UNKNOWN_ERROR,
} TsTransferResultCategory;

TsTransferResultCategory ts_transfer_result_category_from_detail(
    const char *detail,
    int succeeded,
    int cancelled
);

const char *ts_transfer_result_category_title(TsTransferResultCategory category);
const char *ts_transfer_result_category_advice(TsTransferResultCategory category);

#ifdef __cplusplus
}
#endif

#endif
