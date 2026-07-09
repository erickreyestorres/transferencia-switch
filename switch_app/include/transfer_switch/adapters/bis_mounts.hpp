#ifndef TRANSFER_SWITCH_ADAPTERS_BIS_MOUNTS_HPP
#define TRANSFER_SWITCH_ADAPTERS_BIS_MOUNTS_HPP

#include <switch.h>

namespace transfer_switch {

class BisMounts final {
public:
    BisMounts();
    ~BisMounts();

    BisMounts(const BisMounts&) = delete;
    BisMounts& operator=(const BisMounts&) = delete;

    bool mountAvailable();
    void unmount();

    bool userMounted() const;
    bool systemMounted() const;
    Result userResult() const;
    Result systemResult() const;

    static constexpr const char* userPath() { return "nand_user:/"; }
    static constexpr const char* systemPath() { return "nand_system:/"; }

private:
    bool mountOne(FsBisPartitionId partition, const char* name, Result& result);

    bool user_mounted_;
    bool system_mounted_;
    Result user_result_;
    Result system_result_;
};

}  // namespace transfer_switch

#endif
