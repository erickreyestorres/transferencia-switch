#ifndef TRANSFER_SWITCH_DOMAIN_SAFE_PATH_H
#define TRANSFER_SWITCH_DOMAIN_SAFE_PATH_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool ts_is_safe_direct_child(
    const char* storage_root,
    const char* parent_path,
    const char* candidate_path
);

#ifdef __cplusplus
}
#endif

#endif
