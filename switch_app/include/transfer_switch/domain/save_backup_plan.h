#ifndef TRANSFER_SWITCH_DOMAIN_SAVE_BACKUP_PLAN_H
#define TRANSFER_SWITCH_DOMAIN_SAVE_BACKUP_PLAN_H

#include <stddef.h>
#include <stdint.h>

#include "transfer_switch/domain/status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TS_SAVE_BACKUP_ROOT "sdmc:/switch/transferencia-switch/backups/saves"
#define TS_SAVE_BACKUP_PATH_MAX 256
#define TS_SAVE_BACKUP_MANIFEST_MAX 512

typedef struct {
    uint64_t application_id;
    uint64_t user_id_low;
    uint64_t user_id_high;
    char date[11];
    char folder_name[80];
    char backup_path[TS_SAVE_BACKUP_PATH_MAX];
    char files_path[TS_SAVE_BACKUP_PATH_MAX];
    char manifest_path[TS_SAVE_BACKUP_PATH_MAX];
} TsSaveBackupPlan;

TsStatus ts_save_backup_plan_create(
    TsSaveBackupPlan* plan,
    uint64_t application_id,
    uint64_t user_id_low,
    uint64_t user_id_high,
    const char* date
);

TsStatus ts_save_backup_manifest_json(
    const TsSaveBackupPlan* plan,
    char* buffer,
    size_t capacity
);

#ifdef __cplusplus
}
#endif

#endif
