#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cstring> // Include for memset

namespace Mycelium::Scripting::Lang
{
    class AstAllocator;

    // A single page of memory for the allocator.
    // It's a plain data structure managed by the AstAllocator.
    class AstPage
    {
    public:
        // Using a common page size, leaving some room for metadata.
        // This size can be tuned for performance.
        static const int PAGE_SIZE = 4096 - 64;

        AstPage* nextPage;
        uint8_t* currentPtr;
        uint8_t* endPtr;
        uint8_t data[PAGE_SIZE];
    };

    // A page-based bump allocator specifically for AST nodes.
    // It allocates memory in large chunks (pages) and "bumps" a pointer
    // for each new allocation. This is significantly faster than heap allocation.
    class AstAllocator
    {
    private:
        AstPage* headPage;
        AstPage* currentPage;
        std::vector<AstPage*> allPages; // For easy cleanup

        void new_page();

    public:
        AstAllocator();
        ~AstAllocator();

        // Disallow copy and move semantics to prevent accidental duplication of the
        // allocator and its managed memory.
        AstAllocator(const AstAllocator&) = delete;
        AstAllocator& operator=(const AstAllocator&) = delete;
        AstAllocator(AstAllocator&&) = delete;
        AstAllocator& operator=(AstAllocator&&) = delete;

        // Allocates raw memory of a given size and alignment.
        void* alloc_bytes(size_t size, size_t alignment);

        // Allocates and default-constructs an object of type T using placement new.
        template <typename T>
        T* alloc()
        {
            void* mem = alloc_bytes(sizeof(T), alignof(T));
            
            // FIX: Zero-initialize the allocated memory before construction.
            // This ensures all pointer members are nullptr by default, preventing
            // crashes from uninitialized pointers.
            memset(mem, 0, sizeof(T));
            
            return new (mem) T();
        }
    };

} // namespace Mycelium::Scripting::Lang