#ifndef TRANSFER_SWITCH_ADAPTERS_READ_ONLY_SD_DATABASE_HPP
#define TRANSFER_SWITCH_ADAPTERS_READ_ONLY_SD_DATABASE_HPP

#include <map>
#include <set>
#include <string>
#include <vector>

#include "MtpDatabase.h"

namespace transfer_switch {

class ReadOnlySdDatabase final : public android::MtpDatabase {
public:
    explicit ReadOnlySdDatabase(std::string root_path, bool writable = false);

    bool isHandleValid(android::MtpObjectHandle handle) override;
    void addStoragePath(
        const android::MtpString& path,
        const android::MtpString& display_name,
        android::MtpStorageID storage,
        bool hidden
    ) override;
    void addStoragePathWithPolicy(
        const android::MtpString& path,
        const android::MtpString& display_name,
        android::MtpStorageID storage,
        bool hidden,
        bool writable
    );
    void addVirtualStorage(android::MtpStorageID storage);
    void addActionStorage(android::MtpStorageID storage, const std::string& path);
    void addVirtualRootDirectory(
        android::MtpStorageID storage,
        const std::string& display_name,
        const std::string& mounted_path
    );
    void removeStorage(android::MtpStorageID storage) override;

    android::MtpObjectHandle beginSendObject(
        const android::MtpString& path,
        android::MtpObjectFormat format,
        android::MtpObjectHandle parent,
        android::MtpStorageID storage,
        uint64_t size,
        time_t modified
    ) override;
    void endSendObject(
        const android::MtpString& path,
        android::MtpObjectHandle handle,
        android::MtpObjectFormat format,
        bool succeeded
    ) override;

    android::MtpObjectHandleList* getObjectList(
        android::MtpStorageID storage,
        android::MtpObjectFormat format,
        android::MtpObjectHandle parent
    ) override;
    int getNumObjects(
        android::MtpStorageID storage,
        android::MtpObjectFormat format,
        android::MtpObjectHandle parent
    ) override;

    android::MtpObjectFormatList* getSupportedPlaybackFormats() override;
    android::MtpObjectFormatList* getSupportedCaptureFormats() override;
    android::MtpObjectPropertyList* getSupportedObjectProperties(
        android::MtpObjectFormat format
    ) override;
    android::MtpDevicePropertyList* getSupportedDeviceProperties() override;

    android::MtpResponseCode getObjectPropertyValue(
        android::MtpObjectHandle handle,
        android::MtpObjectProperty property,
        android::MtpDataPacket& packet
    ) override;
    android::MtpResponseCode setObjectPropertyValue(
        android::MtpObjectHandle handle,
        android::MtpObjectProperty property,
        android::MtpDataPacket& packet
    ) override;
    android::MtpResponseCode getDevicePropertyValue(
        android::MtpDeviceProperty property,
        android::MtpDataPacket& packet
    ) override;
    android::MtpResponseCode setDevicePropertyValue(
        android::MtpDeviceProperty property,
        android::MtpDataPacket& packet
    ) override;
    android::MtpResponseCode resetDeviceProperty(android::MtpDeviceProperty property) override;
    android::MtpResponseCode getObjectPropertyList(
        android::MtpObjectHandle handle,
        uint32_t format,
        uint32_t property,
        int group_code,
        int depth,
        android::MtpDataPacket& packet
    ) override;

    android::MtpResponseCode getObjectInfo(
        android::MtpObjectHandle handle,
        android::MtpObjectInfo& info
    ) override;
    void* getThumbnail(android::MtpObjectHandle handle, size_t& thumbnail_size) override;
    android::MtpResponseCode getObjectFilePath(
        android::MtpObjectHandle handle,
        android::MtpString& path,
        int64_t& length,
        android::MtpObjectFormat& format
    ) override;

    android::MtpResponseCode deleteFile(android::MtpObjectHandle handle) override;
    android::MtpResponseCode moveFile(
        android::MtpObjectHandle handle,
        android::MtpObjectHandle new_parent
    ) override;
    android::MtpObjectHandleList* getObjectReferences(
        android::MtpObjectHandle handle
    ) override;
    android::MtpResponseCode setObjectReferences(
        android::MtpObjectHandle handle,
        android::MtpObjectHandleList* references
    ) override;
    android::MtpProperty* getObjectPropertyDesc(
        android::MtpObjectProperty property,
        android::MtpObjectFormat format
    ) override;
    android::MtpProperty* getDevicePropertyDesc(
        android::MtpDeviceProperty property
    ) override;
    void sessionStarted(android::MtpServer* server) override;
    void sessionEnded() override;

private:
    struct Entry {
        android::MtpStorageID storage;
        android::MtpObjectHandle parent;
        android::MtpObjectFormat format;
        std::string name;
        std::string path;
        uint64_t size;
        time_t modified;
        bool scanned;
    };

    struct StorageRoot {
        std::string path;
        bool writable;
        bool scanned;
        bool virtual_root;
        bool action;
    };

    struct VirtualRootSeed {
        android::MtpStorageID storage;
        std::string name;
        std::string path;
    };

    bool scan(android::MtpStorageID storage, android::MtpObjectHandle parent);
    android::MtpObjectFormat detectFormat(const std::string& name, bool directory) const;
    static std::string normalizeRoot(std::string path);
    void addVirtualRootEntry(const VirtualRootSeed& seed);

    std::string initial_root_path_;
    bool initial_writable_;
    android::MtpObjectHandle next_handle_;
    std::map<android::MtpStorageID, StorageRoot> storage_roots_;
    std::vector<VirtualRootSeed> virtual_root_seeds_;
    std::map<android::MtpObjectHandle, Entry> entries_;
    std::set<android::MtpObjectHandle> pending_handles_;
};

}  // namespace transfer_switch

#endif
