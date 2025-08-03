#include "ast/ast_allocator.hpp"
#include <cstdlib> // for malloc, free, abort
#include <memory>  // for std::align

namespace Myre
{
    AstAllocator::AstAllocator()
    {
        headPage = nullptr;
        currentPage = nullptr;
        new_page();
    }

    AstAllocator::~AstAllocator()
    {
        for (auto page : allPages)
        {
            // Using free since pages are allocated with malloc.
            free(page);
        }
    }

    void AstAllocator::new_page()
    {
        // Allocate memory for a new page struct.
        AstPage* new_page_ptr = (AstPage*)malloc(sizeof(AstPage));
        if (!new_page_ptr)
        {
            // In a real-world compiler, you might throw an exception or
            // have a more robust error handling mechanism.
            abort();
        }

        new_page_ptr->nextPage = nullptr;
        new_page_ptr->currentPtr = new_page_ptr->data;
        new_page_ptr->endPtr = new_page_ptr->data + AstPage::PAGE_SIZE;

        if (currentPage)
        {
            currentPage->nextPage = new_page_ptr;
        }
        else
        {
            headPage = new_page_ptr;
        }
        currentPage = new_page_ptr;
        allPages.push_back(new_page_ptr);
    }

    void* AstAllocator::alloc_bytes(size_t size, size_t alignment)
    {
        // Ensure there is enough space on the current page, considering alignment.
        size_t space = currentPage->endPtr - currentPage->currentPtr;
        void* ptr = currentPage->currentPtr;

        if (std::align(alignment, size, ptr, space))
        {
            // Allocation fits on the current page.
            void* result = ptr;
            currentPage->currentPtr = static_cast<uint8_t*>(ptr) + size;
            return result;
        }

        // Not enough space, so we need a new page.
        new_page();

        // This allocator design does not support single allocations larger than a page.
        // A production-ready version might handle this by allocating a custom-sized page.
        if (size > AstPage::PAGE_SIZE)
        {
            abort();
        }

        // Retry allocation on the new page. This should always succeed.
        space = currentPage->endPtr - currentPage->currentPtr;
        ptr = currentPage->currentPtr;
        if (std::align(alignment, size, ptr, space))
        {
            void* result = ptr;
            currentPage->currentPtr = static_cast<uint8_t*>(ptr) + size;
            return result;
        }

        // This should be unreachable if the logic is correct.
        abort();
        return nullptr;
    }

} // namespace Myre