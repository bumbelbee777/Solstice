#include "PtrRuntime.hxx"

#include <atomic>

namespace {
    std::atomic<uint64_t> g_NextPtrId{1};
}

namespace Solstice::Scripting {

    PtrValue MakePtr(const Value& payload) {
        auto header = std::make_shared<PtrHeader>();
        header->Id = g_NextPtrId.fetch_add(1, std::memory_order_relaxed);
        header->Alive = true;
        header->Payload = payload;
        return header;
    }

    void PtrReset(PtrValue& ptr) {
        if (!ptr) {
            return;
        }

        if (ptr->Alive) {
            ptr->Alive = false;
            // Clear payload to allow underlying resources to be reclaimed.
            ptr->Payload = static_cast<int64_t>(0);
        }
    }

    bool PtrIsValid(const PtrValue& ptr) {
        return ptr && ptr->Alive;
    }

    Value PtrGetValue(const PtrValue& ptr) {
        if (!ptr) {
            return static_cast<int64_t>(0);
        }
        return ptr->Payload;
    }

} // namespace Solstice::Scripting

