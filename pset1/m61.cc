#include "m61.hh"
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <cassert>
#include <sys/mman.h>
#include <map>

static m61_statistics global_stats = {
    .nactive = 0,
    .active_size = 0,
    .ntotal = 0,
    .total_size = 0,
    .nfail = 0,
    .fail_size = 0,
    .heap_min = 0,
    .heap_max = 0
};

std::map<void*, size_t> active_sizes;
std::map<void*, size_t> freed_allocations;

struct m61_memory_buffer {
    char* buffer;
    size_t pos = 0;
    size_t size = 8 << 20; /* 8 MiB */

    m61_memory_buffer();
    ~m61_memory_buffer();
};

static m61_memory_buffer default_buffer;


m61_memory_buffer::m61_memory_buffer() {
    void* buf = mmap(nullptr,    // Place the buffer at a random address
        this->size,              // Buffer should be 8 MiB big
        PROT_WRITE,              // We want to read and write the buffer
        MAP_ANON | MAP_PRIVATE, -1, 0);
                                 // We want memory freshly allocated by the OS
    assert(buf != MAP_FAILED);
    this->buffer = (char*) buf;
}

m61_memory_buffer::~m61_memory_buffer() {
    munmap(this->buffer, this->size);
}



static void* m61_find_free_space(size_t sz) {
    // do we have a freed allocation that will work?

    for (auto it = freed_allocations.begin(); it != freed_allocations.end(); it++){
        if(it->second >= sz){
            void* ptr = it -> first;
            freed_allocations.erase(ptr);
            return ptr;
        }
    }
    // otherwise fail
    return nullptr;
}


/// m61_malloc(sz, file, line)
///    Returns a pointer to `sz` bytes of freshly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc may
///    return either `nullptr` or a pointer to a unique allocation.
///    The allocation request was made at source code location `file`:`line`.

void* m61_malloc(size_t sz, const char* file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.

    void* ptr = m61_find_free_space(sz);

    if (ptr != nullptr) {
        active_sizes.insert({ptr, sz});
        ++global_stats.ntotal;
        ++global_stats.nactive;
        global_stats.total_size+=sz;
        global_stats.active_size+=sz;
        return ptr;
    }

    // test 8 !!
    size_t align = alignof(std::max_align_t);
    size_t aligned_sz = (sz + align - 1) & ~(align - 1);

    //test 18
    if (default_buffer.size - default_buffer.pos < sz){
        ++global_stats.nfail;
        global_stats.fail_size+=sz;
        return nullptr;
    }

    ++global_stats.ntotal;
    ++global_stats.nactive;
    global_stats.total_size+=sz;
    global_stats.active_size+=sz;

    // Otherwise there is enough space; claim the next `sz` bytes
    ptr = &default_buffer.buffer[default_buffer.pos];
    default_buffer.pos += aligned_sz; //test 8!!!

    active_sizes.insert({ptr, sz});

    //test 10
    uintptr_t addr = (uintptr_t) ptr;

    if (global_stats.ntotal == 1){
        global_stats.heap_min = addr;
    }

    if (global_stats.ntotal == 1){
        global_stats.heap_max = addr + sz - 1;
    }

    if (addr < global_stats.heap_min){
        global_stats.heap_min = addr;
    }

    if (addr > global_stats.heap_max){
        global_stats.heap_max = addr + sz - 1;
    }

    return ptr;
}


/// m61_free(ptr, file, line)
///    Frees the memory allocation pointed to by `ptr`. If `ptr == nullptr`,
///    does nothing. Otherwise, `ptr` must point to a currently active
///    allocation returned by `m61_malloc`. The free was called at location
///    `file`:`line`.

void m61_free(void* ptr, const char* file, int line) {
    // avoid uninitialized variable warnings
    if (ptr == nullptr){
        return;
    }

    (void) ptr, (void) file, (void) line;
    --global_stats.nactive;
    // Your code here. The handout code does nothing!


    auto it = active_sizes.find(ptr);
    size_t sz = it -> second;
    global_stats.active_size -= sz;

    freed_allocations.insert({ptr, sz});

    active_sizes.erase(ptr);
    
}


/// m61_calloc(count, sz, file, line)
///    Returns a pointer a fresh dynamic memory allocation big enough to
///    hold an array of `count` elements of `sz` bytes each. Returned
///    memory is initialized to zero. The allocation request was at
///    location `file`:`line`. Returns `nullptr` if out of memory; may
///    also return `nullptr` if `count == 0` or `size == 0`.

void* m61_calloc(size_t count, size_t sz, const char* file, int line) {
    // Your code here (not needed for first tests).


    //test 19 a
    if (count == 0 || sz==0){
        return m61_malloc(0);
    }

    //test 19
    if (sz > (SIZE_MAX / count)){
        ++global_stats.nfail;
        global_stats.fail_size+=count*sz;
        return nullptr;
    }

    void* ptr = m61_malloc(count * sz, file, line);
    if (ptr) {
        memset(ptr, 0, count * sz);
    }
    return ptr;
}


/// m61_get_statistics()
///    Return the current memory statistics.

m61_statistics m61_get_statistics() {
    // Your code here.
    // The handout code sets all statistics to enormous numbers.
    m61_statistics stats;
    // this is what changed for test 0!! (used to have 255 instead of 0)
    // is the same as just changing each variable individually
    memset(&stats, 0, sizeof(m61_statistics));

    //set to global_stats
    stats=global_stats;
    
    return stats;
}


/// m61_print_statistics()
///    Prints the current memory statistics.

void m61_print_statistics() {
    m61_statistics stats = m61_get_statistics();
    printf("alloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("alloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}


/// m61_print_leak_report()
///    Prints a report of all currently-active allocated blocks of dynamic
///    memory.

void m61_print_leak_report() {
    // Your code here.
}
