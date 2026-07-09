#ifndef TRANSFER_SWITCH_ADAPTERS_INSTALLED_CATALOG_HPP
#define TRANSFER_SWITCH_ADAPTERS_INSTALLED_CATALOG_HPP

#include <cstddef>

#include <switch.h>

namespace transfer_switch {

class InstalledCatalog final {
public:
    InstalledCatalog();

    bool generate();

    size_t applicationCount() const;
    size_t metadataFailureCount() const;
    Result result() const;

    static constexpr const char* rootPath() {
        return "sdmc:/switch/transferencia-switch/cache/installed/";
    }

private:
    size_t application_count_;
    size_t metadata_failure_count_;
    Result result_;
};

}  // namespace transfer_switch

#endif
