#ifndef TRANSFER_SWITCH_ADAPTERS_CONSOLE_TRANSFER_PRESENTER_HPP
#define TRANSFER_SWITCH_ADAPTERS_CONSOLE_TRANSFER_PRESENTER_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "transfer_switch/ports/transfer_observer.hpp"

namespace transfer_switch {

class ConsoleTransferPresenter final : public TransferObserver {
public:
    ConsoleTransferPresenter();

    void setStorageNote(const char* note);
    void showWaiting();
    void transferStarted(const char* path, uint64_t total_bytes) override;
    void transferProgress(uint64_t transferred_bytes, uint64_t total_bytes) override;
    void transferFinished(
        const char* path,
        uint64_t transferred_bytes,
        uint64_t total_bytes,
        TransferOutcome outcome,
        const char* detail
    ) override;
    void writeSummary(char* destination, size_t capacity) const;

private:
    struct ResultLine {
        std::string name;
        TransferOutcome outcome;
        std::string detail;
    };

    void render(const char* state);
    static std::string fileName(const std::string& path);

    std::string active_path_;
    std::string storage_note_;
    uint64_t transferred_bytes_;
    uint64_t total_bytes_;
    unsigned int last_percent_;
    unsigned int succeeded_;
    unsigned int failed_;
    unsigned int cancelled_;
    std::vector<ResultLine> recent_;
};

}  // namespace transfer_switch

#endif
