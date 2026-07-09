#ifndef TRANSFER_SWITCH_ADAPTERS_ALBUM_MOUNTS_HPP
#define TRANSFER_SWITCH_ADAPTERS_ALBUM_MOUNTS_HPP

#include <switch.h>

namespace transfer_switch {

class AlbumMounts final {
public:
    AlbumMounts();
    ~AlbumMounts();

    AlbumMounts(const AlbumMounts&) = delete;
    AlbumMounts& operator=(const AlbumMounts&) = delete;

    bool mountAvailable();
    void unmount();

    bool sdMounted() const;
    bool nandMounted() const;
    Result sdResult() const;
    Result nandResult() const;

    static constexpr const char* sdPath() { return "album_sd:/"; }
    static constexpr const char* nandPath() { return "album_nand:/"; }

private:
    bool mountOne(FsImageDirectoryId directory, const char* name, Result& result);

    bool sd_mounted_;
    bool nand_mounted_;
    Result sd_result_;
    Result nand_result_;
};

}  // namespace transfer_switch

#endif
