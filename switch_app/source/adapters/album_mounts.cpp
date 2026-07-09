#include "transfer_switch/adapters/album_mounts.hpp"

namespace transfer_switch {

AlbumMounts::AlbumMounts()
    : sd_mounted_(false),
      nand_mounted_(false),
      sd_result_(0),
      nand_result_(0) {
}

AlbumMounts::~AlbumMounts() {
    unmount();
}

bool AlbumMounts::mountOne(
    FsImageDirectoryId directory,
    const char* name,
    Result& result
) {
    FsFileSystem filesystem {};
    result = fsOpenImageDirectoryFileSystem(&filesystem, directory);
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

bool AlbumMounts::mountAvailable() {
    unmount();
    sd_mounted_ = mountOne(FsImageDirectoryId_Sd, "album_sd", sd_result_);
    nand_mounted_ = mountOne(FsImageDirectoryId_Nand, "album_nand", nand_result_);
    return sd_mounted_ || nand_mounted_;
}

void AlbumMounts::unmount() {
    if (sd_mounted_) {
        fsdevUnmountDevice("album_sd");
        sd_mounted_ = false;
    }
    if (nand_mounted_) {
        fsdevUnmountDevice("album_nand");
        nand_mounted_ = false;
    }
}

bool AlbumMounts::sdMounted() const {
    return sd_mounted_;
}

bool AlbumMounts::nandMounted() const {
    return nand_mounted_;
}

Result AlbumMounts::sdResult() const {
    return sd_result_;
}

Result AlbumMounts::nandResult() const {
    return nand_result_;
}

}  // namespace transfer_switch
