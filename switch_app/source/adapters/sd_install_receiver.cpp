#include "transfer_switch/adapters/sd_install_receiver.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <sys/stat.h>
#include <string>
#include <utility>
#include <vector>

#include <switch.h>

namespace transfer_switch {
namespace {

constexpr uint32_t kPfs0Magic = 0x30534650;
constexpr uint32_t kHfs0Magic = 0x30534648;
constexpr uint64_t kXciRootOffset = 0xF000;
constexpr size_t kNcaHeaderSize = 0x4000;
constexpr size_t kMaxFiles = 4096;
constexpr size_t kMaxHeaderSize = 8 * 1024 * 1024;
constexpr size_t kMaxSidecarSize = 2 * 1024 * 1024;
constexpr long kMaxInstallLogSize = 512 * 1024;
constexpr const char* kLogDir = "sdmc:/switch/transferencia-switch/logs";
constexpr const char* kInstallLogPath = "sdmc:/switch/transferencia-switch/logs/install.log";
constexpr const char* kPreviousInstallLogPath =
    "sdmc:/switch/transferencia-switch/logs/install.previous.log";

void ensureLogDirectory() {
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/transferencia-switch", 0777);
    mkdir(kLogDir, 0777);
}

void rotateInstallLogIfNeeded() {
    struct stat info {};
    if (stat(kInstallLogPath, &info) != 0 || info.st_size < kMaxInstallLogSize) {
        return;
    }

    std::remove(kPreviousInstallLogPath);
    std::rename(kInstallLogPath, kPreviousInstallLogPath);
}

void appendInstallLog(const char* format, ...) {
    ensureLogDirectory();
    rotateInstallLogIfNeeded();
    FILE* file = std::fopen(kInstallLogPath, "a");
    if (file == nullptr) return;

    std::fputs("[transferencia-switch] ", file);
    va_list args;
    va_start(args, format);
    std::vfprintf(file, format, args);
    va_end(args);
    std::fputc('\n', file);
    std::fclose(file);
}

struct Pfs0Header {
    uint32_t magic;
    uint32_t file_count;
    uint32_t string_table_size;
    uint32_t reserved;
} NX_PACKED;

struct Pfs0Entry {
    uint64_t data_offset;
    uint64_t file_size;
    uint32_t string_offset;
    uint32_t reserved;
} NX_PACKED;

struct Hfs0Entry {
    uint64_t data_offset;
    uint64_t file_size;
    uint32_t string_offset;
    uint32_t hashed_size;
    uint64_t reserved;
    uint8_t hash[0x20];
} NX_PACKED;

enum class PackageType {
    nsp,
    xci,
};

struct PackagedContentMetaHeader {
    uint64_t title_id;
    uint32_t version;
    uint8_t type;
    uint8_t reserved_0d;
    uint16_t extended_header_size;
    uint16_t content_count;
    uint16_t content_meta_count;
    uint8_t attributes;
    uint8_t storage_id;
    uint8_t install_type;
    uint8_t committed;
    uint32_t required_system_version;
    uint32_t reserved_1c;
} NX_PACKED;

struct ApplicationRecordStorage {
    NcmContentMetaKey key;
    uint64_t storage_id;
};

struct Entry {
    uint64_t offset;
    uint64_t size;
    std::string name;
    bool nca;
    bool cnmt;
    bool ticket;
    bool certificate;
    NcmContentId content_id;
};

bool endsWith(const std::string& value, const char* suffix) {
    const size_t suffix_size = std::strlen(suffix);
    return value.size() >= suffix_size &&
        value.compare(value.size() - suffix_size, suffix_size, suffix) == 0;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool contentIdFromName(const std::string& name, NcmContentId& output) {
    if (name.size() < 36) {
        return false;
    }
    for (size_t index = 0; index < sizeof(output.c); ++index) {
        const char high = name[index * 2];
        const char low_value = name[index * 2 + 1];
        auto nibble = [](char value) -> int {
            if (value >= '0' && value <= '9') return value - '0';
            if (value >= 'a' && value <= 'f') return value - 'a' + 10;
            if (value >= 'A' && value <= 'F') return value - 'A' + 10;
            return -1;
        };
        const int high_value = nibble(high);
        const int low_nibble = nibble(low_value);
        if (high_value < 0 || low_nibble < 0) {
            return false;
        }
        output.c[index] = static_cast<uint8_t>((high_value << 4) | low_nibble);
    }
    return true;
}

NcmPlaceHolderId placeholderFromContentId(const NcmContentId& content_id) {
    NcmPlaceHolderId placeholder {};
    static_assert(sizeof(placeholder) == sizeof(content_id));
    std::memcpy(&placeholder, &content_id, sizeof(placeholder));
    return placeholder;
}

void appendBytes(std::vector<uint8_t>& output, const void* data, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    output.insert(output.end(), bytes, bytes + size);
}

uint64_t baseApplicationId(uint64_t title_id, uint8_t type) {
    if (type == NcmContentMetaType_Patch) {
        return title_id ^ 0x800;
    }
    if (type == NcmContentMetaType_AddOnContent) {
        return (title_id ^ 0x1000) & ~0xFFFull;
    }
    return title_id;
}

class SdPackageInstallSink final : public IncomingObjectSink {
public:
    SdPackageInstallSink(std::string name, uint64_t expected_size, PackageType package_type)
        : name_(std::move(name)), expected_size_(expected_size), package_type_(package_type) {
    }

    ~SdPackageInstallSink() override {
        if (!finished_) {
            abort();
        }
        closeServices();
    }

    bool initialize() {
        appendInstallLog(
            "START file=\"%s\" type=%s expected=%llu",
            name_.c_str(),
            package_type_ == PackageType::xci ? "XCI" : "NSP",
            static_cast<unsigned long long>(expected_size_)
        );
        // Un NSP válido mínimo necesita al menos la cabecera PFS0 + una entrada + tabla
        // de strings. Un XCI mínimo válido tiene el preámbulo de 0xF000 bytes.
        // 1 MiB es un límite conservador que rechaza archivos de texto/prueba
        // antes de llegar al parser.
        constexpr uint64_t kMinPackageSize = 1024ull * 1024ull;
        if (expected_size_ == 0xFFFFFFFFull) {
            return fail("archivo grande con tamano MTP desconocido aun no soportado");
        }
        if (expected_size_ < kMinPackageSize) {
            return fail("tamano de paquete no compatible en esta version");
        }
        Result result = ncmInitialize();
        if (R_FAILED(result)) return failResult("no se pudo iniciar NCM", result);
        ncm_initialized_ = true;

        result = ncmOpenContentStorage(&content_storage_, NcmStorageId_SdCard);
        if (R_FAILED(result)) return failResult("no se pudo abrir ContentStorage SD", result);
        content_storage_open_ = true;

        result = nsInitialize();
        if (R_FAILED(result)) return failResult("no se pudo iniciar NS", result);
        ns_initialized_ = true;
        result = nsGetApplicationManagerInterface(&application_manager_);
        if (R_FAILED(result)) return failResult("no se pudo abrir ApplicationManager", result);
        application_manager_open_ = true;

        if (package_type_ == PackageType::xci) {
            result = splCryptoInitialize();
            if (R_FAILED(result)) return failResult("no se pudo iniciar SPL crypto", result);
            spl_crypto_initialized_ = true;
            result = splInitialize();
            if (R_FAILED(result)) return failResult("no se pudo iniciar SPL general", result);
            spl_initialized_ = true;
            static constexpr uint8_t header_kek_source[0x10] = {
                0x1F, 0x12, 0x91, 0x3A, 0x4A, 0xCB, 0xF0, 0x0D,
                0x4C, 0xDE, 0x3A, 0xF6, 0xD5, 0x23, 0x88, 0x2A,
            };
            static constexpr uint8_t header_key_source[0x20] = {
                0x5A, 0x3E, 0xD8, 0x4F, 0xDE, 0xC0, 0xD8, 0x26,
                0x31, 0xF7, 0xE2, 0x5D, 0x19, 0x7B, 0xF5, 0xD0,
                0x1C, 0x9B, 0x7B, 0xFA, 0xF6, 0x28, 0x18, 0x3D,
                0x71, 0xF6, 0x4D, 0x73, 0xF1, 0x50, 0xB9, 0xD2,
            };
            uint8_t sealed_kek[0x10] {};
            result = splCryptoGenerateAesKek(header_kek_source, 0, 0, sealed_kek);
            if (R_SUCCEEDED(result)) {
                result = splCryptoGenerateAesKey(
                    sealed_kek, header_key_source, header_key_.data());
            }
            if (R_SUCCEEDED(result)) {
                result = splCryptoGenerateAesKey(
                    sealed_kek, header_key_source + 0x10, header_key_.data() + 0x10);
            }
            if (R_FAILED(result)) return failResult("no se pudo derivar NCA header key", result);
        }

        detail_ = package_type_ == PackageType::xci
            ? "receptor XCI preparado"
            : "receptor NSP preparado";
        return true;
    }

    bool write(const void* data, size_t size) override {
        if (failed_ || finished_ || data == nullptr || received_ + size > expected_size_) {
            return fail("flujo de paquete fuera de limites");
        }
        const auto* bytes = static_cast<const uint8_t*>(data);
        size_t remaining = size;
        while (remaining > 0 && !failed_) {
            if (!header_parsed_) {
                const size_t consumed = package_type_ == PackageType::xci
                    ? consumeXciHeader(bytes, remaining)
                    : consumeNspHeader(bytes, remaining);
                bytes += consumed;
                remaining -= consumed;
                received_ += consumed;
                continue;
            }
            const size_t consumed = consumeData(bytes, remaining);
            if (consumed == 0 && !failed_) {
                return fail("el analizador del paquete no pudo avanzar");
            }
            bytes += consumed;
            remaining -= consumed;
            received_ += consumed;
        }
        return !failed_;
    }

    bool finish() override {
        if (failed_) return false;
        while (current_entry_ < entries_.size() &&
               entries_[current_entry_].size == 0 &&
               data_position_ == entries_[current_entry_].offset) {
            Entry& empty = entries_[current_entry_];
            if (!beginEntry(empty) || !endEntry(empty)) return false;
            ++current_entry_;
            entry_started_ = false;
        }
        if (received_ != expected_size_ || !header_parsed_ || current_entry_ != entries_.size()) {
            return fail("paquete incompleto");
        }
        if (cnmt_ids_.empty() || cnmt_ids_.size() > 32) {
            return fail("cantidad de CNMT no compatible");
        }
        if (!importTickets()) {
            rollback();
            return false;
        }
        for (const NcmContentId& cnmt_id : cnmt_ids_) {
            if (!installMetadata(cnmt_id)) {
                rollback();
                return false;
            }
        }
        finished_ = true;
        detail_ = package_type_ == PackageType::xci
            ? "XCI instalado correctamente en SD"
            : "NSP instalado correctamente en SD";
        appendInstallLog(
            "OK file=\"%s\" type=%s received=%llu cnmt=%u nca=%u",
            name_.c_str(),
            package_type_ == PackageType::xci ? "XCI" : "NSP",
            static_cast<unsigned long long>(received_),
            static_cast<unsigned int>(cnmt_ids_.size()),
            static_cast<unsigned int>(newly_registered_.size())
        );
        return true;
    }

    void abort() override {
        if (aborted_ || finished_) return;
        aborted_ = true;
        if (entry_placeholder_open_) {
            const NcmPlaceHolderId placeholder = placeholderFromContentId(active_content_id_);
            ncmContentStorageDeletePlaceHolder(&content_storage_, &placeholder);
            entry_placeholder_open_ = false;
        }
        rollback();
        if (detail_.empty()) detail_ = "instalacion cancelada";
        appendInstallLog(
            "CANCEL file=\"%s\" type=%s received=%llu/%llu detail=\"%s\"",
            name_.c_str(),
            package_type_ == PackageType::xci ? "XCI" : "NSP",
            static_cast<unsigned long long>(received_),
            static_cast<unsigned long long>(expected_size_),
            detail_.c_str()
        );
    }

    const char* detail() const override {
        return detail_.c_str();
    }

private:
    bool fail(const char* message) {
        failed_ = true;
        detail_ = message == nullptr ? "error de instalacion" : message;
        appendInstallLog(
            "ERROR file=\"%s\" type=%s received=%llu/%llu data_pos=%llu entry=%u/%u detail=\"%s\"",
            name_.c_str(),
            package_type_ == PackageType::xci ? "XCI" : "NSP",
            static_cast<unsigned long long>(received_),
            static_cast<unsigned long long>(expected_size_),
            static_cast<unsigned long long>(data_position_),
            static_cast<unsigned int>(current_entry_),
            static_cast<unsigned int>(entries_.size()),
            detail_.c_str()
        );
        return false;
    }

    bool failResult(const char* message, Result result) {
        char buffer[160];
        std::snprintf(buffer, sizeof(buffer), "%s (0x%08X)", message, result);
        return fail(buffer);
    }

    size_t consumeNspHeader(const uint8_t* data, size_t size) {
        size_t wanted = sizeof(Pfs0Header);
        if (header_target_ != 0) wanted = header_target_;
        const size_t needed = wanted - header_.size();
        const size_t amount = std::min(size, needed);
        header_.insert(header_.end(), data, data + amount);

        if (header_target_ == 0 && header_.size() == sizeof(Pfs0Header)) {
            const auto* base = reinterpret_cast<const Pfs0Header*>(header_.data());
            if (base->magic != kPfs0Magic || base->file_count == 0 ||
                base->file_count > kMaxFiles) {
                fail("cabecera PFS0 invalida");
                return amount;
            }
            const uint64_t target = sizeof(Pfs0Header) +
                static_cast<uint64_t>(base->file_count) * sizeof(Pfs0Entry) +
                base->string_table_size;
            if (target > kMaxHeaderSize || target > expected_size_) {
                fail("cabecera PFS0 demasiado grande");
                return amount;
            }
            header_target_ = static_cast<size_t>(target);
            header_.reserve(header_target_);
        }
        if (!failed_ && header_target_ != 0 && header_.size() == header_target_) {
            parseNspHeader();
        }
        return amount;
    }

    size_t consumeXciHeader(const uint8_t* data, size_t size) {
        size_t consumed = 0;
        while (consumed < size && !header_parsed_ && !failed_) {
            if (xci_cursor_ < kXciRootOffset) {
                const size_t amount = static_cast<size_t>(std::min<uint64_t>(
                    size - consumed, kXciRootOffset - xci_cursor_));
                xci_cursor_ += amount;
                consumed += amount;
                continue;
            }

            if (xci_root_target_ == 0 || xci_root_header_.size() < xci_root_target_) {
                size_t wanted = xci_root_target_ == 0
                    ? sizeof(Pfs0Header)
                    : xci_root_target_;
                const size_t amount = std::min(
                    size - consumed, wanted - xci_root_header_.size());
                xci_root_header_.insert(
                    xci_root_header_.end(), data + consumed, data + consumed + amount);
                xci_cursor_ += amount;
                consumed += amount;
                if (xci_root_target_ == 0 &&
                    xci_root_header_.size() == sizeof(Pfs0Header)) {
                    const auto* base = reinterpret_cast<const Pfs0Header*>(
                        xci_root_header_.data());
                    if (!setHfsHeaderTarget(*base, xci_root_target_, "HFS0 raiz invalido")) {
                        return consumed;
                    }
                    xci_root_header_.reserve(xci_root_target_);
                }
                if (xci_root_target_ == 0 || xci_root_header_.size() < xci_root_target_) {
                    continue;
                }
                if (!findSecurePartition()) return consumed;
                continue;
            }

            if (xci_cursor_ < xci_secure_offset_) {
                const size_t amount = static_cast<size_t>(std::min<uint64_t>(
                    size - consumed, xci_secure_offset_ - xci_cursor_));
                xci_cursor_ += amount;
                consumed += amount;
                continue;
            }

            size_t wanted = xci_secure_target_ == 0
                ? sizeof(Pfs0Header)
                : xci_secure_target_;
            const size_t amount = std::min(
                size - consumed, wanted - xci_secure_header_.size());
            xci_secure_header_.insert(
                xci_secure_header_.end(), data + consumed, data + consumed + amount);
            xci_cursor_ += amount;
            consumed += amount;
            if (xci_secure_target_ == 0 &&
                xci_secure_header_.size() == sizeof(Pfs0Header)) {
                const auto* base = reinterpret_cast<const Pfs0Header*>(
                    xci_secure_header_.data());
                if (!setHfsHeaderTarget(*base, xci_secure_target_, "HFS0 secure invalido")) {
                    return consumed;
                }
                if (xci_secure_offset_ + xci_secure_target_ > expected_size_) {
                    fail("cabecera secure fuera del XCI");
                    return consumed;
                }
                xci_secure_header_.reserve(xci_secure_target_);
            }
            if (xci_secure_target_ != 0 &&
                xci_secure_header_.size() == xci_secure_target_) {
                if (!parseXciSecureHeader()) return consumed;
                header_parsed_ = true;
                data_position_ = xci_cursor_;
            }
        }
        return consumed;
    }

    bool setHfsHeaderTarget(
        const Pfs0Header& base,
        size_t& target,
        const char* error
    ) {
        if (base.magic != kHfs0Magic || base.file_count == 0 ||
            base.file_count > kMaxFiles) {
            return fail(error);
        }
        const uint64_t calculated = sizeof(Pfs0Header) +
            static_cast<uint64_t>(base.file_count) * sizeof(Hfs0Entry) +
            base.string_table_size;
        if (calculated > kMaxHeaderSize) return fail("cabecera HFS0 demasiado grande");
        target = static_cast<size_t>(calculated);
        return true;
    }

    bool findSecurePartition() {
        const auto* base = reinterpret_cast<const Pfs0Header*>(xci_root_header_.data());
        const auto* raw_entries = reinterpret_cast<const Hfs0Entry*>(
            xci_root_header_.data() + sizeof(Pfs0Header));
        const char* strings = reinterpret_cast<const char*>(
            xci_root_header_.data() + sizeof(Pfs0Header) +
            base->file_count * sizeof(Hfs0Entry));
        const uint64_t data_start = kXciRootOffset + xci_root_target_;
        for (uint32_t index = 0; index < base->file_count; ++index) {
            const Hfs0Entry& raw = raw_entries[index];
            if (raw.string_offset >= base->string_table_size) return fail("tabla HFS0 raiz invalida");
            const size_t available = base->string_table_size - raw.string_offset;
            const char* name = strings + raw.string_offset;
            if (std::memchr(name, 0, available) == nullptr) return fail("nombre HFS0 invalido");
            appendInstallLog(
                "XCI root entry file=\"%s\" index=%u name=\"%s\" data_start=%llu data_offset=%llu size=%llu expected=%llu",
                name_.c_str(),
                static_cast<unsigned int>(index),
                name,
                static_cast<unsigned long long>(data_start),
                static_cast<unsigned long long>(raw.data_offset),
                static_cast<unsigned long long>(raw.file_size),
                static_cast<unsigned long long>(expected_size_)
            );
            if (std::strcmp(name, "secure") == 0) {
                if (raw.data_offset + raw.file_size < raw.data_offset ||
                    data_start + raw.data_offset + raw.file_size > expected_size_) {
                    return fail("particion secure fuera del XCI");
                }
                xci_secure_offset_ = data_start + raw.data_offset;
                xci_secure_size_ = raw.file_size;
                if (xci_secure_offset_ < xci_cursor_) return fail("offset secure invalido");
                appendInstallLog(
                    "XCI secure file=\"%s\" offset=%llu size=%llu header=%u entries=%u",
                    name_.c_str(),
                    static_cast<unsigned long long>(xci_secure_offset_),
                    static_cast<unsigned long long>(xci_secure_size_),
                    static_cast<unsigned int>(xci_secure_target_),
                    static_cast<unsigned int>(base->file_count)
                );
                return true;
            }
        }
        return fail("particion secure no encontrada");
    }

    bool parseXciSecureHeader() {
        const auto* base = reinterpret_cast<const Pfs0Header*>(xci_secure_header_.data());
        const auto* raw_entries = reinterpret_cast<const Hfs0Entry*>(
            xci_secure_header_.data() + sizeof(Pfs0Header));
        const char* strings = reinterpret_cast<const char*>(
            xci_secure_header_.data() + sizeof(Pfs0Header) +
            base->file_count * sizeof(Hfs0Entry));
        const uint64_t data_start = xci_secure_offset_ + xci_secure_target_;
        // Los offsets de las NCAs son relativos al inicio de los datos de la partición
        // secure (es decir, después del header HFS0). xci_secure_size_ es el tamaño
        // total de la partición (header + datos). Los datos disponibles son:
        //   xci_secure_size_ - xci_secure_target_
        // La validación correcta es que cada entrada quepa dentro de ese espacio.
        if (xci_secure_size_ < xci_secure_target_) {
            return fail("particion secure menor que su cabecera");
        }
        const uint64_t secure_data_size = xci_secure_size_ - xci_secure_target_;
        uint64_t previous_end = 0;
        std::set<std::string> names;
        std::set<std::array<uint8_t, 16>> content_ids;
        entries_.reserve(base->file_count);
        newly_registered_.reserve(base->file_count);
        cnmt_ids_.reserve(base->file_count);
        for (uint32_t index = 0; index < base->file_count; ++index) {
            const Hfs0Entry& raw = raw_entries[index];
            if (raw.string_offset >= base->string_table_size || raw.data_offset < previous_end ||
                raw.data_offset + raw.file_size < raw.data_offset ||
                raw.data_offset + raw.file_size > secure_data_size) {
                return fail("tabla HFS0 secure invalida");
            }
            const size_t available = base->string_table_size - raw.string_offset;
            const char* raw_name = strings + raw.string_offset;
            if (std::memchr(raw_name, 0, available) == nullptr) {
                return fail("nombre secure sin terminador");
            }
            std::string name(raw_name);
            const std::string normalized = lower(name);
            if (!names.insert(normalized).second) return fail("nombre secure duplicado");
            Entry entry {
                data_start + raw.data_offset,
                raw.file_size,
                name,
                endsWith(normalized, ".nca"),
                endsWith(normalized, ".cnmt.nca"),
                endsWith(normalized, ".tik"),
                endsWith(normalized, ".cert"),
                {},
            };
            if (entry.nca && !contentIdFromName(name, entry.content_id)) {
                return fail("nombre NCA de XCI invalido");
            }
            if (entry.nca) {
                std::array<uint8_t, 16> id {};
                std::memcpy(id.data(), entry.content_id.c, id.size());
                if (!content_ids.insert(id).second) return fail("Content ID de XCI duplicado");
            }
            entries_.push_back(std::move(entry));
            previous_end = raw.data_offset + raw.file_size;
        }
        appendInstallLog(
            "XCI secure parsed file=\"%s\" entries=%u cnmt_candidates=%u data_start=%llu data_size=%llu",
            name_.c_str(),
            static_cast<unsigned int>(entries_.size()),
            static_cast<unsigned int>(cnmt_ids_.capacity()),
            static_cast<unsigned long long>(data_start),
            static_cast<unsigned long long>(secure_data_size)
        );
        return true;
    }

    bool parseNspHeader() {
        const auto* base = reinterpret_cast<const Pfs0Header*>(header_.data());
        const auto* raw_entries = reinterpret_cast<const Pfs0Entry*>(
            header_.data() + sizeof(Pfs0Header));
        const char* strings = reinterpret_cast<const char*>(
            header_.data() + sizeof(Pfs0Header) + base->file_count * sizeof(Pfs0Entry));

        uint64_t previous_end = 0;
        std::set<std::string> names;
        std::set<std::array<uint8_t, 16>> content_ids;
        entries_.reserve(base->file_count);
        newly_registered_.reserve(base->file_count);
        cnmt_ids_.reserve(base->file_count);
        for (uint32_t index = 0; index < base->file_count; ++index) {
            const Pfs0Entry& raw = raw_entries[index];
            if (raw.string_offset >= base->string_table_size || raw.data_offset < previous_end ||
                raw.data_offset + raw.file_size < raw.data_offset) {
                return fail("tabla PFS0 invalida");
            }
            const size_t available = base->string_table_size - raw.string_offset;
            const char* raw_name = strings + raw.string_offset;
            const void* terminator = std::memchr(raw_name, 0, available);
            if (terminator == nullptr) return fail("nombre PFS0 sin terminador");

            std::string name(raw_name);
            const std::string normalized = lower(name);
            if (!names.insert(normalized).second) return fail("nombre PFS0 duplicado");
            Entry entry {
                header_target_ + raw.data_offset,
                raw.file_size,
                name,
                endsWith(normalized, ".nca"),
                endsWith(normalized, ".cnmt.nca"),
                endsWith(normalized, ".tik"),
                endsWith(normalized, ".cert"),
                {},
            };
            if (entry.nca && !contentIdFromName(name, entry.content_id)) {
                return fail("nombre NCA invalido");
            }
            if (entry.nca) {
                std::array<uint8_t, 16> id {};
                std::memcpy(id.data(), entry.content_id.c, id.size());
                if (!content_ids.insert(id).second) return fail("Content ID duplicado");
            }
            if ((entry.ticket || entry.certificate) && entry.size > kMaxSidecarSize) {
                return fail("ticket o certificado demasiado grande");
            }
            entries_.push_back(std::move(entry));
            previous_end = raw.data_offset + raw.file_size;
        }
        if (header_target_ + previous_end != expected_size_) {
            return fail("tamano NSP no coincide con PFS0");
        }
        appendInstallLog(
            "NSP parsed file=\"%s\" entries=%u payload=%llu",
            name_.c_str(),
            static_cast<unsigned int>(entries_.size()),
            static_cast<unsigned long long>(previous_end)
        );
        header_parsed_ = true;
        data_position_ = header_target_;
        return true;
    }

    size_t consumeData(const uint8_t* data, size_t size) {
        while (current_entry_ < entries_.size() &&
               entries_[current_entry_].size == 0 &&
               data_position_ == entries_[current_entry_].offset) {
            Entry& empty = entries_[current_entry_];
            if (!beginEntry(empty) || !endEntry(empty)) return 0;
            ++current_entry_;
            entry_started_ = false;
        }
        if (current_entry_ >= entries_.size()) {
            const uint64_t trailing = expected_size_ - received_;
            return static_cast<size_t>(std::min<uint64_t>(size, trailing));
        }
        Entry& entry = entries_[current_entry_];
        if (data_position_ < entry.offset) {
            const size_t gap = static_cast<size_t>(std::min<uint64_t>(
                size, entry.offset - data_position_));
            data_position_ += gap;
            return gap;
        }
        if (!entry_started_ && !beginEntry(entry)) return 0;

        const uint64_t entry_offset = data_position_ - entry.offset;
        const size_t amount = static_cast<size_t>(std::min<uint64_t>(
            size, entry.size - entry_offset));
        if (!consumeEntry(entry, entry_offset, data, amount)) return 0;
        data_position_ += amount;
        if (entry_offset + amount == entry.size) {
            if (!endEntry(entry)) return 0;
            ++current_entry_;
            entry_started_ = false;
        }
        return amount;
    }

    bool beginEntry(const Entry& entry) {
        entry_started_ = true;
        sidecar_.clear();
        if (!entry.nca) return true;
        nca_header_buffer_.clear();
        appendInstallLog(
            "ENTRY begin file=\"%s\" name=\"%s\" size=%llu cnmt=%u offset=%llu",
            name_.c_str(),
            entry.name.c_str(),
            static_cast<unsigned long long>(entry.size),
            entry.cnmt ? 1u : 0u,
            static_cast<unsigned long long>(entry.offset)
        );
        if (package_type_ == PackageType::xci && !entry.cnmt && entry.size < kNcaHeaderSize) {
            char message[128];
            std::snprintf(
                message,
                sizeof(message),
                "XCI NCA chica: %.48s (%llu bytes)",
                entry.name.c_str(),
                static_cast<unsigned long long>(entry.size)
            );
            return fail(message);
        }

        active_content_id_ = entry.content_id;
        bool present = false;
        Result result = ncmContentStorageHas(&content_storage_, &present, &entry.content_id);
        if (R_FAILED(result)) return failResult("no se pudo consultar NCA", result);
        if (present) {
            int64_t installed_size = 0;
            result = ncmContentStorageGetSizeFromContentId(
                &content_storage_, &installed_size, &entry.content_id);
            if (R_FAILED(result) || installed_size != static_cast<int64_t>(entry.size)) {
                return fail("NCA existente con tamano diferente");
            }
            skip_active_nca_ = true;
            return true;
        }

        const NcmPlaceHolderId placeholder = placeholderFromContentId(entry.content_id);
        ncmContentStorageDeletePlaceHolder(&content_storage_, &placeholder);
        result = ncmContentStorageCreatePlaceHolder(
            &content_storage_, &entry.content_id, &placeholder, entry.size);
        if (R_FAILED(result)) return failResult("no se pudo crear placeholder SD", result);
        entry_placeholder_open_ = true;
        skip_active_nca_ = false;
        return true;
    }

    bool consumeEntry(
        const Entry& entry,
        uint64_t offset,
        const uint8_t* data,
        size_t size
    ) {
        if (entry.nca && !skip_active_nca_) {
            const NcmPlaceHolderId placeholder = placeholderFromContentId(entry.content_id);
            size_t consumed = 0;
            if (package_type_ == PackageType::xci && !entry.cnmt && offset < kNcaHeaderSize) {
                if (nca_header_buffer_.size() != offset) {
                    return fail("flujo de cabecera NCA no secuencial");
                }
                const size_t amount = std::min(size, kNcaHeaderSize - nca_header_buffer_.size());
                nca_header_buffer_.insert(
                    nca_header_buffer_.end(), data, data + amount);
                consumed += amount;
                if (nca_header_buffer_.size() == kNcaHeaderSize) {
                    if (!convertGamecardNcaHeader()) return false;
                    const Result result = ncmContentStorageWritePlaceHolder(
                        &content_storage_, &placeholder, 0,
                        nca_header_buffer_.data(), nca_header_buffer_.size());
                    if (R_FAILED(result)) {
                        return failResult("fallo escribiendo cabecera NCA", result);
                    }
                }
            }
            if (consumed < size) {
                const Result result = ncmContentStorageWritePlaceHolder(
                    &content_storage_, &placeholder, offset + consumed,
                    data + consumed, size - consumed);
                if (R_FAILED(result)) return failResult("fallo escribiendo placeholder", result);
            }
        } else if (entry.ticket || entry.certificate) {
            sidecar_.insert(sidecar_.end(), data, data + size);
        }
        return true;
    }

    bool convertGamecardNcaHeader() {
        if (nca_header_buffer_.size() != kNcaHeaderSize) {
            return fail("cabecera NCA incompleta");
        }
        std::vector<uint8_t> plain(kNcaHeaderSize);
        Aes128XtsContext decryptor {};
        aes128XtsContextCreate(
            &decryptor, header_key_.data(), header_key_.data() + 0x10, false);
        for (size_t offset = 0; offset < kNcaHeaderSize; offset += 0x200) {
            aes128XtsContextResetSector(&decryptor, offset / 0x200, true);
            aes128XtsDecrypt(
                &decryptor, plain.data() + offset, nca_header_buffer_.data() + offset, 0x200);
        }
        const uint32_t magic = static_cast<uint32_t>(plain[0x200]) |
            (static_cast<uint32_t>(plain[0x201]) << 8) |
            (static_cast<uint32_t>(plain[0x202]) << 16) |
            (static_cast<uint32_t>(plain[0x203]) << 24);
        if (magic != 0x3341434E) {
            char message[96];
            std::snprintf(message, sizeof(message),
                "magic NCA3 invalido en XCI: 0x%08X", magic);
            return fail(message);
        }
        plain[0x204] = 0;

        Aes128XtsContext encryptor {};
        aes128XtsContextCreate(
            &encryptor, header_key_.data(), header_key_.data() + 0x10, true);
        for (size_t offset = 0; offset < kNcaHeaderSize; offset += 0x200) {
            aes128XtsContextResetSector(&encryptor, offset / 0x200, true);
            aes128XtsEncrypt(
                &encryptor, nca_header_buffer_.data() + offset, plain.data() + offset, 0x200);
        }
        return true;
    }

    bool endEntry(const Entry& entry) {
        if (entry.nca) {
            if (!skip_active_nca_) {
                Result result = ncmContentStorageFlushPlaceHolder(&content_storage_);
                if (R_FAILED(result)) return failResult("no se pudo sincronizar placeholder", result);
                const NcmPlaceHolderId placeholder = placeholderFromContentId(entry.content_id);
                result = ncmContentStorageRegister(
                    &content_storage_, &entry.content_id, &placeholder);
                if (R_FAILED(result)) return failResult("no se pudo registrar NCA", result);
                entry_placeholder_open_ = false;
                newly_registered_.push_back(entry.content_id);
            }
            if (entry.cnmt) cnmt_ids_.push_back(entry.content_id);
            appendInstallLog(
                "ENTRY end file=\"%s\" name=\"%s\" cnmt=%u skipped=%u",
                name_.c_str(),
                entry.name.c_str(),
                entry.cnmt ? 1u : 0u,
                skip_active_nca_ ? 1u : 0u
            );
        } else if (entry.ticket) {
            tickets_[entry.name.substr(0, entry.name.size() - 4)] = sidecar_;
        } else if (entry.certificate) {
            certificates_[entry.name.substr(0, entry.name.size() - 5)] = sidecar_;
        }
        sidecar_.clear();
        return true;
    }

    bool readCnmt(const NcmContentId& id, std::vector<uint8_t>& output) {
        char path[FS_MAX_PATH] {};
        Result result = ncmContentStorageGetPath(
            &content_storage_, path, sizeof(path), &id);
        if (R_FAILED(result)) return failResult("no se encontro ruta CNMT", result);

        FsFileSystem filesystem {};
        result = fsOpenFileSystemWithId(
            &filesystem, 0, FsFileSystemType_ContentMeta, path, FsContentAttributes_All);
        if (R_FAILED(result)) return failResult("no se pudo abrir filesystem CNMT", result);

        FsDir directory {};
        result = fsFsOpenDirectory(&filesystem, "/", FsDirOpenMode_ReadFiles, &directory);
        if (R_FAILED(result)) {
            fsFsClose(&filesystem);
            return failResult("no se pudo listar CNMT", result);
        }
        std::string cnmt_name;
        std::string cnmt_name_without_root;
        while (cnmt_name.empty()) {
            FsDirectoryEntry entries[8] {};
            int64_t count = 0;
            result = fsDirRead(&directory, &count, 8, entries);
            if (R_FAILED(result) || count == 0) break;
            for (int64_t index = 0; index < count; ++index) {
                const std::string candidate(entries[index].name);
                appendInstallLog(
                    "CNMT dir file=\"%s\" candidate=\"%s\"",
                    name_.c_str(),
                    candidate.c_str()
                );
                if (endsWith(lower(candidate), ".cnmt")) {
                    cnmt_name = "/" + candidate;
                    cnmt_name_without_root = candidate;
                    break;
                }
            }
        }
        fsDirClose(&directory);
        if (cnmt_name.empty()) {
            fsFsClose(&filesystem);
            return fail("CNMT interno no encontrado");
        }

        FsFile file {};
        result = fsFsOpenFile(&filesystem, cnmt_name.c_str(), FsOpenMode_Read, &file);
        if (R_FAILED(result) && !cnmt_name_without_root.empty()) {
            appendInstallLog(
                "CNMT open retry file=\"%s\" first=\"%s\" result=0x%08X retry=\"%s\"",
                name_.c_str(),
                cnmt_name.c_str(),
                static_cast<unsigned int>(result),
                cnmt_name_without_root.c_str()
            );
            result = fsFsOpenFile(
                &filesystem, cnmt_name_without_root.c_str(), FsOpenMode_Read, &file);
            if (R_SUCCEEDED(result)) {
                cnmt_name = cnmt_name_without_root;
            }
        }
        if (R_FAILED(result)) {
            char message[72];
            std::snprintf(
                message,
                sizeof(message),
                "abrir CNMT %.32s",
                cnmt_name.c_str()
            );
            fsFsClose(&filesystem);
            return failResult(message, result);
        }
        int64_t size = 0;
        result = fsFileGetSize(&file, &size);
        if (R_FAILED(result) || size < static_cast<int64_t>(sizeof(PackagedContentMetaHeader)) ||
            size > static_cast<int64_t>(kMaxHeaderSize)) {
            fsFileClose(&file);
            fsFsClose(&filesystem);
            return fail("tamano CNMT invalido");
        }
        output.resize(static_cast<size_t>(size));
        uint64_t read = 0;
        result = fsFileRead(&file, 0, output.data(), output.size(), FsReadOption_None, &read);
        fsFileClose(&file);
        fsFsClose(&filesystem);
        if (R_FAILED(result) || read != output.size()) {
            return failResult("lectura CNMT incompleta", result);
        }
        return true;
    }

    bool buildInstallMeta(
        const std::vector<uint8_t>& packaged,
        const NcmContentId& cnmt_id,
        std::vector<uint8_t>& output,
        NcmContentMetaKey& key
    ) {
        const auto* header = reinterpret_cast<const PackagedContentMetaHeader*>(packaged.data());
        if (header->type != NcmContentMetaType_Application &&
            header->type != NcmContentMetaType_Patch &&
            header->type != NcmContentMetaType_AddOnContent) {
            return fail("tipo de contenido no admitido en esta version");
        }
        const uint64_t records_offset = sizeof(*header) + header->extended_header_size;
        const uint64_t records_size =
            static_cast<uint64_t>(header->content_count) * sizeof(NcmPackagedContentInfo);
        const uint64_t meta_info_size =
            static_cast<uint64_t>(header->content_meta_count) * sizeof(NcmContentMetaInfo);
        if (records_offset + records_size + meta_info_size > packaged.size()) {
            return fail("estructura CNMT fuera de limites");
        }

        const auto* content = reinterpret_cast<const NcmPackagedContentInfo*>(
            packaged.data() + records_offset);
        std::vector<NcmContentInfo> selected;
        for (uint16_t index = 0; index < header->content_count; ++index) {
            if (content[index].info.content_type > NcmContentType_LegalInformation) continue;
            bool present = false;
            Result result = ncmContentStorageHas(
                &content_storage_, &present, &content[index].info.content_id);
            if (R_FAILED(result) || !present) return fail("falta una NCA requerida por CNMT");
            int64_t installed_size = 0;
            result = ncmContentStorageGetSizeFromContentId(
                &content_storage_, &installed_size, &content[index].info.content_id);
            uint64_t declared_size = 0;
            ncmContentInfoSizeToU64(&content[index].info, &declared_size);
            if (R_FAILED(result) || installed_size != static_cast<int64_t>(declared_size)) {
                return fail("tamano NCA no coincide con CNMT");
            }
            selected.push_back(content[index].info);
        }

        NcmContentMetaHeader install_header {};
        install_header.extended_header_size = header->extended_header_size;
        install_header.content_count = static_cast<uint16_t>(selected.size() + 1);
        install_header.content_meta_count = header->content_meta_count;
        install_header.attributes = header->attributes;
        install_header.storage_id = NcmStorageId_None;
        appendBytes(output, &install_header, sizeof(install_header));
        appendBytes(output, packaged.data() + sizeof(*header), header->extended_header_size);

        NcmContentInfo cnmt_info {};
        cnmt_info.content_id = cnmt_id;
        ncmU64ToContentInfoSize(findEntrySize(cnmt_id), &cnmt_info);
        cnmt_info.content_type = NcmContentType_Meta;
        appendBytes(output, &cnmt_info, sizeof(cnmt_info));
        for (const NcmContentInfo& info : selected) appendBytes(output, &info, sizeof(info));

        const uint8_t* meta_info = packaged.data() + records_offset + records_size;
        appendBytes(output, meta_info, static_cast<size_t>(meta_info_size));

        size_t extended_data_size = 0;
        if (header->type == NcmContentMetaType_Patch &&
            header->extended_header_size >= sizeof(NcmPatchMetaExtendedHeader)) {
            const auto* extended = reinterpret_cast<const NcmPatchMetaExtendedHeader*>(
                packaged.data() + sizeof(*header));
            extended_data_size = extended->extended_data_size;
        }
        const uint64_t extended_data_offset = records_offset + records_size + meta_info_size;
        if (extended_data_offset + extended_data_size > packaged.size()) {
            return fail("datos extendidos CNMT fuera de limites");
        }
        appendBytes(output, packaged.data() + extended_data_offset, extended_data_size);

        key = {};
        key.id = header->title_id;
        key.version = header->version;
        key.type = header->type;
        key.install_type = header->install_type;
        return true;
    }

    uint64_t findEntrySize(const NcmContentId& id) const {
        for (const Entry& entry : entries_) {
            if (entry.nca && std::memcmp(&entry.content_id, &id, sizeof(id)) == 0) {
                return entry.size;
            }
        }
        return 0;
    }

    bool importTickets() {
        if (tickets_.size() != certificates_.size()) {
            return fail("cantidad de tickets y certificados no coincide");
        }
        if (tickets_.empty()) return true;
        Result result = smGetService(&es_service_, "es");
        if (R_FAILED(result)) return failResult("no se pudo abrir ES", result);
        es_open_ = true;
        for (const auto& [stem, ticket] : tickets_) {
            const auto certificate = certificates_.find(stem);
            if (certificate == certificates_.end()) return fail("certificado de ticket ausente");
            result = serviceDispatch(&es_service_, 1,
                .buffer_attrs = {
                    SfBufferAttr_HipcMapAlias | SfBufferAttr_In,
                    SfBufferAttr_HipcMapAlias | SfBufferAttr_In,
                },
                .buffers = {
                    { ticket.data(), ticket.size() },
                    { certificate->second.data(), certificate->second.size() },
                },
            );
            if (R_FAILED(result)) return failResult("ES rechazo el ticket", result);
        }
        return true;
    }

    bool installMetadata(const NcmContentId& cnmt_id) {
        std::vector<uint8_t> packaged;
        if (!readCnmt(cnmt_id, packaged)) return false;

        std::vector<uint8_t> install_meta;
        NcmContentMetaKey key {};
        if (!buildInstallMeta(packaged, cnmt_id, install_meta, key)) return false;

        NcmContentMetaDatabase database {};
        Result result = ncmOpenContentMetaDatabase(&database, NcmStorageId_SdCard);
        if (R_FAILED(result)) return failResult("no se pudo abrir ContentMetaDatabase", result);
        result = ncmContentMetaDatabaseSet(
            &database, &key, install_meta.data(), install_meta.size());
        if (R_SUCCEEDED(result)) result = ncmContentMetaDatabaseCommit(&database);
        ncmContentMetaDatabaseClose(&database);
        if (R_FAILED(result)) return failResult("no se pudo confirmar ContentMeta", result);
        metadata_keys_.push_back(key);

        const auto* header = reinterpret_cast<const PackagedContentMetaHeader*>(packaged.data());
        ApplicationRecordStorage record {key, NcmStorageId_SdCard};
        struct {
            uint8_t event;
            uint8_t padding[7];
            uint64_t application_id;
        } input {3, {}, baseApplicationId(header->title_id, header->type)};
        result = serviceDispatchIn(&application_manager_, 16, input,
            .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_In },
            .buffers = { { &record, sizeof(record) } },
        );
        if (R_FAILED(result)) return failResult("no se pudo publicar ApplicationRecord", result);
        return true;
    }

    void rollback() {
        if (!content_storage_open_) return;
        for (auto key = metadata_keys_.rbegin(); key != metadata_keys_.rend(); ++key) {
            NcmContentMetaDatabase database {};
            if (R_SUCCEEDED(ncmOpenContentMetaDatabase(&database, NcmStorageId_SdCard))) {
                if (R_SUCCEEDED(ncmContentMetaDatabaseRemove(&database, &*key))) {
                    ncmContentMetaDatabaseCommit(&database);
                }
                ncmContentMetaDatabaseClose(&database);
            }
        }
        metadata_keys_.clear();
        for (auto entry = newly_registered_.rbegin(); entry != newly_registered_.rend(); ++entry) {
            ncmContentStorageDelete(&content_storage_, &*entry);
        }
        newly_registered_.clear();
    }

    void closeServices() {
        if (application_manager_open_) serviceClose(&application_manager_);
        if (ns_initialized_) nsExit();
        if (es_open_) serviceClose(&es_service_);
        if (spl_crypto_initialized_) splCryptoExit();
        if (spl_initialized_) splExit();
        if (content_storage_open_) ncmContentStorageClose(&content_storage_);
        if (ncm_initialized_) ncmExit();
        application_manager_open_ = false;
        ns_initialized_ = false;
        es_open_ = false;
        spl_crypto_initialized_ = false;
        spl_initialized_ = false;
        content_storage_open_ = false;
        ncm_initialized_ = false;
    }

    std::string name_;
    uint64_t expected_size_;
    PackageType package_type_;
    uint64_t received_ = 0;
    uint64_t data_position_ = 0;
    size_t header_target_ = 0;
    size_t current_entry_ = 0;
    std::vector<uint8_t> header_;
    std::vector<uint8_t> xci_root_header_;
    std::vector<uint8_t> xci_secure_header_;
    std::vector<uint8_t> nca_header_buffer_;
    std::array<uint8_t, 0x20> header_key_ {};
    uint64_t xci_cursor_ = 0;
    uint64_t xci_secure_offset_ = 0;
    uint64_t xci_secure_size_ = 0;
    size_t xci_root_target_ = 0;
    size_t xci_secure_target_ = 0;
    std::vector<Entry> entries_;
    std::vector<uint8_t> sidecar_;
    std::map<std::string, std::vector<uint8_t>> tickets_;
    std::map<std::string, std::vector<uint8_t>> certificates_;
    std::vector<NcmContentId> cnmt_ids_;
    std::vector<NcmContentId> newly_registered_;
    NcmContentId active_content_id_ {};
    std::vector<NcmContentMetaKey> metadata_keys_;
    std::string detail_;
    NcmContentStorage content_storage_ {};
    Service application_manager_ {};
    Service es_service_ {};
    bool header_parsed_ = false;
    bool entry_started_ = false;
    bool entry_placeholder_open_ = false;
    bool skip_active_nca_ = false;
    bool ncm_initialized_ = false;
    bool content_storage_open_ = false;
    bool ns_initialized_ = false;
    bool application_manager_open_ = false;
    bool es_open_ = false;
    bool spl_crypto_initialized_ = false;
    bool spl_initialized_ = false;
    bool failed_ = false;
    bool finished_ = false;
    bool aborted_ = false;
};

}  // namespace

SdInstallReceiverFactory::SdInstallReceiverFactory(uint32_t storage_id)
    : storage_id_(storage_id), detail_("listo para NSP/XCI") {
}

bool SdInstallReceiverFactory::handles(uint32_t storage_id) const {
    return storage_id == storage_id_;
}

std::unique_ptr<IncomingObjectSink> SdInstallReceiverFactory::open(
    uint32_t storage_id,
    const char* path,
    uint64_t expected_size
) {
    // Resetear siempre antes de evaluar el nuevo archivo para que un fallo anterior
    // no contamine el mensaje cuando este archivo es rechazado en una fase temprana.
    detail_ = "evaluando archivo";

    if (!handles(storage_id) || path == nullptr) {
        detail_ = "storage de instalacion invalido";
        appendInstallLog("REJECT path=\"%s\" storage=0x%08X detail=\"%s\"",
            path ? path : "(null)", storage_id, detail_.c_str());
        return nullptr;
    }
    const std::string full_path(path);
    const size_t separator = full_path.find_last_of('/');
    const std::string name = separator == std::string::npos
        ? full_path
        : full_path.substr(separator + 1);
    const std::string normalized = lower(name);
    const bool is_nsp = endsWith(normalized, ".nsp");
    const bool is_xci = endsWith(normalized, ".xci");
    if (!is_nsp && !is_xci) {
        detail_ = "solo NSP o XCI sin compresion en esta version";
        appendInstallLog(
            "REJECT path=\"%s\" expected=%llu detail=\"%s\"",
            full_path.c_str(),
            static_cast<unsigned long long>(expected_size),
            detail_.c_str()
        );
        return nullptr;
    }
    const PackageType package_type = is_xci ? PackageType::xci : PackageType::nsp;
    auto sink = std::make_unique<SdPackageInstallSink>(name, expected_size, package_type);
    if (!sink->initialize()) {
        detail_ = sink->detail();
        appendInstallLog(
            "REJECT init path=\"%s\" expected=%llu detail=\"%s\"",
            full_path.c_str(),
            static_cast<unsigned long long>(expected_size),
            detail_.c_str()
        );
        return nullptr;
    }
    detail_ = is_xci ? "receptor XCI preparado" : "receptor NSP preparado";
    return sink;
}

const char* SdInstallReceiverFactory::detail() const {
    return detail_.c_str();
}

}  // namespace transfer_switch
