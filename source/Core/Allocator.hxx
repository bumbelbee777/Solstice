#pragma once

#include <Core/Async.hxx>
#include <vector>
#include <algorithm>
#include <cstddef>

namespace Solstice::Core {

struct ArenaAllocator {
	std::vector<std::byte> Buffer;
	size_t Offset = 0;

	explicit ArenaAllocator(size_t Capacity = 1 << 20) { Buffer.resize(Capacity); }

	void Reset() { Offset = 0; }

    std::byte* Allocate(size_t Count, size_t Alignment = alignof(std::max_align_t)) {
        size_t CurrentAddress = reinterpret_cast<size_t>(Buffer.data() + Offset);
        size_t AlignOffset = (Alignment - (CurrentAddress % Alignment)) % Alignment;
        size_t Needed = Offset + AlignOffset + Count;

        if (Needed > Buffer.size()) {
            size_t NewSize = std::max(Buffer.size() * 2, Needed);
            Buffer.resize(NewSize);
        }

        std::byte* Ptr = Buffer.data() + Offset + AlignOffset;
        Offset = Needed;
        return Ptr;
    }

	template<class T> T* AllocateObject(size_t Count = 1) {
		return reinterpret_cast<T*>(Allocate(sizeof(T) * Count, alignof(T)));
	}
};


template<class T> struct PoolAllocator {
    struct Node {
        Node* Next = nullptr;
    };

    std::vector<std::byte> Buffer;
    Node* FreeList = nullptr;
    size_t BlockSize;
    size_t Capacity;
    size_t Used = 0;

    Spinlock Lock; // optional for multithreaded access

    explicit PoolAllocator(size_t Capacity)
        : BlockSize(sizeof(T) > sizeof(Node) ? sizeof(T) : sizeof(Node)),
        Capacity(Capacity) {
        Buffer.resize(BlockSize * Capacity);
        // initialize free list
        FreeList = nullptr;
        for (size_t i = 0; i < Capacity; ++i) {
            Node* node = reinterpret_cast<Node*>(Buffer.data() + i * BlockSize);
            node->Next = FreeList;
            FreeList = node;
        }
    }

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    PoolAllocator(PoolAllocator&& Other) noexcept {
        if(this != &Other) {
            Buffer = std::move(Other.Buffer);
            FreeList = Other.FreeList;
            BlockSize = Other.BlockSize;
            Capacity = Other.Capacity;
            Used = Other.Used;
            Other.FreeList = nullptr;
            Other.Used = 0;
        }
    }

    T* Allocate() {
        Lock.Lock();
        if (!FreeList) {
            Lock.Unlock();
            return nullptr; // pool exhausted
        }
        Node* Node = FreeList;
        FreeList = FreeList->Next;
        ++Used;
        Lock.Unlock();
        return reinterpret_cast<T*>(Node);
    }

    void Free(T* Ptr) {
        if (!Ptr) return;
        Lock.Lock();
        Node* Node_ = reinterpret_cast<Node*>(Ptr);
        Node_->Next = FreeList;
        FreeList = Node_;
        --Used;
        Lock.Unlock();
    }

    size_t Available() const { return Capacity - Used; }
};

template<typename T>
struct PerthreadAllocator {
    static thread_local PoolAllocator<T>* LocalPool;

    static void SetThreadPool(PoolAllocator<T>* Pool) { LocalPool = Pool; }

    static T* Allocate() { return LocalPool->Allocate(); }
    static void Free(T* Ptr) { LocalPool->Free(Ptr); }
};

template<class T>
inline thread_local PoolAllocator<T>* PerthreadAllocator<T>::LocalPool = nullptr;

}