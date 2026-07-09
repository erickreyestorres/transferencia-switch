#ifndef TRANSFER_SWITCH_ADAPTERS_SD_INSTALL_RECEIVER_HPP
#define TRANSFER_SWITCH_ADAPTERS_SD_INSTALL_RECEIVER_HPP

#include <cstdint>
#include <memory>
#include <string>

#include "transfer_switch/ports/incoming_object_sink.hpp"

namespace transfer_switch {

class SdInstallReceiverFactory final : public IncomingObjectSinkFactory {
public:
    explicit SdInstallReceiverFactory(uint32_t storage_id);

    bool handles(uint32_t storage_id) const override;
    std::unique_ptr<IncomingObjectSink> open(
        uint32_t storage_id,
        const char* path,
        uint64_t expected_size
    ) override;
    const char* detail() const override;

private:
    uint32_t storage_id_;
    std::string detail_;
};

}  // namespace transfer_switch

#endif
