#include "transfer_switch/domain/safe_path.h"

#include <stddef.h>
#include <string.h>

static bool starts_with(const char* value, const char* prefix) {
    return strncmp(value, prefix, strlen(prefix)) == 0;
}

bool ts_is_safe_direct_child(
    const char* storage_root,
    const char* parent_path,
    const char* candidate_path
) {
    if (storage_root == NULL || parent_path == NULL || candidate_path == NULL) {
        return false;
    }

    const size_t root_length = strlen(storage_root);
    const size_t parent_length = strlen(parent_path);
    if (root_length == 0 || parent_length == 0 || storage_root[root_length - 1] != '/') {
        return false;
    }
    if (!starts_with(parent_path, storage_root) || !starts_with(candidate_path, storage_root)) {
        return false;
    }

    const char* name = NULL;
    if (parent_length == root_length && strcmp(parent_path, storage_root) == 0) {
        name = candidate_path + root_length;
    } else {
        if (parent_path[parent_length - 1] == '/' ||
            strncmp(candidate_path, parent_path, parent_length) != 0 ||
            candidate_path[parent_length] != '/') {
            return false;
        }
        name = candidate_path + parent_length + 1;
    }

    if (*name == '\0' || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return false;
    }
    for (const unsigned char* current = (const unsigned char*)name; *current != '\0'; ++current) {
        if (*current < 0x20 || *current == '/' || *current == '\\' || *current == ':') {
            return false;
        }
    }
    return true;
}
