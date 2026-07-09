#include "transfer_switch/adapters/bis_mounts.hpp"

namespace transfer_switch {

BisMounts::BisMounts()
    : user_mounted_(false),
      system_mounted_(false),
      user_result_(0),
      system_result_(0) {
}

BisMounts::~BisMounts() {
    unmount();
}

bool BisMounts::mountOne(
    FsBisPartitionId partition,
    const char* name,
    Result& result
) {
    FsFileSystem filesystem {};
    result = fsOpenBisFileSystem(&filesystem, partition, "");
    if (R_FAILED(result)) {
        return false;
    }
    if (fsdevMountDevice(name, filesystem) < 0) {
        result = fsdevGetLastResult();
        return false;
    }
    result = 0;
    return true;
}

bool BisMounts::mountAvailable() {
    unmount();
    user_mounted_ = mountOne(FsBisPartitionId_User, "nand_user", user_result_);
    system_mounted_ = mountOne(FsBisPartitionId_System, "nand_system", system_result_);
    return user_mounted_ || system_mounted_;
}

void BisMounts::unmount() {
    if (user_mounted_) {
        fsdevUnmountDevice("nand_user");
        user_mounted_ = false;
    }
    if (system_mounted_) {
        fsdevUnmountDevice("nand_system");
        system_mounted_ = false;
    }
}

bool BisMounts::userMounted() const {
    return user_mounted_;
}

bool BisMounts::systemMounted() const {
    return system_mounted_;
}

Result BisMounts::userResult() const {
    return user_result_;
}

Result BisMounts::systemResult() const {
    return system_result_;
}

}  // namespace transfer_switch
