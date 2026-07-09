#include "transfer_switch/adapters/installed_catalog.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <new>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace transfer_switch {
namespace {

bool ensureDirectory(const char* path) {
    return mkdir(path, 0777) == 0 || errno == EEXIST;
}

bool removeTree(const std::string& path) {
    DIR* directory = opendir(path.c_str());
    if (directory == nullptr) {
        return errno == ENOENT;
    }
    while (dirent* item = readdir(directory)) {
        if (std::strcmp(item->d_name, ".") == 0 || std::strcmp(item->d_name, "..") == 0) {
            continue;
        }
        const std::string child = path + "/" + item->d_name;
        struct stat information {};
        if (lstat(child.c_str(), &information) != 0) {
            closedir(directory);
            return false;
        }
        if (S_ISDIR(information.st_mode)) {
            if (!removeTree(child)) {
                closedir(directory);
                return false;
            }
        } else if (unlink(child.c_str()) != 0) {
            closedir(directory);
            return false;
        }
    }
    closedir(directory);
    return rmdir(path.c_str()) == 0;
}

bool writeFileAtomic(const std::string& path, const void* data, size_t size) {
    const std::string temporary = path + ".partial";
    unlink(temporary.c_str());
    const int descriptor = open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (descriptor < 0) {
        return false;
    }
    const unsigned char* source = static_cast<const unsigned char*>(data);
    size_t written = 0;
    while (written < size) {
        const ssize_t current = write(descriptor, source + written, size - written);
        if (current <= 0) {
            close(descriptor);
            unlink(temporary.c_str());
            return false;
        }
        written += static_cast<size_t>(current);
    }
    const bool synced = fsync(descriptor) == 0;
    const bool closed = close(descriptor) == 0;
    if (!synced || !closed || rename(temporary.c_str(), path.c_str()) != 0) {
        unlink(temporary.c_str());
        return false;
    }
    return true;
}

std::string sanitizeName(const char* source, u64 application_id) {
    std::string result = source == nullptr ? "" : source;
    for (char& value : result) {
        const unsigned char byte = static_cast<unsigned char>(value);
        if (byte < 0x20 || value == '/' || value == '\\' || value == ':' || value == '*' ||
            value == '?' || value == '"' || value == '<' || value == '>' || value == '|') {
            value = '_';
        }
    }
    while (!result.empty() && (result.back() == ' ' || result.back() == '.')) {
        result.pop_back();
    }
    if (result.empty()) {
        result = "Application";
    }
    if (result.size() > 72) {
        result.resize(72);
    }
    char suffix[32];
    std::snprintf(
        suffix,
        sizeof(suffix),
        " [%016llX]",
        static_cast<unsigned long long>(application_id)
    );
    result += suffix;
    return result;
}

const char* storageName(u8 storage) {
    switch (storage) {
        case NcmStorageId_GameCard: return "GameCard";
        case NcmStorageId_BuiltInSystem: return "BuiltInSystem";
        case NcmStorageId_BuiltInUser: return "NAND";
        case NcmStorageId_SdCard: return "SD Card";
        default: return "Unknown";
    }
}

const char* metaTypeName(u8 type) {
    switch (type) {
        case NcmContentMetaType_Application: return "Application";
        case NcmContentMetaType_Patch: return "Patch";
        case NcmContentMetaType_AddOnContent: return "AddOnContent";
        case NcmContentMetaType_Delta: return "Delta";
        case NcmContentMetaType_DataPatch: return "DataPatch";
        default: return "Other";
    }
}

}  // namespace

InstalledCatalog::InstalledCatalog()
    : application_count_(0),
      metadata_failure_count_(0),
      result_(0) {
}

bool InstalledCatalog::generate() {
    application_count_ = 0;
    metadata_failure_count_ = 0;
    result_ = 0;

    removeTree(rootPath());
    if (!ensureDirectory("sdmc:/switch") ||
        !ensureDirectory("sdmc:/switch/transferencia-switch") ||
        !ensureDirectory("sdmc:/switch/transferencia-switch/cache") ||
        !ensureDirectory(rootPath())) {
        result_ = 0xFFFFFFFF;
        return false;
    }

    result_ = nsInitialize();
    if (R_FAILED(result_)) {
        return false;
    }

    NsApplicationControlData* control = new (std::nothrow) NsApplicationControlData();
    if (control == nullptr) {
        nsExit();
        result_ = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
        return false;
    }

    s32 offset = 0;
    while (R_SUCCEEDED(result_)) {
        NsApplicationRecord records[32] {};
        s32 record_count = 0;
        result_ = nsListApplicationRecord(records, 32, offset, &record_count);
        if (R_FAILED(result_) || record_count == 0) {
            break;
        }

        for (s32 index = 0; index < record_count; ++index) {
            const NsApplicationRecord& record = records[index];
            std::memset(control, 0, sizeof(*control));
            u64 actual_size = 0;
            const Result control_result = nsGetApplicationControlData(
                NsApplicationControlSource_Storage,
                record.application_id,
                control,
                sizeof(*control),
                &actual_size
            );

            char title[0x201] {};
            char display_version[0x11] {};
            if (R_SUCCEEDED(control_result) && actual_size >= sizeof(control->nacp)) {
                NacpLanguageEntry* language = nullptr;
                if (R_SUCCEEDED(nacpGetLanguageEntry(&control->nacp, &language)) && language != nullptr) {
                    std::strncpy(title, language->name, sizeof(title) - 1);
                }
                std::memcpy(display_version, control->nacp.display_version, 0x10);
            } else {
                ++metadata_failure_count_;
            }

            const std::string directory_name = sanitizeName(title, record.application_id);
            const std::string directory_path = std::string(rootPath()) + directory_name;
            if (!ensureDirectory(directory_path.c_str())) {
                ++metadata_failure_count_;
                continue;
            }

            std::string information;
            char line[256];
            std::snprintf(line, sizeof(line), "Name: %s\n", title[0] == '\0' ? "Unknown" : title);
            information += line;
            std::snprintf(
                line,
                sizeof(line),
                "Application ID: %016llX\nDisplay version: %s\nLast updated: %llu\nControl result: 0x%08X\n\nContent records:\n",
                static_cast<unsigned long long>(record.application_id),
                display_version[0] == '\0' ? "Unknown" : display_version,
                static_cast<unsigned long long>(record.last_updated),
                control_result
            );
            information += line;

            s32 meta_offset = 0;
            while (true) {
                NsApplicationContentMetaStatus statuses[16] {};
                s32 status_count = 0;
                const Result status_result = nsListApplicationContentMetaStatus(
                    record.application_id,
                    meta_offset,
                    statuses,
                    16,
                    &status_count
                );
                if (R_FAILED(status_result) || status_count == 0) {
                    if (R_FAILED(status_result)) {
                        std::snprintf(line, sizeof(line), "- metadata error: 0x%08X\n", status_result);
                        information += line;
                    }
                    break;
                }
                for (s32 status_index = 0; status_index < status_count; ++status_index) {
                    const NsApplicationContentMetaStatus& status = statuses[status_index];
                    std::snprintf(
                        line,
                        sizeof(line),
                        "- %s | storage=%s | version=%u | id=%016llX\n",
                        metaTypeName(status.meta_type),
                        storageName(status.storageID),
                        status.version,
                        static_cast<unsigned long long>(status.application_id)
                    );
                    information += line;
                }
                meta_offset += status_count;
                if (status_count < 16) {
                    break;
                }
            }

            if (!writeFileAtomic(directory_path + "/info.txt", information.data(), information.size())) {
                ++metadata_failure_count_;
            }
            if (R_SUCCEEDED(control_result) && actual_size > sizeof(control->nacp)) {
                const size_t icon_size = static_cast<size_t>(actual_size - sizeof(control->nacp));
                if (icon_size <= sizeof(control->icon) &&
                    !writeFileAtomic(directory_path + "/icon.jpg", control->icon, icon_size)) {
                    ++metadata_failure_count_;
                }
            }
            ++application_count_;
        }
        offset += record_count;
        if (record_count < 32) {
            break;
        }
    }

    delete control;
    nsExit();

    char summary[256];
    const int summary_size = std::snprintf(
        summary,
        sizeof(summary),
        "Transferencia Switch - Installed games\nApplications: %u\nMetadata failures: %u\nResult: 0x%08X\nRead-only catalog. No installed content is modified or extracted.\n",
        static_cast<unsigned int>(application_count_),
        static_cast<unsigned int>(metadata_failure_count_),
        result_
    );
    if (summary_size > 0) {
        writeFileAtomic(std::string(rootPath()) + "catalog.txt", summary, static_cast<size_t>(summary_size));
    }
    return R_SUCCEEDED(result_);
}

size_t InstalledCatalog::applicationCount() const {
    return application_count_;
}

size_t InstalledCatalog::metadataFailureCount() const {
    return metadata_failure_count_;
}

Result InstalledCatalog::result() const {
    return result_;
}

}  // namespace transfer_switch
