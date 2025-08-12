#pragma once

#include <cstdint>
#include <atomic>
#include <functional>


struct SymbolHandle
{
    uint32_t id = 0;

    SymbolHandle()
    {
        static std::atomic<uint32_t> global_next_id {1};
        id = global_next_id++;
    }

    SymbolHandle(uint32_t id) : id(id) {}

    // Equality operators required for std::unordered_map
    bool operator==(const SymbolHandle& other) const {
        return id == other.id;
    }

    bool operator!=(const SymbolHandle& other) const {
        return id != other.id;
    }
};

// Hash function specialization for std::unordered_map
namespace std {
    template<>
    struct hash<SymbolHandle> {
        size_t operator()(const SymbolHandle& handle) const noexcept {
            return std::hash<uint32_t>{}(handle.id);
        }
    };
}

