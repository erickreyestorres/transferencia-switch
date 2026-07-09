#include "transfer_switch/adapters/save_mounts.hpp"

#include <cstdio>
#include <set>

namespace transfer_switch {

SaveMounts::SaveMounts()
    : enumeration_result_(0),
      detected_count_(0),
      failed_count_(0),
      truncated_count_(0) {
}

SaveMounts::~SaveMounts() {
    unmount();
}

bool SaveMounts::mountAvailable() {
    unmount();
    enumeration_result_ = 0;
    detected_count_ = 0;
    failed_count_ = 0;
    truncated_count_ = 0;

    FsSaveDataInfoReader reader {};
    enumeration_result_ = fsOpenSaveDataInfoReader(&reader, FsSaveDataSpaceId_User);
    if (R_FAILED(enumeration_result_)) {
        return false;
    }

    std::set<std::string> seen;
    while (true) {
        FsSaveDataInfo information {};
        s64 read_count = 0;
        enumeration_result_ = fsSaveDataInfoReaderRead(&reader, &information, 1, &read_count);
        if (R_FAILED(enumeration_result_) || read_count == 0) {
            break;
        }
        if (information.save_data_type != FsSaveDataType_Account ||
            information.save_data_rank != FsSaveDataRank_Primary ||
            information.application_id == 0) {
            continue;
        }

        char unique_key[80];
        std::snprintf(
            unique_key,
            sizeof(unique_key),
            "%016llX-%016llX%016llX",
            static_cast<unsigned long long>(information.application_id),
            static_cast<unsigned long long>(information.uid.uid[1]),
            static_cast<unsigned long long>(information.uid.uid[0])
        );
        if (!seen.insert(unique_key).second) {
            continue;
        }
        ++detected_count_;
        if (mounted_.size() >= kMaximumMountedSaves) {
            ++truncated_count_;
            continue;
        }

        char device_name[24];
        std::snprintf(device_name, sizeof(device_name), "ts_save_%02u",
            static_cast<unsigned int>(mounted_.size()));
        const Result mount_result = fsdevMountSaveDataReadOnly(
            device_name,
            information.application_id,
            information.uid
        );
        if (R_FAILED(mount_result)) {
            ++failed_count_;
            continue;
        }

        char display_name[96];
        std::snprintf(
            display_name,
            sizeof(display_name),
            "%016llX - user %08llX",
            static_cast<unsigned long long>(information.application_id),
            static_cast<unsigned long long>(information.uid.uid[0] & 0xFFFFFFFFull)
        );
        mounted_.push_back(MountedSave {
            information.application_id,
            information.uid,
            device_name,
            display_name,
            std::string(device_name) + ":/",
        });
    }
    fsSaveDataInfoReaderClose(&reader);
    return !mounted_.empty();
}

void SaveMounts::unmount() {
    for (auto entry = mounted_.rbegin(); entry != mounted_.rend(); ++entry) {
        fsdevUnmountDevice(entry->device_name.c_str());
    }
    mounted_.clear();
}

const std::vector<MountedSave>& SaveMounts::mounted() const {
    return mounted_;
}

Result SaveMounts::enumerationResult() const {
    return enumeration_result_;
}

size_t SaveMounts::detectedCount() const {
    return detected_count_;
}

size_t SaveMounts::failedCount() const {
    return failed_count_;
}

size_t SaveMounts::truncatedCount() const {
    return truncated_count_;
}

}  // namespace transfer_switch
