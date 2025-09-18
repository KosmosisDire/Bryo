#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <new>
#include "bound_tree.hpp"

namespace Bryo
{
    class BindingArena
    {
        static constexpr size_t DEFAULT_CHUNK_SIZE = 128 * 1024; // 128KB for bound nodes

        struct Chunk
        {
            std::unique_ptr<uint8_t[]> memory;
            size_t size;
            size_t used;

            Chunk(size_t size) : memory(new uint8_t[size]), size(size), used(0) {}

            void* allocate(size_t bytes, size_t alignment)
            {
                size_t space = size - used;
                void* ptr = memory.get() + used;

                if (std::align(alignment, bytes, ptr, space))
                {
                    size_t offset = static_cast<uint8_t*>(ptr) - (memory.get() + used);
                    used += offset + bytes;
                    return ptr;
                }
                return nullptr;
            }
        };

        std::vector<Chunk> chunks;
        size_t chunkSize;

    public:
        explicit BindingArena(size_t chunkSize = DEFAULT_CHUNK_SIZE) : chunkSize(chunkSize)
        {
            chunks.reserve(32);
            chunks.emplace_back(chunkSize);
        }

        // Non-copyable, non-movable
        BindingArena(const BindingArena&) = delete;
        BindingArena& operator=(const BindingArena&) = delete;
        BindingArena(BindingArena&&) = delete;
        BindingArena& operator=(BindingArena&&) = delete;

        void* allocate(size_t bytes, size_t alignment = alignof(std::max_align_t))
        {
            if (bytes == 0) return nullptr;

            // Try current chunk
            if (!chunks.empty())
            {
                if (void* ptr = chunks.back().allocate(bytes, alignment))
                    return ptr;
            }

            // Need new chunk
            size_t newChunkSize = std::max(chunkSize, bytes + alignment);
            chunks.emplace_back(newChunkSize);

            void* result = chunks.back().allocate(bytes, alignment);
            if (!result) throw std::bad_alloc();
            return result;
        }

        // Main allocation function for bound nodes
        template<typename T, typename... Args>
        T* make(Args&&... args)
        {
            void* memory = allocate(sizeof(T), alignof(T));
            return new (memory) T(std::forward<Args>(args)...);
        }

        // Create vector in arena
        template<typename T>
        std::vector<T>* makeVector()
        {
            void* memory = allocate(sizeof(std::vector<T>), alignof(std::vector<T>));
            return new (memory) std::vector<T>();
        }

        size_t bytesUsed() const
        {
            size_t total = 0;
            for (const auto& chunk : chunks)
                total += chunk.used;
            return total;
        }
    };
}