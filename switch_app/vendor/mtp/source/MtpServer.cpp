/*
 * Copyright (C) 2010 The Android Open Source Project
 * Modified 2026 by Transferencia Switch contributors: read-only operation set and
 * device identity.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iomanip>
#include <iterator>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <malloc.h>

#define LOG_TAG "MtpServer"

#include "MtpDebug.h"
#include "MtpDatabase.h"
#include "MtpObjectInfo.h"
#include "MtpProperty.h"
#include "MtpServer.h"
#include "MtpStorage.h"
#include "MtpStringBuffer.h"

#include "log.h"

namespace android {

static const MtpOperationCode kSupportedOperationCodes[] = {
    MTP_OPERATION_GET_DEVICE_INFO,
    MTP_OPERATION_OPEN_SESSION,
    MTP_OPERATION_CLOSE_SESSION,
    MTP_OPERATION_GET_STORAGE_IDS,
    MTP_OPERATION_GET_STORAGE_INFO,
    MTP_OPERATION_GET_NUM_OBJECTS,
    MTP_OPERATION_GET_OBJECT_HANDLES,
    MTP_OPERATION_GET_OBJECT_INFO,
    MTP_OPERATION_GET_OBJECT,
//    MTP_OPERATION_INITIATE_CAPTURE,
//    MTP_OPERATION_FORMAT_STORE,
//    MTP_OPERATION_RESET_DEVICE,
//    MTP_OPERATION_SELF_TEST,
//    MTP_OPERATION_SET_OBJECT_PROTECTION,
//    MTP_OPERATION_POWER_DOWN,
//    MTP_OPERATION_TERMINATE_OPEN_CAPTURE,
//    MTP_OPERATION_COPY_OBJECT,
    MTP_OPERATION_GET_PARTIAL_OBJECT,
//    MTP_OPERATION_INITIATE_OPEN_CAPTURE,
//    MTP_OPERATION_SKIP,
    // Android extension for direct file IO
    MTP_OPERATION_GET_PARTIAL_OBJECT_64,
};

static const MtpOperationCode kWriteOperationCodes[] = {
    MTP_OPERATION_SEND_OBJECT_INFO,
    MTP_OPERATION_SEND_OBJECT,
};

static const MtpEventCode kSupportedEventCodes[] = {
    MTP_EVENT_OBJECT_ADDED,
    MTP_EVENT_OBJECT_REMOVED,
    MTP_EVENT_STORE_ADDED,
    MTP_EVENT_STORE_REMOVED,
    MTP_EVENT_OBJECT_INFO_CHANGED,
    MTP_EVENT_OBJECT_PROP_CHANGED,
};

MtpServer::MtpServer(USBMtpInterface* usb, MtpDatabase* database, bool ptp,
                    int fileGroup, int filePerm, int directoryPerm,
                    bool writeEnabled, transfer_switch::TransferObserver* observer,
                    transfer_switch::IncomingObjectSinkFactory* sinkFactory)
    :   mUSB(usb),
        mDatabase(database),
        mPtp(ptp),
        mFileGroup(fileGroup),
        mFilePermission(filePerm),
        mDirectoryPermission(directoryPerm),
        mSessionID(0),
        mSessionOpen(false),
        mSendObjectHandle(kInvalidObjectHandle),
        mSendObjectFormat(0),
        mSendObjectFileSize(0),
        mWriteEnabled(writeEnabled),
        mTransferObserver(observer),
        mSinkFactory(sinkFactory),
        mSendObjectSinkCompleted(false),
        mSendObjectStorageID(0)
{
}

MtpServer::~MtpServer() {
}

void MtpServer::addStorage(MtpStorage* storage) {
    MtpAutolock autoLock(mMutex);

    mStorages.push_back(storage);
    sendStoreAdded(storage->getStorageID());
}

void MtpServer::removeStorage(MtpStorage* storage) {
    MtpAutolock autoLock(mMutex);

    for (int i = 0; i < mStorages.size(); i++) {
        if (mStorages[i] == storage) {
            mStorages.erase(mStorages.begin()+i);
            sendStoreRemoved(storage->getStorageID());
            break;
        }
    }
}

MtpStorage* MtpServer::getStorage(MtpStorageID id) {
    if (id == 0)
        return mStorages[0];
    for (int i = 0; i < mStorages.size(); i++) {
        MtpStorage* storage = mStorages[i];
        if (storage->getStorageID() == id)
            return storage;
    }
    return NULL;
}

bool MtpServer::hasStorage(MtpStorageID id) {
    if (id == 0 || id == 0xFFFFFFFF)
        return mStorages.size() > 0;
    return (getStorage(id) != NULL);
}

void MtpServer::stop() {
    mRunning = false;
}

void MtpServer::run() {
    USBMtpInterface* usb = mUSB;

    VLOG(1) << "MtpServer::run";

    mRunning = true;
    while (mRunning) {
        
        consoleUpdate(NULL);
                
        int ret = mRequest.read(usb);
        if (ret <= 0) {
            VLOG(2) << "request read returned " << ret;
            continue;
        }
        MtpOperationCode operation = mRequest.getOperationCode();
        MtpTransactionID transaction = mRequest.getTransactionID();

        VLOG(2) << "operation: " << MtpDebug::getOperationCodeName(operation);
        mRequest.dump();

        // FIXME need to generalize this
        bool dataIn = (operation == MTP_OPERATION_SEND_OBJECT_INFO
                    || operation == MTP_OPERATION_SET_OBJECT_REFERENCES
                    || operation == MTP_OPERATION_SET_OBJECT_PROP_VALUE
                    || operation == MTP_OPERATION_SET_DEVICE_PROP_VALUE);
        if (dataIn) {
            int ret = mData.read(usb);
            if (ret < 0) {
                VLOG(2) << "data read returned " << ret;
                continue;
            }
            VLOG(2) << "received data:";
            mData.dump();
        } else {
            mData.reset();
        }

        if (handleRequest()) {
            if (!dataIn && mData.hasData()) {
                mData.setOperationCode(operation);
                mData.setTransactionID(transaction);
                VLOG(2) << "sending data:";
                mData.dump();
                ret = mData.write(usb);
                if (ret < 0) {
                    VLOG(2) << "request write returned " << ret;
                    continue;
                }
            }

            mResponse.setTransactionID(transaction);
            VLOG(2) << "sending response "
                    << std::hex << mResponse.getResponseCode() << std::dec;
            ret = mResponse.write(usb);
            mResponse.dump();
            if (mSendObjectSinkCompleted) {
                // The host must receive the final MTP result before potentially slow
                // NCM/NS/ES service teardown performed by the action sink destructor.
                mSendObjectSink.reset();
                mSendObjectSinkCompleted = false;
            }
            if (ret < 0) {
                VLOG(2) << "request write returned " << ret;
                continue;
            }
        } else {
            VLOG(2) << "skipping response";
        }
    }

    // commit any open edits
    int count = mObjectEditList.size();
    for (int i = 0; i < count; i++) {
        ObjectEdit* edit = mObjectEditList[i];
        commitEdit(edit);
        delete edit;
    }
    mObjectEditList.clear();

    if (mSessionOpen)
        mDatabase->sessionEnded();
    mUSB = NULL;
}

void MtpServer::sendObjectAdded(MtpObjectHandle handle) {
    VLOG(1) << "sendObjectAdded " << handle;
    sendEvent(MTP_EVENT_OBJECT_ADDED, handle, 0, 0);
}

void MtpServer::sendObjectRemoved(MtpObjectHandle handle) {
    VLOG(1) << "sendObjectRemoved " << handle;
    sendEvent(MTP_EVENT_OBJECT_REMOVED, handle, 0, 0);
}

void MtpServer::sendObjectInfoChanged(MtpObjectHandle handle) {
    VLOG(1) << "sendObjectInfoChanged " << handle;
    sendEvent(MTP_EVENT_OBJECT_INFO_CHANGED, handle, 0, 0);
}

void MtpServer::sendObjectPropChanged(MtpObjectHandle handle,
                                      MtpObjectProperty prop) {
    VLOG(1) << "sendObjectPropChanged " << handle << " " << prop;
    sendEvent(MTP_EVENT_OBJECT_PROP_CHANGED, handle, prop, 0);
}

void MtpServer::sendStoreAdded(MtpStorageID id) {
    VLOG(1) << "sendStoreAdded " << std::hex << id << std::dec;
    sendEvent(MTP_EVENT_STORE_ADDED, id, 0, 0);
}

void MtpServer::sendStoreRemoved(MtpStorageID id) {
    VLOG(1) << "sendStoreRemoved " << std::hex << id << std::dec;
    sendEvent(MTP_EVENT_STORE_REMOVED, id, 0, 0);
}

void MtpServer::sendEvent(MtpEventCode code,
                          uint32_t param1,
                          uint32_t param2,
                          uint32_t param3) {
    if (mSessionOpen) {
        mEvent.setEventCode(code);
        mEvent.setTransactionID(mRequest.getTransactionID());
        mEvent.setParameter(1, param1);
        mEvent.setParameter(2, param2);
        mEvent.setParameter(3, param3);
        int ret = mEvent.write(mUSB);
        VLOG(2) << "mEvent.write returned " << ret;
    }
}

void MtpServer::addEditObject(MtpObjectHandle handle, MtpString& path,
        uint64_t size, MtpObjectFormat format, int fd) {
    ObjectEdit*  edit = new ObjectEdit(handle, path, size, format, fd);
    mObjectEditList.push_back(edit);
}

MtpServer::ObjectEdit* MtpServer::getEditObject(MtpObjectHandle handle) {
    int count = mObjectEditList.size();
    for (int i = 0; i < count; i++) {
        ObjectEdit* edit = mObjectEditList[i];
        if (edit->mHandle == handle) return edit;
    }
    return NULL;
}

void MtpServer::removeEditObject(MtpObjectHandle handle) {
    int count = mObjectEditList.size();
    for (int i = 0; i < count; i++) {
        ObjectEdit* edit = mObjectEditList[i];
        if (edit->mHandle == handle) {
            delete edit;
            mObjectEditList.erase(mObjectEditList.begin() + i);
            return;
        }
    }
    LOG(ERROR) << "ObjectEdit not found in removeEditObject";
}

void MtpServer::commitEdit(ObjectEdit* edit) {
    mDatabase->endSendObject(edit->mPath.c_str(), edit->mHandle, edit->mFormat, true);
}


bool MtpServer::handleRequest() {
    MtpAutolock autoLock(mMutex);

    MtpOperationCode operation = mRequest.getOperationCode();
    MtpResponseCode response;

    mResponse.reset();

    if (mSendObjectHandle != kInvalidObjectHandle && operation != MTP_OPERATION_SEND_OBJECT) {
        LOG(ERROR) << "expected SendObject after SendObjectInfo";
        if (!mSendObjectTempPath.empty())
            unlink(mSendObjectTempPath.c_str());
        mDatabase->endSendObject(
                mSendObjectFilePath, mSendObjectHandle, mSendObjectFormat, false);
        mSendObjectHandle = kInvalidObjectHandle;
        mSendObjectFormat = 0;
        mSendObjectFilePath.clear();
        mSendObjectTempPath.clear();
    }

    switch (operation) {
        case MTP_OPERATION_GET_DEVICE_INFO:
            response = doGetDeviceInfo();
            break;
        case MTP_OPERATION_OPEN_SESSION:
            response = doOpenSession();
            break;
        case MTP_OPERATION_CLOSE_SESSION:
            response = doCloseSession();
            break;
        case MTP_OPERATION_GET_STORAGE_IDS:
            response = doGetStorageIDs();
            break;
         case MTP_OPERATION_GET_STORAGE_INFO:
            response = doGetStorageInfo();
            break;
        case MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED:
            response = doGetObjectPropsSupported();
            break;
        case MTP_OPERATION_GET_OBJECT_HANDLES:
            response = doGetObjectHandles();
            break;
        case MTP_OPERATION_GET_NUM_OBJECTS:
            response = doGetNumObjects();
            break;
        case MTP_OPERATION_GET_OBJECT_REFERENCES:
            response = doGetObjectReferences();
            break;
        case MTP_OPERATION_SET_OBJECT_REFERENCES:
            response = doSetObjectReferences();
            break;
        case MTP_OPERATION_GET_OBJECT_PROP_VALUE:
            response = doGetObjectPropValue();
            break;
        case MTP_OPERATION_SET_OBJECT_PROP_VALUE:
            response = doSetObjectPropValue();
            break;
        case MTP_OPERATION_GET_DEVICE_PROP_VALUE:
            response = doGetDevicePropValue();
            break;
        case MTP_OPERATION_SET_DEVICE_PROP_VALUE:
            response = doSetDevicePropValue();
            break;
        case MTP_OPERATION_RESET_DEVICE_PROP_VALUE:
            response = doResetDevicePropValue();
            break;
        case MTP_OPERATION_GET_OBJECT_PROP_LIST:
            response = doGetObjectPropList();
            break;
        case MTP_OPERATION_GET_OBJECT_INFO:
            response = doGetObjectInfo();
            break;
        case MTP_OPERATION_GET_OBJECT:
            response = doGetObject();
            break;
        case MTP_OPERATION_GET_THUMB:
            response = doGetThumb();
            break;
        case MTP_OPERATION_GET_PARTIAL_OBJECT:
        case MTP_OPERATION_GET_PARTIAL_OBJECT_64:
            response = doGetPartialObject(operation);
            break;
        case MTP_OPERATION_SEND_OBJECT_INFO:
            response = mWriteEnabled ? doSendObjectInfo() : MTP_RESPONSE_STORE_READ_ONLY;
            break;
        case MTP_OPERATION_SEND_OBJECT:
            response = mWriteEnabled ? doSendObject() : MTP_RESPONSE_STORE_READ_ONLY;
            break;
        case MTP_OPERATION_DELETE_OBJECT:
            response = doDeleteObject();
            break;
        case MTP_OPERATION_MOVE_OBJECT:
            response = doMoveObject();
            break;
        case MTP_OPERATION_GET_OBJECT_PROP_DESC:
            response = doGetObjectPropDesc();
            break;
        case MTP_OPERATION_GET_DEVICE_PROP_DESC:
            response = doGetDevicePropDesc();
            break;
        case MTP_OPERATION_SEND_PARTIAL_OBJECT:
            response = doSendPartialObject();
            break;
        case MTP_OPERATION_TRUNCATE_OBJECT:
            response = doTruncateObject();
            break;
        case MTP_OPERATION_BEGIN_EDIT_OBJECT:
            response = doBeginEditObject();
            break;
        case MTP_OPERATION_END_EDIT_OBJECT:
            response = doEndEditObject();
            break;
        default:
            LOG(ERROR) << "got unsupported command " << MtpDebug::getOperationCodeName(operation);
            response = MTP_RESPONSE_OPERATION_NOT_SUPPORTED;
            break;
    }

    if (response == MTP_RESPONSE_TRANSACTION_CANCELLED)
        return false;
    mResponse.setResponseCode(response);
    return true;
}

MtpResponseCode MtpServer::doGetDeviceInfo() {
    VLOG(1) <<  __PRETTY_FUNCTION__;
    MtpStringBuffer   string;
    //char prop_value[PROP_VALUE_MAX];

    MtpObjectFormatList* playbackFormats = mDatabase->getSupportedPlaybackFormats();
    MtpObjectFormatList* captureFormats = mDatabase->getSupportedCaptureFormats();
    MtpDevicePropertyList* deviceProperties = mDatabase->getSupportedDeviceProperties();

    // fill in device info
    mData.putUInt16(MTP_STANDARD_VERSION);
    if (mPtp) {
        mData.putUInt32(0);
    } else {
        // MTP Vendor Extension ID
        mData.putUInt32(6);
    }
    mData.putUInt16(MTP_STANDARD_VERSION);
    if (mPtp) {
        // no extensions
        string.set("");
    } else {
        // MTP extensions
        string.set("microsoft.com: 1.0; android.com: 1.0;");
    }
    mData.putString(string); // MTP Extensions
    mData.putUInt16(0); //Functional Mode
    UInt16List supportedOperations(
            std::begin(kSupportedOperationCodes), std::end(kSupportedOperationCodes));
    if (mWriteEnabled) {
        supportedOperations.insert(
                supportedOperations.end(),
                std::begin(kWriteOperationCodes),
                std::end(kWriteOperationCodes));
    }
    mData.putAUInt16(&supportedOperations); // Operations Supported
    mData.putAUInt16(kSupportedEventCodes,
            sizeof(kSupportedEventCodes) / sizeof(uint16_t)); // Events Supported
    mData.putAUInt16(deviceProperties); // Device Properties Supported
    mData.putAUInt16(captureFormats); // Capture Formats
    mData.putAUInt16(playbackFormats);  // Playback Formats

    //property_get("ro.product.manufacturer", prop_value, "unknown manufacturer");
    string.set("Transferencia Switch");
    mData.putString(string);   // Manufacturer

    //property_get("ro.product.model", prop_value, "MTP Device");
    string.set("Transferencia Switch");
    mData.putString(string);   // Model
    string.set("0.5.4");
    mData.putString(string);   // Device Version

    //property_get("ro.serialno", prop_value, "????????");
    string.set("TS-MTP-0001");
    mData.putString(string);   // Serial Number

    delete playbackFormats;
    delete captureFormats;
    delete deviceProperties;

    return MTP_RESPONSE_OK;
}

MtpResponseCode MtpServer::doOpenSession() {
    if (mSessionOpen) {
        mResponse.setParameter(1, mSessionID);
        return MTP_RESPONSE_SESSION_ALREADY_OPEN;
    }
    mSessionID = mRequest.getParameter(1);
    mSessionOpen = true;

    mDatabase->sessionStarted(this);

    return MTP_RESPONSE_OK;
}

MtpResponseCode MtpServer::doCloseSession() {
    if (!mSessionOpen)
        return MTP_RESPONSE_SESSION_NOT_OPEN;
    mSessionID = 0;
    mSessionOpen = false;
    mDatabase->sessionEnded();
    return MTP_RESPONSE_OK;
}

MtpResponseCode MtpServer::doGetStorageIDs() {
    if (!mSessionOpen)
        return MTP_RESPONSE_SESSION_NOT_OPEN;

    int count = mStorages.size();
    mData.putUInt32(count);
    for (int i = 0; i < count; i++)
        mData.putUInt32(mStorages[i]->getStorageID());

    return MTP_RESPONSE_OK;
}

MtpResponseCode MtpServer::doGetStorageInfo() {
    MtpStringBuffer   string;

    if (!mSessionOpen)
        return MTP_RESPONSE_SESSION_NOT_OPEN;
    MtpStorageID id = mRequest.getParameter(1);
    MtpStorage* storage = getStorage(id);
    if (!storage)
        return MTP_RESPONSE_INVALID_STORAGE_ID;

    mData.putUInt16(storage->getType());
    mData.putUInt16(storage->getFileSystemType());
    mData.putUInt16(storage->getAccessCapability());
    mData.putUInt64(storage->getMaxCapacity());
    mData.putUInt64(storage->getFreeSpace());
    mData.putUInt32(1024*1024*1024); // Free Space in Objects
    string.set(storage->getDescription());
    mData.putString(string);
    mData.putEmptyString();   // Volume Identifier

    return MTP_RESPONSE_OK;
}

MtpResponseCode MtpServer::doGetObjectPropsSupported() {
    if (!mSessionOpen)
        return MTP_RESPONSE_SESSION_NOT_OPEN;
    MtpObjectFormat format = mRequest.getParameter(1);
    MtpObjectPropertyList* properties = mDatabase->getSupportedObjectProperties(format);
    mData.putAUInt16(properties);
    delete properties;
    return MTP_RESPONSE_OK;
}

MtpResponseCode MtpServer::doGetObjectHandles() {
    if (!mSessionOpen)
        return MTP_RESPONSE_SESSION_NOT_OPEN;
    MtpStorageID storageID = mRequest.getParameter(1);      // 0xFFFFFFFF for all storage
    MtpObjectFormat format = mRequest.getParameter(2);      // 0 for all formats
    MtpObjectHandle parent = mRequest.getParameter(3);      // 0xFFFFFFFF for objects with no parent
                                                            // 0x00000000 for all objects

    if (!hasStorage(storageID))
        return MTP_RESPONSE_INVALID_STORAGE_ID;

    MtpObjectHandleList* handles = mDatabase->getObjectList(storageID, format, parent);
    mData.putAUInt32(handles);
    delete handles;
    return MTP_RESPONSE_OK;
}

MtpResponseCode MtpServer::doGetNumObjects() {
    if (!mSessionOpen)
        return MTP_RESPONSE_SESSION_NOT_OPEN;
    MtpStorageID storageID = mRequest.getParameter(1);      // 0xFFFFFFFF for all storage
    MtpObjectFormat format = mRequest.getParameter(2);      // 0 for all formats
    MtpObjectHandle parent = mRequest.getParameter(3);      // 0xFFFFFFFF for objects with no parent
                                                            // 0x00000000 for all objects
    if (!hasStorage(storageID))
        return MTP_RESPONSE_INVALID_STORAGE_ID;

    int count = mDatabase->getNumObjects(storageID, format, parent);
    if (count >= 0) {
        mResponse.setParameter(1, count);
        return MTP_RESPONSE_OK;
    } else {
        mResponse.setParameter(1, 0);
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    }
}

MtpResponseCode MtpServer::doGetObjectReferences() {
    if (!mSessionOpen)
        return MTP_RESPONSE_SESSION_NOT_OPEN;
    if (!hasStorage())
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    MtpObjectHandle handle = mRequest.getParameter(1);

    if (!mDatabase->isHandleValid(handle)) {
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    }

    MtpObjectHandleList* handles = mDatabase->getObjectReferences(handle);
    if (handles) {
        mData.putAUInt32(handles);
        delete handles;
    } else {
        mData.putEmptyArray();
    }
    return MTP_RESPONSE_OK;
}

MtpResponseCode MtpServer::doSetObjectReferences() {
    if (!mSessionOpen)
        return MTP_RESPONSE_SESSION_NOT_OPEN;
    if (!hasStorage())
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    MtpStorageID handle = mRequest.getParameter(1);

    MtpObjectHandleList* references = mData.getAUInt32();
    MtpResponseCode result = mDatabase->setObjectReferences(handle, references);
    delete references;
    return result;
}

MtpResponseCode MtpServer::doGetObjectPropValue() {
    if (!hasStorage())
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    MtpObjectHandle handle = mRequest.getParameter(1);
    MtpObjectProperty property = mRequest.getParameter(2);
    VLOG(2) << "GetObjectPropValue " << handle
            << " " << MtpDebug::getObjectPropCodeName(property);

    return mDatabase->getObjectPropertyValue(handle, property, mData);
}

MtpResponseCode MtpServer::doSetObjectPropValue() {
    MtpResponseCode response;

    if (!hasStorage())
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    MtpObjectHandle handle = mRequest.getParameter(1);
    MtpObjectProperty property = mRequest.getParameter(2);
    VLOG(2) << "SetObjectPropValue " << handle
            << " " << MtpDebug::getObjectPropCodeName(property);

    response = mDatabase->setObjectPropertyValue(handle, property, mData);

    //sendObjectPropChanged(handle, property);

    return response;
}

MtpResponseCode MtpServer::doGetDevicePropValue() {
    MtpDeviceProperty property = mRequest.getParameter(1);
    VLOG(1) << "GetDevicePropValue " << MtpDebug::getDevicePropCodeName(property);

    return mDatabase->getDevicePropertyValue(property, mData);
}

MtpResponseCode MtpServer::doSetDevicePropValue() {
    MtpDeviceProperty property = mRequest.getParameter(1);
    VLOG(1) << "SetDevicePropValue " << MtpDebug::getDevicePropCodeName(property);

    return mDatabase->setDevicePropertyValue(property, mData);
}

MtpResponseCode MtpServer::doResetDevicePropValue() {
    MtpDeviceProperty property = mRequest.getParameter(1);
    VLOG(1) << "ResetDevicePropValue " << MtpDebug::getDevicePropCodeName(property);

    return mDatabase->resetDeviceProperty(property);
}

MtpResponseCode MtpServer::doGetObjectPropList() {
    if (!hasStorage())
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;

    MtpObjectHandle handle = mRequest.getParameter(1);
    // use uint32_t so we can support 0xFFFFFFFF
    uint32_t format = mRequest.getParameter(2);
    uint32_t property = mRequest.getParameter(3);
    int groupCode = mRequest.getParameter(4);
    int depth = mRequest.getParameter(5);
    VLOG(2) << "GetObjectPropList " << handle
            << " format: " << MtpDebug::getFormatCodeName(format)
            << " property: " << MtpDebug::getObjectPropCodeName(property)
            << " group: " << groupCode
            << " depth: " << depth;

    return mDatabase->getObjectPropertyList(handle, format, property, groupCode, depth, mData);
}

MtpResponseCode MtpServer::doGetObjectInfo() {
    if (!hasStorage())
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    MtpObjectHandle handle = mRequest.getParameter(1);
    MtpObjectInfo info(handle);
    MtpResponseCode result = mDatabase->getObjectInfo(handle, info);
    if (result == MTP_RESPONSE_OK) {
        char    date[20];

        mData.putUInt32(info.mStorageID);
        mData.putUInt16(info.mFormat);
        mData.putUInt16(info.mProtectionStatus);

        // if object is being edited the database size may be out of date
        uint32_t size = info.mCompressedSize;
        ObjectEdit* edit = getEditObject(handle);
        if (edit)
            size = (edit->mSize > 0xFFFFFFFFLL ? 0xFFFFFFFF : (uint32_t)edit->mSize);
        mData.putUInt32(size);

        mData.putUInt16(info.mThumbFormat);
        mData.putUInt32(info.mThumbCompressedSize);
        mData.putUInt32(info.mThumbPixWidth);
        mData.putUInt32(info.mThumbPixHeight);
        mData.putUInt32(info.mImagePixWidth);
        mData.putUInt32(info.mImagePixHeight);
        mData.putUInt32(info.mImagePixDepth);
        mData.putUInt32(info.mParent);
        mData.putUInt16(info.mAssociationType);
        mData.putUInt32(info.mAssociationDesc);
        mData.putUInt32(info.mSequenceNumber);
        mData.putString(info.mName);
        mData.putEmptyString();    // date created
        formatDateTime(info.mDateModified, date, sizeof(date));
        mData.putString(date);   // date modified
        mData.putEmptyString();   // keywords
    }
    return result;
}

struct mtp_file_range {
    int fd;
    off_t offset;
    int64_t length;
    uint16_t command;
    uint32_t transaction_id;
};

static int send_file(USBMtpInterface* usb, struct mtp_file_range * mfr)
{
    int actualsize;
    int j, ofs;
    int blocksize;

    struct stat buf;
    fstat(mfr->fd, &buf);

    unsigned char * buffer = (unsigned char*)memalign(0x1000, 16384);
    *(uint32_t*)&buffer[0] = mfr->length + MTP_CONTAINER_HEADER_SIZE;
    *(uint16_t*)&buffer[4] = MTP_CONTAINER_TYPE_DATA;
    *(uint16_t*)&buffer[6] = mfr->command;
    *(uint32_t*)&buffer[8] = mfr->transaction_id;

    if(mfr->offset >= buf.st_size)
    {
        actualsize = 0;
    }
    else
    {
      if(mfr->offset + mfr->length > buf.st_size)
          actualsize = buf.st_size - mfr->offset;
      else
          actualsize = mfr->length;
    }

    lseek(mfr->fd, mfr->offset, SEEK_SET);
    ofs = MTP_CONTAINER_HEADER_SIZE;
    j = 0;
    do
    {
        if((j + (16384 - ofs)) < actualsize)
            blocksize = (16384 - ofs);
        else
            blocksize = actualsize - j;
    
        read(mfr->fd, &buffer[ofs], blocksize);
        j += blocksize;
        ofs += blocksize;
    
        usb->write((const char*)buffer, ofs);
        ofs = 0;
    } while(j < actualsize);

    free(buffer);

    return actualsize;
}

static int64_t receive_file(
        USBMtpInterface* usb,
        struct mtp_file_range* mfr,
        transfer_switch::TransferObserver* observer = nullptr,
        uint64_t progressBase = 0,
        uint64_t progressTotal = 0)
{
    if (mfr->length == 0xFFFFFFFF) {
        errno = EINVAL;
        return -1;
    }

    int64_t total = 0;
    unsigned char * buffer = (unsigned char*)memalign(0x1000, 16384);
    if (buffer == nullptr) {
        errno = ENOMEM;
        return -1;
    }
    if (lseek(mfr->fd, mfr->offset, SEEK_SET) < 0) {
        free(buffer);
        return -1;
    }

    while (total < mfr->length) {
        const size_t remaining = static_cast<size_t>(mfr->length - total);
        const size_t requestSize = remaining < 16384 ? remaining : 16384;
        const ssize_t size = usb->read((char*)buffer, requestSize);
        if (size <= 0) {
            if (size == 0)
                errno = EIO;
            total = -1;
            break;
        }

        ssize_t written = 0;
        while (written < size) {
            const ssize_t current = write(mfr->fd, buffer + written, size - written);
            if (current <= 0) {
                if (current == 0)
                    errno = EIO;
                total = -1;
                break;
            }
            written += current;
        }
        if (total < 0)
            break;

        total += size;
        if (observer != nullptr)
            observer->transferProgress(progressBase + total, progressTotal);
    }

    free(buffer);

    return total;
}

static int64_t receive_sink(
        USBMtpInterface* usb,
        transfer_switch::IncomingObjectSink* sink,
        int64_t length,
        transfer_switch::TransferObserver* observer = nullptr,
        uint64_t progressBase = 0,
        uint64_t progressTotal = 0,
        bool* sinkRejected = nullptr)
{
    if (sink == nullptr || length < 0) {
        errno = EINVAL;
        return -1;
    }

    const bool unknownLength = length == 0xFFFFFFFF;
    int64_t total = 0;
    bool rejected = sinkRejected != nullptr && *sinkRejected;
    unsigned char* buffer = static_cast<unsigned char*>(memalign(0x1000, 16384));
    if (buffer == nullptr) {
        errno = ENOMEM;
        return -1;
    }
    while (unknownLength || total < length) {
        if (unknownLength && sink->isComplete()) {
            break;
        }
        uint64_t effectiveTotal = progressTotal;
        if (unknownLength && sink->hasKnownFinalSize()) {
            effectiveTotal = sink->finalSize();
            if (progressBase + static_cast<uint64_t>(total) >= effectiveTotal) {
                break;
            }
        }
        const uint64_t remaining64 = unknownLength && sink->hasKnownFinalSize()
            ? effectiveTotal - progressBase - static_cast<uint64_t>(total)
            : static_cast<uint64_t>(length - total);
        const size_t remaining = remaining64 < 16384
            ? static_cast<size_t>(remaining64)
            : 16384;
        const size_t requestSize = remaining < 16384 ? remaining : 16384;
        const ssize_t size = usb->read(reinterpret_cast<char*>(buffer), requestSize);
        if (size <= 0) {
            if (size == 0) errno = EIO;
            total = -1;
            break;
        }
        if (!rejected && !sink->write(buffer, static_cast<size_t>(size))) {
            // Keep draining the MTP data container so the following transaction is
            // still aligned and Windows can receive the real rejection response.
            rejected = true;
        }
        total += size;
        if (observer != nullptr)
            observer->transferProgress(
                progressBase + total,
                unknownLength && sink->hasKnownFinalSize() ? sink->finalSize() : progressTotal);
    }
    free(buffer);
    if (sinkRejected != nullptr) *sinkRejected = rejected;
    return total;
}

MtpResponseCode MtpServer::doGetObject() {
    if (!hasStorage())
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    MtpObjectHandle handle = mRequest.getParameter(1);
    MtpString pathBuf;
    int64_t fileLength;
    MtpObjectFormat format;
    int result = mDatabase->getObjectFilePath(handle, pathBuf, fileLength, format);
    if (result != MTP_RESPONSE_OK)
        return result;

    struct mtp_file_range mfr;
    mfr.fd = open(pathBuf.c_str(), O_RDONLY);
    if (mfr.fd < 0) {
        return MTP_RESPONSE_GENERAL_ERROR;
    }
    mfr.offset = 0;
    mfr.length = fileLength;
    mfr.command = mRequest.getOperationCode();
    mfr.transaction_id = mRequest.getTransactionID();

    // then transfer the file
    int ret = send_file(mUSB, &mfr);
    VLOG(2) << "MTP_SEND_FILE_WITH_HEADER returned " << ret;
    close(mfr.fd);
    if (ret < 0) {
        if (errno == ECANCELED)
            return MTP_RESPONSE_TRANSACTION_CANCELLED;
        else
            return MTP_RESPONSE_GENERAL_ERROR;
    }
    return MTP_RESPONSE_OK;
}

MtpResponseCode MtpServer::doGetThumb() {
    MtpObjectHandle handle = mRequest.getParameter(1);
    size_t thumbSize;
    void* thumb = mDatabase->getThumbnail(handle, thumbSize);
    if (thumb) {
        // send data
        mData.setOperationCode(mRequest.getOperationCode());
        mData.setTransactionID(mRequest.getTransactionID());
        mData.writeData(mUSB, thumb, thumbSize);
        free(thumb);
        return MTP_RESPONSE_OK;
    } else {
        return MTP_RESPONSE_GENERAL_ERROR;
    }
}

MtpResponseCode MtpServer::doGetPartialObject(MtpOperationCode operation) {
    if (!hasStorage())
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    MtpObjectHandle handle = mRequest.getParameter(1);
    uint64_t offset;
    uint32_t length;
    offset = mRequest.getParameter(2);
    if (operation == MTP_OPERATION_GET_PARTIAL_OBJECT_64) {
        // android extension with 64 bit offset
        uint64_t offset2 = mRequest.getParameter(3);
        offset = offset | (offset2 << 32);
        length = mRequest.getParameter(4);
    } else {
        // standard GetPartialObject
        length = mRequest.getParameter(3);
    }
    MtpString pathBuf;
    int64_t fileLength;
    MtpObjectFormat format;
    int result = mDatabase->getObjectFilePath(handle, pathBuf, fileLength, format);
    if (result != MTP_RESPONSE_OK)
        return result;
    if (offset + length > fileLength)
        length = fileLength - offset;

    mtp_file_range  mfr;
    mfr.fd = open(pathBuf.c_str(), O_RDONLY);
    if (mfr.fd < 0) {
        return MTP_RESPONSE_GENERAL_ERROR;
    }
    mfr.offset = offset;
    mfr.length = length;
    mfr.command = mRequest.getOperationCode();
    mfr.transaction_id = mRequest.getTransactionID();
    mResponse.setParameter(1, length);

    // transfer the file
    int ret = send_file(mUSB, &mfr);
    VLOG(2) << "MTP_SEND_FILE_WITH_HEADER returned " << ret;
    close(mfr.fd);
    if (ret < 0) {
        if (errno == ECANCELED)
            return MTP_RESPONSE_TRANSACTION_CANCELLED;
        else
            return MTP_RESPONSE_GENERAL_ERROR;
    }
    return MTP_RESPONSE_OK;
}

MtpResponseCode MtpServer::doSendObjectInfo() {
    if (!mWriteEnabled)
        return MTP_RESPONSE_STORE_READ_ONLY;

    MtpString path;
    MtpStorageID storageID = mRequest.getParameter(1);
    MtpStorage* storage = getStorage(storageID);
    MtpObjectHandle parent = mRequest.getParameter(2);
    if (!storage)
        return MTP_RESPONSE_INVALID_STORAGE_ID;

    // special case the root
    if (parent == MTP_PARENT_ROOT) {
        path = storage->getPath();
        parent = 0;
    } else {
        int64_t length;
        MtpObjectFormat format;
        int result = mDatabase->getObjectFilePath(parent, path, length, format);
        if (result != MTP_RESPONSE_OK)
            return result;
        if (format != MTP_FORMAT_ASSOCIATION)
            return MTP_RESPONSE_INVALID_PARENT_OBJECT;
    }

    // read only the fields we need
    mData.getUInt32();  // storage ID
    MtpObjectFormat format = mData.getUInt16();
    mData.getUInt16();  // protection status
    mSendObjectFileSize = mData.getUInt32();
    mData.getUInt16();  // thumb format
    mData.getUInt32();  // thumb compressed size
    mData.getUInt32();  // thumb pix width
    mData.getUInt32();  // thumb pix height
    mData.getUInt32();  // image pix width
    mData.getUInt32();  // image pix height
    mData.getUInt32();  // image bit depth
    mData.getUInt32();  // parent
    uint16_t associationType = mData.getUInt16();
    uint32_t associationDesc = mData.getUInt32();   // association desc
    mData.getUInt32();  // sequence number
    MtpStringBuffer name, created, modified;
    mData.getString(name);    // file name
    mData.getString(created);      // date created
    mData.getString(modified);     // date modified
    // keywords follow

    VLOG(2) << "name: " << (const char *) name
            << " format: " << std::hex << format << std::dec;
    time_t modifiedTime;
    if (!parseDateTime(modified, modifiedTime))
        modifiedTime = 0;

    if (path[path.size() - 1] != '/')
        path += "/";
    path += (const char *)name;

    // check space first
    if (mSendObjectFileSize > storage->getFreeSpace())
        return MTP_RESPONSE_STORAGE_FULL;
    uint64_t maxFileSize = storage->getMaxFileSize();
    // check storage max file size
    if (maxFileSize != 0) {
        // if mSendObjectFileSize is 0xFFFFFFFF, then all we know is the file size
        // is >= 0xFFFFFFFF
        if (mSendObjectFileSize > maxFileSize || mSendObjectFileSize == 0xFFFFFFFF)
            return MTP_RESPONSE_OBJECT_TOO_LARGE;
    }

    VLOG(2) << "path: " << path.c_str() << " parent: " << parent
            << " storageID: " << std::hex << storageID << std::dec;
    MtpObjectHandle handle = mDatabase->beginSendObject(path.c_str(),
            format, parent, storageID, mSendObjectFileSize, modifiedTime);
    if (handle == kInvalidObjectHandle) {
        return MTP_RESPONSE_GENERAL_ERROR;
    }

    if (format == MTP_FORMAT_ASSOCIATION) {
        int ret = mkdir(path.c_str(), mDirectoryPermission);
        if (ret != 0 && errno != EEXIST) {
            mDatabase->endSendObject(path, handle, MTP_FORMAT_ASSOCIATION, false);
            return MTP_RESPONSE_GENERAL_ERROR;
        }

        // SendObject does not get sent for directories, so call endSendObject here instead
        mDatabase->endSendObject(path, handle, MTP_FORMAT_ASSOCIATION, true);
    } else {
        mSendObjectFilePath = path;
        mSendObjectStorageID = storageID;
        if (mSinkFactory != nullptr && mSinkFactory->handles(storageID)) {
            mSendObjectSinkCompleted = false;
            mSendObjectSink = mSinkFactory->open(
                storageID, mSendObjectFilePath.c_str(), mSendObjectFileSize);
            if (!mSendObjectSink) {
                if (mTransferObserver != nullptr) {
                    mTransferObserver->transferStarted(
                        mSendObjectFilePath.c_str(), mSendObjectFileSize);
                    mTransferObserver->transferFinished(
                        mSendObjectFilePath.c_str(),
                        0,
                        mSendObjectFileSize,
                        transfer_switch::TransferOutcome::failed,
                        mSinkFactory->detail());
                }
                mDatabase->endSendObject(path, handle, format, false);
                mSendObjectHandle = kInvalidObjectHandle;
                mSendObjectStorageID = 0;
                mSendObjectFilePath.clear();
                mSendObjectFileSize = 0;
                return MTP_RESPONSE_INVALID_DATASET;
            }
        }
        mSendObjectTempPath = storage->getPath();
        if (mSendObjectTempPath[mSendObjectTempPath.size() - 1] != '/')
            mSendObjectTempPath += "/";
        mSendObjectTempPath += ".transferencia-switch-staging";
        if (!mSendObjectSink &&
            mkdir(mSendObjectTempPath.c_str(), mDirectoryPermission) != 0 && errno != EEXIST) {
            mDatabase->endSendObject(path, handle, format, false);
            mSendObjectTempPath.clear();
            return MTP_RESPONSE_GENERAL_ERROR;
        }
        if (!mSendObjectSink) {
            char temporaryName[32];
            std::snprintf(temporaryName, sizeof(temporaryName), "/%08X.partial", handle);
            mSendObjectTempPath += temporaryName;
        } else {
            mSendObjectTempPath.clear();
        }
        // save the handle for the SendObject call, which should follow
        mSendObjectHandle = handle;
        mSendObjectFormat = format;
    }

    mResponse.setParameter(1, storageID);
    mResponse.setParameter(2, parent);
    mResponse.setParameter(3, handle);

    return MTP_RESPONSE_OK;
}

MtpResponseCode MtpServer::doSendObject() {
    if (!mWriteEnabled)
        return MTP_RESPONSE_STORE_READ_ONLY;
    if (!hasStorage())
        return MTP_RESPONSE_GENERAL_ERROR;

    if (mSendObjectHandle == kInvalidObjectHandle) {
        LOG(ERROR) << "Expected SendObjectInfo before SendObject";
        mData.reset();
        return MTP_RESPONSE_NO_VALID_OBJECT_INFO;
    }

    MtpResponseCode result = MTP_RESPONSE_OK;
    const char* detail = "completado";
    uint64_t transferred = 0;
    bool sink_rejected = false;
    int fd = -1;
    if (mTransferObserver != nullptr)
        mTransferObserver->transferStarted(mSendObjectFilePath.c_str(), mSendObjectFileSize);

    // read the header, and possibly some data
    int ret = mData.read(mUSB, 512);
    if (ret < MTP_CONTAINER_HEADER_SIZE) {
        result = MTP_RESPONSE_GENERAL_ERROR;
        detail = "cabecera MTP incompleta";
    }
    const int initialData = result == MTP_RESPONSE_OK
        ? ret - MTP_CONTAINER_HEADER_SIZE
        : 0;
    const bool unknownObjectSize = mSendObjectFileSize == 0xFFFFFFFF;
    if (result == MTP_RESPONSE_OK && !unknownObjectSize &&
        static_cast<uint64_t>(initialData) > mSendObjectFileSize) {
        result = MTP_RESPONSE_INVALID_DATASET;
        detail = "tamano recibido invalido";
    }

    if (!mSendObjectSink)
        unlink(mSendObjectTempPath.c_str());
    if (result == MTP_RESPONSE_OK && !mSendObjectSink)
        fd = open(mSendObjectTempPath.c_str(), O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (result == MTP_RESPONSE_OK && !mSendObjectSink && fd < 0) {
        result = MTP_RESPONSE_GENERAL_ERROR;
        detail = "no se pudo crear el archivo temporal";
    }

    if (result == MTP_RESPONSE_OK && initialData > 0) {
        const unsigned char* source = static_cast<const unsigned char*>(mData.getData());
        if (mSendObjectSink) {
            if (!mSendObjectSink->write(source, static_cast<size_t>(initialData))) {
                sink_rejected = true;
                detail = mSendObjectSink->detail();
            }
            transferred = initialData;
        } else {
            int written = 0;
            while (written < initialData) {
                const ssize_t current = write(fd, source + written, initialData - written);
                if (current <= 0) {
                    result = MTP_RESPONSE_GENERAL_ERROR;
                    detail = "fallo de escritura inicial";
                    break;
                }
                written += current;
            }
            transferred = written;
        }
        if (mTransferObserver != nullptr)
            mTransferObserver->transferProgress(
                transferred,
                mSendObjectSink && mSendObjectSink->hasKnownFinalSize()
                    ? mSendObjectSink->finalSize()
                    : mSendObjectFileSize);
    }

    if (result == MTP_RESPONSE_OK &&
        (unknownObjectSize || transferred < mSendObjectFileSize)) {
        VLOG(2) << "receiving " << mSendObjectFilePath.c_str();
        int64_t received = 0;
        if (mSendObjectSink) {
            received = receive_sink(
                mUSB,
                mSendObjectSink.get(),
                unknownObjectSize ? 0xFFFFFFFF : mSendObjectFileSize - transferred,
                mTransferObserver,
                transferred,
                mSendObjectFileSize,
                &sink_rejected
            );
        } else {
            mtp_file_range mfr;
            mfr.fd = fd;
            mfr.offset = transferred;
            mfr.length = mSendObjectFileSize - transferred;
            received = receive_file(
                mUSB, &mfr, mTransferObserver, transferred, mSendObjectFileSize);
        }
        VLOG(2) << "MTP_RECEIVE_FILE returned " << received;
        if (received < 0) {
            result = errno == ECANCELED
                ? MTP_RESPONSE_TRANSACTION_CANCELLED
                : MTP_RESPONSE_GENERAL_ERROR;
            detail = errno == ECANCELED ? "cancelado" : "fallo durante la recepcion";
        } else {
            transferred += static_cast<uint64_t>(received);
            if (sink_rejected) {
                result = MTP_RESPONSE_INVALID_DATASET;
                detail = mSendObjectSink->detail();
            }
        }
    }

    if (mSendObjectSink && result == MTP_RESPONSE_OK) {
        const uint64_t finalSize = mSendObjectSink->hasKnownFinalSize()
            ? mSendObjectSink->finalSize()
            : mSendObjectFileSize;
        if (!mSendObjectSink->hasKnownFinalSize() || transferred != finalSize) {
            result = MTP_RESPONSE_GENERAL_ERROR;
            detail = "tamano final no coincide";
        } else if (!mSendObjectSink->finish()) {
            result = MTP_RESPONSE_GENERAL_ERROR;
            detail = mSendObjectSink->detail();
        } else {
            detail = mSendObjectSink->detail();
        }
    }
    if (fd >= 0 && result == MTP_RESPONSE_OK) {
        struct stat information {};
        if (fstat(fd, &information) != 0 ||
            static_cast<uint64_t>(information.st_size) != mSendObjectFileSize ||
            transferred != mSendObjectFileSize) {
            result = MTP_RESPONSE_GENERAL_ERROR;
            detail = "tamano final no coincide";
        } else if (fsync(fd) != 0) {
            result = MTP_RESPONSE_GENERAL_ERROR;
            detail = "no se pudo sincronizar la SD";
        }
    }
    if (fd >= 0 && close(fd) != 0 && result == MTP_RESPONSE_OK) {
        result = MTP_RESPONSE_GENERAL_ERROR;
        detail = "no se pudo cerrar el archivo temporal";
    }
    if (!mSendObjectSink && result == MTP_RESPONSE_OK &&
        rename(mSendObjectTempPath.c_str(), mSendObjectFilePath.c_str()) != 0) {
        result = MTP_RESPONSE_GENERAL_ERROR;
        detail = "no se pudo publicar el archivo final";
    }
    if (result != MTP_RESPONSE_OK) {
        if (mSendObjectSink)
            mSendObjectSink->abort();
        else
            unlink(mSendObjectTempPath.c_str());
    }

    // reset so we don't attempt to send the data back
    mData.reset();

    mDatabase->endSendObject(mSendObjectFilePath, mSendObjectHandle, mSendObjectFormat,
            result == MTP_RESPONSE_OK);
    if (mTransferObserver != nullptr) {
        const transfer_switch::TransferOutcome outcome = result == MTP_RESPONSE_OK
            ? transfer_switch::TransferOutcome::succeeded
            : (result == MTP_RESPONSE_TRANSACTION_CANCELLED
                ? transfer_switch::TransferOutcome::cancelled
                : transfer_switch::TransferOutcome::failed);
        mTransferObserver->transferFinished(
                mSendObjectFilePath.c_str(),
                transferred,
                mSendObjectSink && mSendObjectSink->hasKnownFinalSize()
                    ? mSendObjectSink->finalSize()
                    : mSendObjectFileSize,
                outcome,
                detail);
    }
    mSendObjectHandle = kInvalidObjectHandle;
    mSendObjectSinkCompleted = mSendObjectSink != nullptr;
    mSendObjectStorageID = 0;
    mSendObjectFormat = 0;
    mSendObjectFilePath.clear();
    mSendObjectTempPath.clear();
    mSendObjectFileSize = 0;
    return result;
}

static void deleteRecursive(const char* path) {
    char pathbuf[PATH_MAX];
    int pathLength = strlen(path);
    if (pathLength >= sizeof(pathbuf) - 1) {
        LOG(ERROR) << "path too long: " << path;
    }
    strcpy(pathbuf, path);
    if (pathbuf[pathLength - 1] != '/') {
        pathbuf[pathLength++] = '/';
    }
    char* fileSpot = pathbuf + pathLength;
    int pathRemaining = sizeof(pathbuf) - pathLength - 1;

    DIR* dir = opendir(path);
    if (!dir) {
        LOG(ERROR) << "opendir " << path << " failed";
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir))) {
        const char* name = entry->d_name;

        // ignore "." and ".."
        if (name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0))) {
            continue;
        }

        int nameLength = strlen(name);
        if (nameLength > pathRemaining) {
            LOG(ERROR) << "path " << path << "/" << name << " too long";
            continue;
        }
        strcpy(fileSpot, name);

        int type = entry->d_type;
        if (entry->d_type == DT_DIR) {
            deleteRecursive(pathbuf);
            rmdir(pathbuf);
        } else {
            unlink(pathbuf);
        }
    }
    closedir(dir);
}

static void deletePath(const char* path) {
    struct stat statbuf;
    if (stat(path, &statbuf) == 0) {
        if (S_ISDIR(statbuf.st_mode)) {
            deleteRecursive(path);
            rmdir(path);
        } else {
            unlink(path);
        }
    } else {
        LOG(ERROR) << "deletePath stat failed for " << path;
    }
}

MtpResponseCode MtpServer::doDeleteObject() {
    if (!hasStorage())
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    MtpObjectHandle handle = mRequest.getParameter(1);
    MtpObjectFormat format = mRequest.getParameter(2);
    // FIXME - support deleting all objects if handle is 0xFFFFFFFF
    // FIXME - implement deleting objects by format

    MtpString filePath;
    int64_t fileLength;
    int result = mDatabase->getObjectFilePath(handle, filePath, fileLength, format);
    if (result == MTP_RESPONSE_OK) {
        VLOG(2) << "deleting " << filePath.c_str();
        result = mDatabase->deleteFile(handle);
        // Don't delete the actual files unless the database deletion is allowed
        if (result == MTP_RESPONSE_OK) {
            deletePath(filePath.c_str());
        }
    }

    return result;
}

MtpResponseCode MtpServer::doMoveObject() {
    if (!hasStorage())
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    MtpObjectHandle handle = mRequest.getParameter(1);
    MtpObjectFormat format = mRequest.getParameter(2);
    MtpObjectHandle newparent = mRequest.getParameter(3);

    MtpString filePath;
    MtpString newPath;
    int64_t fileLength;
    int result = mDatabase->getObjectFilePath(handle, filePath, fileLength, format);
    result = mDatabase->getObjectFilePath(handle, newPath, fileLength, format);
    if (result == MTP_RESPONSE_OK) {
        VLOG(2) << "moving " << filePath.c_str() << " to " << newPath.c_str();
        result = mDatabase->moveFile(handle, newparent);
        // Don't move the actual files unless the database deletion is allowed
        if (result == MTP_RESPONSE_OK) {
            rename(filePath.c_str(), newPath.c_str());
        }
    }

    return result;
}

MtpResponseCode MtpServer::doGetObjectPropDesc() {
    MtpObjectProperty propCode = mRequest.getParameter(1);
    MtpObjectFormat format = mRequest.getParameter(2);
    VLOG(2) << "GetObjectPropDesc " << MtpDebug::getObjectPropCodeName(propCode)
            << " " << MtpDebug::getFormatCodeName(format);
    MtpProperty* property = mDatabase->getObjectPropertyDesc(propCode, format);
    if (!property)
        return MTP_RESPONSE_OBJECT_PROP_NOT_SUPPORTED;
    property->write(mData);
    delete property;
    return MTP_RESPONSE_OK;
}

MtpResponseCode MtpServer::doGetDevicePropDesc() {
    MtpDeviceProperty propCode = mRequest.getParameter(1);
    VLOG(1) << "GetDevicePropDesc " << MtpDebug::getDevicePropCodeName(propCode);
    MtpProperty* property = mDatabase->getDevicePropertyDesc(propCode);
    if (!property)
        return MTP_RESPONSE_DEVICE_PROP_NOT_SUPPORTED;
    property->write(mData);
    delete property;
    return MTP_RESPONSE_OK;
}

MtpResponseCode MtpServer::doSendPartialObject() {
    if (!hasStorage())
        return MTP_RESPONSE_INVALID_OBJECT_HANDLE;
    MtpObjectHandle handle = mRequest.getParameter(1);
    uint64_t offset = mRequest.getParameter(2);
    uint64_t offset2 = mRequest.getParameter(3);
    offset = offset | (offset2 << 32);
    uint32_t length = mRequest.getParameter(4);

    ObjectEdit* edit = getEditObject(handle);
    if (!edit) {
        LOG(ERROR) << "object not open for edit in doSendPartialObject";
        return MTP_RESPONSE_GENERAL_ERROR;
    }

    // can't start writing past the end of the file
    if (offset > edit->mSize) {
        VLOG(2) << "writing past end of object, offset: " << offset
                << " edit->mSize: " << edit->mSize;
        return MTP_RESPONSE_GENERAL_ERROR;
    }

    const char* filePath = edit->mPath.c_str();
    VLOG(2) << "receiving partial " << filePath
            << " " << offset << " " << length;

    // read the header, and possibly some data
    int ret = mData.read(mUSB, 512);
    if (ret < MTP_CONTAINER_HEADER_SIZE)
        return MTP_RESPONSE_GENERAL_ERROR;
    int initialData = ret - MTP_CONTAINER_HEADER_SIZE;

    if (initialData > 0) {
        ret = write(edit->mFD, mData.getData(), initialData);
        offset += initialData;
        length -= initialData;
    }

    if (length > 0) {
        mtp_file_range  mfr;
        mfr.fd = edit->mFD;
        mfr.offset = offset;
        mfr.length = length;

        // transfer the file
        ret = receive_file(mUSB, &mfr, nullptr, offset, offset + length);
        VLOG(2) << "MTP_RECEIVE_FILE returned " << ret;
    }
    if (ret < 0) {
        mResponse.setParameter(1, 0);
        if (errno == ECANCELED)
            return MTP_RESPONSE_TRANSACTION_CANCELLED;
        else
            return MTP_RESPONSE_GENERAL_ERROR;
    }

    // reset so we don't attempt to send this back
    mData.reset();
    mResponse.setParameter(1, length);
    uint64_t end = offset + length;
    if (end > edit->mSize) {
        edit->mSize = end;
    }
    return MTP_RESPONSE_OK;
}

MtpResponseCode MtpServer::doTruncateObject() {
    MtpObjectHandle handle = mRequest.getParameter(1);
    ObjectEdit* edit = getEditObject(handle);
    if (!edit) {
        LOG(ERROR) << "object not open for edit in doTruncateObject";
        return MTP_RESPONSE_GENERAL_ERROR;
    }

    uint64_t offset = mRequest.getParameter(2);
    uint64_t offset2 = mRequest.getParameter(3);
    offset |= (offset2 << 32);
    if (ftruncate(edit->mFD, offset) != 0) {
        return MTP_RESPONSE_GENERAL_ERROR;
    } else {
        edit->mSize = offset;
        return MTP_RESPONSE_OK;
    }
}

MtpResponseCode MtpServer::doBeginEditObject() {
    MtpObjectHandle handle = mRequest.getParameter(1);
    if (getEditObject(handle)) {
        LOG(ERROR) << "object already open for edit in doBeginEditObject";
        return MTP_RESPONSE_GENERAL_ERROR;
    }

    MtpString path;
    int64_t fileLength;
    MtpObjectFormat format;
    int result = mDatabase->getObjectFilePath(handle, path, fileLength, format);
    if (result != MTP_RESPONSE_OK)
        return result;

    int fd = open(path.c_str(), O_RDWR | O_EXCL);
    if (fd < 0) {
        LOG(ERROR) << "open failed for " << path.c_str() << " in doBeginEditObject";
        return MTP_RESPONSE_GENERAL_ERROR;
    }

    addEditObject(handle, path, fileLength, format, fd);
    return MTP_RESPONSE_OK;
}

MtpResponseCode MtpServer::doEndEditObject() {
    MtpObjectHandle handle = mRequest.getParameter(1);
    ObjectEdit* edit = getEditObject(handle);
    if (!edit) {
        LOG(ERROR) << "object not open for edit in doEndEditObject";
        return MTP_RESPONSE_GENERAL_ERROR;
    }

    commitEdit(edit);
    removeEditObject(handle);
    return MTP_RESPONSE_OK;
}

}  // namespace android
