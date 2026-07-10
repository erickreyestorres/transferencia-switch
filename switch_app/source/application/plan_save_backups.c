#include "transfer_switch/application/plan_save_backups.h"

#include <string.h>

TsStatus ts_plan_save_backups(
    const TsSaveBackupCandidate* candidates,
    size_t candidate_count,
    const char* date,
    TsSaveBackupPlan* plans,
    size_t plan_capacity,
    TsSaveBackupPlanningSummary* summary
) {
    if (summary == NULL) {
        return TS_INVALID_ARGUMENT;
    }
    memset(summary, 0, sizeof(*summary));
    summary->requested_count = candidate_count;
    summary->first_error = TS_OK;

    if ((candidate_count > 0 && candidates == NULL) ||
        (plan_capacity > 0 && plans == NULL) ||
        date == NULL) {
        summary->failed_count = candidate_count;
        summary->first_error = TS_INVALID_ARGUMENT;
        return TS_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < candidate_count; ++index) {
        if (summary->planned_count >= plan_capacity) {
            ++summary->failed_count;
            if (summary->first_error == TS_OK) {
                summary->first_error = TS_BUFFER_TOO_SMALL;
            }
            continue;
        }

        TsSaveBackupPlan plan;
        const TsStatus status = ts_save_backup_plan_create(
            &plan,
            candidates[index].application_id,
            candidates[index].user_id_low,
            candidates[index].user_id_high,
            date
        );
        if (status != TS_OK) {
            ++summary->failed_count;
            if (summary->first_error == TS_OK) {
                summary->first_error = status;
            }
            continue;
        }

        plans[summary->planned_count] = plan;
        ++summary->planned_count;
    }

    return summary->failed_count == 0 ? TS_OK : summary->first_error;
}
