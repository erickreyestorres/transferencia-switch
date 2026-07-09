#ifndef TRANSFER_SWITCH_ADAPTERS_SAVE_MOUNTS_HPP
#define TRANSFER_SWITCH_ADAPTERS_SAVE_MOUNTS_HPP

#include <string>
#include <vector>

#include <switch.h>

namespace transfer_switch {

struct MountedSave {
    u64 application_id;
    AccountUid uid;
    std::string device_name;
    std::string display_name;
    std::string path;
};

class SaveMounts final {
public:
    static constexpr size_t kMaximumMountedSaves = 20;

    SaveMounts();
    ~SaveMounts();

    SaveMounts(const SaveMounts&) = delete;
    SaveMounts& operator=(const SaveMounts&) = delete;

    bool mountAvailable();
    void unmount();

    const std::vector<MountedSave>& mounted() const;
    Result enumerationResult() const;
    size_t detectedCount() const;
    size_t failedCount() const;
    size_t truncatedCount() const;

private:
    std::vector<MountedSave> mounted_;
    Result enumeration_result_;
    size_t detected_count_;
    size_t failed_count_;
    size_t truncated_count_;
};

}  // namespace transfer_switch

#endif
