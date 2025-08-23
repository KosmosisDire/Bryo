#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <utility>
#include <new>
#include <span>
#include <string_view>
#include "ast.hpp"

namespace Myre
{

    class Arena
    {
        static constexpr size_t DEFAULT_CHUNK_SIZE = 64 * 1024; // 64KB chunks

        struct Chunk
        {
            std::unique_ptr<uint8_t[]> memory;
            size_t size;
            size_t used;

            Chunk(size_t size) : memory(new uint8_t[size]), size(size), used(0) {}

            void *allocate(size_t bytes, size_t alignment)
            {
                size_t space = size - used;
                void *ptr = memory.get() + used;

                if (std::align(alignment, bytes, ptr, space))
                {
                    size_t offset = static_cast<uint8_t *>(ptr) - (memory.get() + used);
                    used += offset + bytes;
                    return ptr;
                }
                return nullptr;
            }
        };

        std::vector<Chunk> chunks;
        size_t chunkSize;

    public:
        explicit Arena(size_t chunkSize = DEFAULT_CHUNK_SIZE) : chunkSize(chunkSize)
        {
            chunks.reserve(16);
            chunks.emplace_back(chunkSize);
        }

        // Non-copyable, non-movable
        Arena(const Arena &) = delete;
        Arena &operator=(const Arena &) = delete;
        Arena(Arena &&) = delete;
        Arena &operator=(Arena &&) = delete;

        void *allocate(size_t bytes, size_t alignment = alignof(std::max_align_t))
        {
            if (bytes == 0)
                return nullptr;

            // Try current chunk
            if (!chunks.empty())
            {
                if (void *ptr = chunks.back().allocate(bytes, alignment))
                {
                    return ptr;
                }
            }

            // Need new chunk
            size_t newChunkSize = std::max(chunkSize, bytes + alignment);
            chunks.emplace_back(newChunkSize);

            void *result = chunks.back().allocate(bytes, alignment);
            if (!result)
                throw std::bad_alloc();
            return result;
        }

        // Main allocation function for AST nodes
        template <typename T, typename... Args>
        T *make(Args &&...args)
        {
            void *memory = allocate(sizeof(T), alignof(T));
            return new (memory) T(std::forward<Args>(args)...);
        }

        // Create a span from a vector
        template <typename T>
        std::span<T> makeList(const std::vector<T> &vec)
        {
            if (vec.empty())
                return {};

            T *array = static_cast<T *>(allocate(sizeof(T) * vec.size(), alignof(T)));
            std::copy(vec.begin(), vec.end(), array);
            return std::span<T>(array, vec.size());
        }

        // Helper factory methods
        Identifier *makeIdentifier(std::string text)
        {
            auto *id = make<Identifier>();
            id->text = text;
            return id;
        }

        ErrorExpression *makeErrorExpr(std::string message)
        {
            auto *error = make<ErrorExpression>();
            error->message = message;
            error->partialNodes = {};
            return error;
        }

        ErrorStatement *makeErrorStmt(std::string message)
        {
            auto *error = make<ErrorStatement>();
            error->message = message;
            error->partialNodes = {};
            return error;
        }

        // Empty list
        template <typename T>
        std::span<T> emptyList() { return {}; }

        // Memory statistics
        size_t bytesUsed() const
        {
            size_t total = 0;
            for (const auto &chunk : chunks)
            {
                total += chunk.used;
            }
            return total;
        }

        size_t bytesReserved() const
        {
            size_t total = 0;
            for (const auto &chunk : chunks)
            {
                total += chunk.size;
            }
            return total;
        }

        void clear()
        {
            chunks.clear();
            chunks.emplace_back(chunkSize);
        }
    };

} // namespace Myre::AST