#include "transfer_switch/domain/save_backup_plan.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int is_valid_date(const char* date) {
    if (date == NULL || strlen(date) != 10) {
        return 0;
    }
    for (size_t index = 0; index < 10; ++index) {
        if (index == 4 || index == 7) {
            if (date[index] != '-') return 0;
        } else if (!isdigit((unsigned char)date[index])) {
            return 0;
        }
    }
    return 1;
}

static TsStatus checked_snprintf(char* target, size_t capacity, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int written = vsnprintf(target, capacity, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= capacity) {
        return TS_BUFFER_TOO_SMALL;
    }
    return TS_OK;
}

TsStatus ts_save_backup_plan_create(
    TsSaveBackupPlan* plan,
    uint64_t application_id,
    uint64_t user_id_low,
    uint64_t user_id_high,
    const char* date
) {
    if (plan == NULL || application_id == 0 || !is_valid_date(date)) {
        return TS_INVALID_ARGUMENT;
    }

    memset(plan, 0, sizeof(*plan));
    plan->application_id = application_id;
    plan->user_id_low = user_id_low;
    plan->user_id_high = user_id_high;
    memcpy(plan->date, date, 11);

    TsStatus status = checked_snprintf(
        plan->folder_name,
        sizeof(plan->folder_name),
        "%016llX_user_%08llX",
        (unsigned long long)application_id,
        (unsigned long long)(user_id_low & 0xFFFFFFFFull)
    );
    if (status != TS_OK) return status;

    status = checked_snprintf(
        plan->backup_path,
        sizeof(plan->backup_path),
        "%s/%s/%s",
        TS_SAVE_BACKUP_ROOT,
        plan->date,
        plan->folder_name
    );
    if (status != TS_OK) return status;

    status = checked_snprintf(
        plan->files_path,
        sizeof(plan->files_path),
        "%s/files",
        plan->backup_path
    );
    if (status != TS_OK) return status;

    return checked_snprintf(
        plan->manifest_path,
        sizeof(plan->manifest_path),
        "%s/manifest.json",
        plan->backup_path
    );
}

TsStatus ts_save_backup_manifest_json(
    const TsSaveBackupPlan* plan,
    char* buffer,
    size_t capacity
) {
    if (plan == NULL || buffer == NULL || capacity == 0) {
        return TS_INVALID_ARGUMENT;
    }
    return checked_snprintf(
        buffer,
        capacity,
        "{\n"
        "  \"schema\": \"transferencia-switch.save-backup.v1\",\n"
        "  \"application_id\": \"%016llX\",\n"
        "  \"user_id_low\": \"%016llX\",\n"
        "  \"user_id_high\": \"%016llX\",\n"
        "  \"date\": \"%s\",\n"
        "  \"files_dir\": \"files\"\n"
        "}\n",
        (unsigned long long)plan->application_id,
        (unsigned long long)plan->user_id_low,
        (unsigned long long)plan->user_id_high,
        plan->date
    );
}
