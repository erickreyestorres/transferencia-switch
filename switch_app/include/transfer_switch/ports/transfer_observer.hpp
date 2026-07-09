#ifndef TRANSFER_SWITCH_PORTS_TRANSFER_OBSERVER_HPP
#define TRANSFER_SWITCH_PORTS_TRANSFER_OBSERVER_HPP

#include <cstdint>

namespace transfer_switch {

enum class TransferOutcome {
    succeeded,
    failed,
    cancelled,
};

class TransferObserver {
public:
    virtual ~TransferObserver() = default;

    virtual void transferStarted(const char* path, uint64_t total_bytes) = 0;
    virtual void transferProgress(uint64_t transferred_bytes, uint64_t total_bytes) = 0;
    virtual void transferFinished(
        const char* path,
        uint64_t transferred_bytes,
        uint64_t total_bytes,
        TransferOutcome outcome,
        const char* detail
    ) = 0;
};

}  // namespace transfer_switch

#endif
