#ifndef TRANSFER_SWITCH_PORTS_INCOMING_OBJECT_SINK_HPP
#define TRANSFER_SWITCH_PORTS_INCOMING_OBJECT_SINK_HPP

#include <cstddef>
#include <cstdint>
#include <memory>

namespace transfer_switch {

class IncomingObjectSink {
public:
    virtual ~IncomingObjectSink() = default;

    virtual bool write(const void* data, size_t size) = 0;
    virtual bool hasKnownFinalSize() const {
        return false;
    }
    virtual uint64_t finalSize() const {
        return 0;
    }
    virtual bool isComplete() const {
        return false;
    }
    virtual bool finish() = 0;
    virtual void abort() = 0;
    virtual const char* detail() const = 0;
};

class IncomingObjectSinkFactory {
public:
    virtual ~IncomingObjectSinkFactory() = default;

    virtual bool handles(uint32_t storage_id) const = 0;
    virtual std::unique_ptr<IncomingObjectSink> open(
        uint32_t storage_id,
        const char* path,
        uint64_t expected_size
    ) = 0;
    virtual const char* detail() const = 0;
};

}  // namespace transfer_switch

#endif
