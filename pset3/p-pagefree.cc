#include "u-lib.hh"

extern uint8_t end[];

uintptr_t heap_bottom;
uintptr_t heap_top;
uintptr_t stack_bottom;

// Helper: get a byte pointer into the heap, with sanity checks
inline volatile uint8_t* heap_byte_ptr(uintptr_t addr) {
    assert(addr >= heap_bottom && addr < heap_top);
    return reinterpret_cast<volatile uint8_t*>(addr);
}

// TEST 1: invalid arguments 

static void test_invalid_args() {
    // Below user region (ex: NULL) should be rejected
    int r = sys_page_free(reinterpret_cast<void*>(0));
    assert(r < 0);

    // Unaligned address in user range should be rejected
    // define heap_bottom before calling this test
    uintptr_t unaligned = heap_bottom + 1;
    r = sys_page_free(reinterpret_cast<void*>(unaligned));
    assert(r < 0);
}

//  TEST 2: free on unmapped user address 

static void test_unmapped_free() {
    // `heap_bottom` is a valid user-space address but currently unmapped
    int r = sys_page_free(reinterpret_cast<void*>(heap_bottom));
    assert(r < 0);
}

//  TEST 3: basic alloc + free cycle 

static void test_alloc_free_basic() {
    void* addr = reinterpret_cast<void*>(heap_bottom);

    // Allocate a page at heap_bottom
    int r = sys_page_alloc(addr);
    assert(r == 0);

    // Now the heap has one page mapped
    heap_top = heap_bottom + PAGESIZE;

    // We should be able to write to it
    *heap_byte_ptr(heap_bottom) = 0xAB;
    assert(*heap_byte_ptr(heap_bottom) == 0xAB);

    // Free the page: should succeed
    r = sys_page_free(addr);
    assert(r == 0);

    // Freeing again should fail: the mapping is gone
    r = sys_page_free(addr);
    assert(r < 0);

    // Re-allocate at the same address: should work again
    r = sys_page_alloc(addr);
    assert(r == 0);

    // Update heap_top again (in case another test messed with it later)
    heap_top = heap_bottom + PAGESIZE;

    // New page should be zero-filled
    assert(*heap_byte_ptr(heap_bottom) == 0);
}

void process_main() {
    pid_t self = sys_getpid();
    assert(self > 0 && self < MAXNPROC);

    // Set up heap and stack bounds like your allocator tests
    heap_bottom = round_up(reinterpret_cast<uintptr_t>(end), PAGESIZE);
    heap_top = heap_bottom;
    stack_bottom = round_down(rdrsp() - 1, PAGESIZE);

    // Run tests in a fixed order
    test_invalid_args();
    test_unmapped_free();
    test_alloc_free_basic();

    // If we got here, all asserts passed
    // Yield forever so the kernel can keep scheduling us or others
    while (true) {
        sys_yield();
    }
}
