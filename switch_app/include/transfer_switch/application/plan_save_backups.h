#ifndef TRANSFER_SWITCH_APPLICATION_PLAN_SAVE_BACKUPS_H
#define TRANSFER_SWITCH_APPLICATION_PLAN_SAVE_BACKUPS_H

#include <stddef.h>
#include <stdint.h>

#include "transfer_switch/domain/save_backup_plan.h"
#include "transfer_switch/domain/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t application_id;
    uint64_t user_id_low;
    uint64_t user_id_high;
} TsSaveBackupCandidate;

typedef struct {
    size_t requested_count;
    size_t planned_count;
    size_t failed_count;
    TsStatus first_error;
} TsSaveBackupPlanningSummary;

TsStatus ts_plan_save_backups(
    const TsSaveBackupCandidate* candidates,
    size_t candidate_count,
    const char* date,
    TsSaveBackupPlan* plans,
    size_t plan_capacity,
    TsSaveBackupPlanningSummary* summary
);

#ifdef __cplusplus
}
#endif

#endif
