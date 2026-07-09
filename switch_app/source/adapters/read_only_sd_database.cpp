#include "transfer_switch/adapters/read_only_sd_database.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <limits>
#include <sys/stat.h>

#include "MtpDataPacket.h"
#include "MtpObjectInfo.h"
#include "MtpProperty.h"
#include "mtp.h"
#include "transfer_switch/domain/safe_path.h"

namespace transfer_switch {

using namespace android;

ReadOnlySdDatabase::ReadOnlySdDatabase(std::string root_path, bool writable)
    : initial_root_path_(normalizeRoot(std::move(root_path))),
      initial_writable_(writable),
      next_handle_(1) {
}

std::string ReadOnlySdDatabase::normalizeRoot(std::string path) {
    if (!path.empty() && path.back() != '/') {
        path += '/';
    }
    return path;
}

bool ReadOnlySdDatabase::isHandleValid(MtpObjectHandle handle) {
    return entries_.find(handle) != entries_.end();
}

void ReadOnlySdDatabase::addStoragePath(
    const MtpString& path,
    const MtpString& display_name,
    MtpStorageID storage,
    bool hidden
) {
    const bool writable = storage_roots_.empty() &&
        normalizeRoot(path.c_str()) == initial_root_path_ && initial_writable_;
    addStoragePathWithPolicy(path, display_name, storage, hidden, writable);
}

void ReadOnlySdDatabase::addStoragePathWithPolicy(
    const MtpString& path,
    const MtpString& display_name,
    MtpStorageID storage,
    bool hidden,
    bool writable
) {
    (void)display_name;
    (void)hidden;
    for (auto entry = entries_.begin(); entry != entries_.end();) {
        if (entry->second.storage == storage) {
            pending_handles_.erase(entry->first);
            entry = entries_.erase(entry);
        } else {
            ++entry;
        }
    }
    storage_roots_.insert_or_assign(storage, StorageRoot {
        normalizeRoot(path.c_str()),
        writable,
        false,
        false,
        false,
    });
}

void ReadOnlySdDatabase::addVirtualStorage(MtpStorageID storage) {
    storage_roots_.insert_or_assign(storage, StorageRoot {
        "",
        false,
        true,
        true,
        false,
    });
}

void ReadOnlySdDatabase::addActionStorage(MtpStorageID storage, const std::string& path) {
    storage_roots_.insert_or_assign(storage, StorageRoot {
        normalizeRoot(path),
        true,
        true,
        false,
        true,
    });
}

void ReadOnlySdDatabase::addVirtualRootEntry(const VirtualRootSeed& seed) {
    entries_.emplace(next_handle_++, Entry {
        seed.storage,
        0,
        MTP_FORMAT_ASSOCIATION,
        seed.name,
        normalizeRoot(seed.path),
        0,
        0,
        false,
    });
}

void ReadOnlySdDatabase::addVirtualRootDirectory(
    MtpStorageID storage,
    const std::string& display_name,
    const std::string& mounted_path
) {
    auto root = storage_roots_.find(storage);
    if (root == storage_roots_.end() || !root->second.virtual_root ||
        display_name.empty() || mounted_path.empty()) {
        return;
    }
    VirtualRootSeed seed {storage, display_name, mounted_path};
    virtual_root_seeds_.push_back(seed);
    addVirtualRootEntry(seed);
}

void ReadOnlySdDatabase::removeStorage(MtpStorageID storage) {
    storage_roots_.erase(storage);
    virtual_root_seeds_.erase(
        std::remove_if(
            virtual_root_seeds_.begin(),
            virtual_root_seeds_.end(),
            [storage](const VirtualRootSeed& seed) { return seed.storage == storage; }
        ),
        virtual_root_seeds_.end()
    );
    for (auto entry = entries_.begin(); entry != entries_.end();) {
        if (entry->second.storage == storage) {
            pending_handles_.erase(entry->first);
            entry = entries_.erase(entry);
        } else {
            ++entry;
        }
    }
}

MtpObjectHandle ReadOnlySdDatabase::beginSendObject(
    const MtpString& path,
    MtpObjectFormat format,
    MtpObjectHandle parent,
    MtpStorageID storage,
    uint64_t size,
    time_t modified
) {
    auto storage_root = storage_roots_.find(storage);
    if (storage_root == storage_roots_.end() || !storage_root->second.writable) {
        return kInvalidObjectHandle;
    }
    if (storage_root->second.action && format == MTP_FORMAT_ASSOCIATION) {
        return kInvalidObjectHandle;
    }

    std::string parent_path;
    if (parent == 0) {
        parent_path = storage_root->second.path;
    } else {
        auto parent_entry = entries_.find(parent);
        if (parent_entry == entries_.end() ||
            parent_entry->second.storage != storage ||
            parent_entry->second.format != MTP_FORMAT_ASSOCIATION) {
            return kInvalidObjectHandle;
        }
        parent_path = parent_entry->second.path;
    }
    if (!ts_is_safe_direct_child(
            storage_root->second.path.c_str(), parent_path.c_str(), path.c_str())) {
        return kInvalidObjectHandle;
    }

    struct stat existing {};
    if (stat(path.c_str(), &existing) == 0) {
        return kInvalidObjectHandle;
    }
    for (const auto& [handle, entry] : entries_) {
        (void)handle;
        if (entry.path == path.c_str()) {
            return kInvalidObjectHandle;
        }
    }

    const std::string full_path(path.c_str());
    const size_t separator = full_path.find_last_of('/');
    const std::string name = separator == std::string::npos
        ? full_path
        : full_path.substr(separator + 1);
    const MtpObjectHandle handle = next_handle_++;
    entries_.emplace(handle, Entry {
        storage,
        parent,
        format,
        name,
        full_path,
        size,
        modified,
        false,
    });
    pending_handles_.insert(handle);
    return handle;
}

void ReadOnlySdDatabase::endSendObject(
    const MtpString& path,
    MtpObjectHandle handle,
    MtpObjectFormat format,
    bool succeeded
) {
    (void)path;
    (void)format;
    auto entry = entries_.find(handle);
    if (entry == entries_.end() || pending_handles_.erase(handle) == 0) {
        return;
    }
    if (!succeeded) {
        entries_.erase(entry);
        return;
    }
    const auto root = storage_roots_.find(entry->second.storage);
    if (root != storage_roots_.end() && root->second.action) {
        // Keep the completed virtual object alive until SessionEnded. Windows asks
        // for its ObjectInfo after SendObject succeeds; removing the handle here
        // makes Explorer report a false device disconnection.
        entry->second.scanned = true;
        return;
    }
    struct stat information {};
    if (stat(entry->second.path.c_str(), &information) == 0) {
        entry->second.size = S_ISDIR(information.st_mode)
            ? 0u
            : static_cast<uint64_t>(information.st_size);
        entry->second.modified = information.st_mtime;
    }
}

MtpObjectFormat ReadOnlySdDatabase::detectFormat(const std::string& name, bool directory) const {
    if (directory) {
        return MTP_FORMAT_ASSOCIATION;
    }
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos) {
        return MTP_FORMAT_UNDEFINED;
    }
    std::string extension = name.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    if (extension == ".jpg" || extension == ".jpeg") {
        return MTP_FORMAT_EXIF_JPEG;
    }
    if (extension == ".png") {
        return MTP_FORMAT_PNG;
    }
    if (extension == ".txt" || extension == ".ini" || extension == ".log") {
        return MTP_FORMAT_TEXT;
    }
    if (extension == ".mp3") {
        return MTP_FORMAT_MP3;
    }
    if (extension == ".mp4") {
        return MTP_FORMAT_MP4_CONTAINER;
    }
    return MTP_FORMAT_UNDEFINED;
}

bool ReadOnlySdDatabase::scan(MtpStorageID storage, MtpObjectHandle parent) {
    std::string directory_path;
    if (parent == 0) {
        auto storage_root = storage_roots_.find(storage);
        if (storage_root == storage_roots_.end()) {
            return false;
        }
        if (storage_root->second.scanned) {
            return true;
        }
        if (storage_root->second.action) {
            storage_root->second.scanned = true;
            return true;
        }
        directory_path = storage_root->second.path;
    } else {
        auto found = entries_.find(parent);
        if (found == entries_.end() || found->second.storage != storage ||
            found->second.format != MTP_FORMAT_ASSOCIATION) {
            return false;
        }
        if (found->second.scanned) {
            return true;
        }
        directory_path = found->second.path;
    }

    DIR* directory = opendir(directory_path.c_str());
    if (directory == nullptr) {
        return false;
    }

    while (dirent* item = readdir(directory)) {
        const std::string name(item->d_name);
        if (name == "." || name == "..") {
            continue;
        }
        if (name == ".transferencia-switch-staging") {
            continue;
        }
        std::string path = directory_path;
        if (!path.empty() && path.back() != '/') {
            path += '/';
        }
        path += name;

        struct stat information {};
        if (stat(path.c_str(), &information) != 0) {
            continue;
        }
        const bool is_directory = S_ISDIR(information.st_mode);
        Entry entry {
            storage,
            parent,
            detectFormat(name, is_directory),
            name,
            path,
            is_directory ? 0u : static_cast<uint64_t>(information.st_size),
            information.st_mtime,
            false,
        };
        entries_.emplace(next_handle_++, std::move(entry));
    }
    closedir(directory);

    if (parent == 0) {
        storage_roots_.at(storage).scanned = true;
    } else {
        entries_.at(parent).scanned = true;
    }
    return true;
}

MtpObjectHandleList* ReadOnlySdDatabase::getObjectList(
    MtpStorageID storage,
    MtpObjectFormat format,
    MtpObjectHandle parent
) {
    if (parent == MTP_PARENT_ROOT) {
        parent = 0;
    }
    if (parent == 0 && (storage == 0 || storage == 0xFFFFFFFF)) {
        for (const auto& [storage_id, root] : storage_roots_) {
            (void)root;
            scan(storage_id, 0);
        }
    } else if (parent == 0) {
        if (!scan(storage, 0)) {
            return new MtpObjectHandleList();
        }
    } else {
        auto parent_entry = entries_.find(parent);
        if (parent_entry == entries_.end() ||
            !scan(parent_entry->second.storage, parent)) {
            return new MtpObjectHandleList();
        }
    }

    auto* result = new MtpObjectHandleList();
    for (const auto& [handle, entry] : entries_) {
        const bool storage_matches = storage == 0 || storage == 0xFFFFFFFF || entry.storage == storage;
        const bool format_matches = format == 0 || entry.format == format;
        if (storage_matches && format_matches && entry.parent == parent) {
            result->push_back(handle);
        }
    }
    return result;
}

int ReadOnlySdDatabase::getNumObjects(
    MtpStorageID storage,
    MtpObjectFormat format,
    MtpObjectHandle parent
) {
    MtpObjectHandleList* objects = getObjectList(storage, format, parent);
    const int count = static_cast<int>(objects->size());
    delete objects;
    return count;
}

MtpObjectFormatList* ReadOnlySdDatabase::getSupportedPlaybackFormats() {
    return new MtpObjectFormatList {
        MTP_FORMAT_UNDEFINED,
        MTP_FORMAT_ASSOCIATION,
        MTP_FORMAT_TEXT,
        MTP_FORMAT_EXIF_JPEG,
        MTP_FORMAT_PNG,
        MTP_FORMAT_MP3,
        MTP_FORMAT_MP4_CONTAINER,
    };
}

MtpObjectFormatList* ReadOnlySdDatabase::getSupportedCaptureFormats() {
    return new MtpObjectFormatList();
}

MtpObjectPropertyList* ReadOnlySdDatabase::getSupportedObjectProperties(MtpObjectFormat format) {
    (void)format;
    return new MtpObjectPropertyList();
}

MtpDevicePropertyList* ReadOnlySdDatabase::getSupportedDeviceProperties() {
    return new MtpDevicePropertyList();
}

MtpResponseCode ReadOnlySdDatabase::getObjectPropertyValue(
    MtpObjectHandle handle,
    MtpObjectProperty property,
    MtpDataPacket& packet
) {
    (void)handle;
    (void)property;
    (void)packet;
    return MTP_RESPONSE_OBJECT_PROP_NOT_SUPPORTED;
}

MtpResponseCode ReadOnlySdDatabase::setObjectPropertyValue(
    MtpObjectHandle handle,
    MtpObjectProperty property,
    MtpDataPacket& packet
) {
    (void)handle;
    (void)property;
    (void)packet;
    return MTP_RESPONSE_STORE_READ_ONLY;
}

MtpResponseCode ReadOnlySdDatabase::getDevicePropertyValue(
    MtpDeviceProperty property,
    MtpDataPacket& packet
) {
    (void)property;
    (void)packet;
    return MTP_RESPONSE_DEVICE_PROP_NOT_SUPPORTED;
}

MtpResponseCode ReadOnlySdDatabase::setDevicePropertyValue(
    MtpDeviceProperty property,
    MtpDataPacket& packet
) {
    (void)property;
    (void)packet;
    return MTP_RESPONSE_DEVICE_PROP_NOT_SUPPORTED;
}

MtpResponseCode ReadOnlySdDatabase::resetDeviceProperty(MtpDeviceProperty property) {
    (void)property;
    return MTP_RESPONSE_DEVICE_PROP_NOT_SUPPORTED;
}

MtpResponseCode ReadOnlySdDatabase::getObjectPropertyList(
    MtpObjectHandle handle,
    uint32_t format,
    uint32_t property,
    int group_code,
    int depth,
    MtpDataPacket& packet
) {
    (void)handle;
    (void)format;
    (void)property;
    (void)group_code;
    (void)depth;
    (void)packet;
    return MTP_RESPONSE_OPERATION_NOT_SUPPORTED;
}

MtpResponseCode ReadOnlySdDatabase::getObjectInfo(MtpObjectHandle handle, MtpObjectInfo& info) {
    auto found = entries_.find(handle);
    if (found == entries_.end()) {
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    }
    const Entry& entry = found->second;
    info.mHandle = handle;
    info.mStorageID = entry.storage;
    info.mFormat = entry.format;
    info.mProtectionStatus = 0x0001;
    info.mCompressedSize = entry.size > std::numeric_limits<uint32_t>::max()
        ? std::numeric_limits<uint32_t>::max()
        : static_cast<uint32_t>(entry.size);
    info.mThumbFormat = 0;
    info.mThumbCompressedSize = 0;
    info.mThumbPixWidth = 0;
    info.mThumbPixHeight = 0;
    info.mImagePixWidth = 0;
    info.mImagePixHeight = 0;
    info.mImagePixDepth = 0;
    info.mParent = entry.parent;
    info.mAssociationType = entry.format == MTP_FORMAT_ASSOCIATION
        ? MTP_ASSOCIATION_TYPE_GENERIC_FOLDER
        : 0;
    info.mAssociationDesc = 0;
    info.mSequenceNumber = 0;
    info.mName = ::strdup(entry.name.c_str());
    info.mDateCreated = entry.modified;
    info.mDateModified = entry.modified;
    info.mKeywords = ::strdup("");
    return MTP_RESPONSE_OK;
}

void* ReadOnlySdDatabase::getThumbnail(MtpObjectHandle handle, size_t& thumbnail_size) {
    (void)handle;
    thumbnail_size = 0;
    return nullptr;
}

MtpResponseCode ReadOnlySdDatabase::getObjectFilePath(
    MtpObjectHandle handle,
    MtpString& path,
    int64_t& length,
    MtpObjectFormat& format
) {
    auto found = entries_.find(handle);
    if (found == entries_.end()) {
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    }
    path = found->second.path;
    length = static_cast<int64_t>(found->second.size);
    format = found->second.format;
    return MTP_RESPONSE_OK;
}

MtpResponseCode ReadOnlySdDatabase::deleteFile(MtpObjectHandle handle) {
    (void)handle;
    return MTP_RESPONSE_STORE_READ_ONLY;
}

MtpResponseCode ReadOnlySdDatabase::moveFile(MtpObjectHandle handle, MtpObjectHandle new_parent) {
    (void)handle;
    (void)new_parent;
    return MTP_RESPONSE_STORE_READ_ONLY;
}

MtpObjectHandleList* ReadOnlySdDatabase::getObjectReferences(MtpObjectHandle handle) {
    (void)handle;
    return new MtpObjectHandleList();
}

MtpResponseCode ReadOnlySdDatabase::setObjectReferences(
    MtpObjectHandle handle,
    MtpObjectHandleList* references
) {
    (void)handle;
    (void)references;
    return MTP_RESPONSE_STORE_READ_ONLY;
}

MtpProperty* ReadOnlySdDatabase::getObjectPropertyDesc(
    MtpObjectProperty property,
    MtpObjectFormat format
) {
    (void)property;
    (void)format;
    return nullptr;
}

MtpProperty* ReadOnlySdDatabase::getDevicePropertyDesc(MtpDeviceProperty property) {
    (void)property;
    return nullptr;
}

void ReadOnlySdDatabase::sessionStarted(MtpServer* server) {
    (void)server;
    entries_.clear();
    pending_handles_.clear();
    next_handle_ = 1;
    for (auto& [storage, root] : storage_roots_) {
        (void)storage;
        root.scanned = root.virtual_root;
    }
    for (const VirtualRootSeed& seed : virtual_root_seeds_) {
        addVirtualRootEntry(seed);
    }
}

void ReadOnlySdDatabase::sessionEnded() {
    for (auto entry = entries_.begin(); entry != entries_.end();) {
        const auto root = storage_roots_.find(entry->second.storage);
        if (root != storage_roots_.end() && root->second.action) {
            pending_handles_.erase(entry->first);
            entry = entries_.erase(entry);
        } else {
            ++entry;
        }
    }
}

}  // namespace transfer_switch
