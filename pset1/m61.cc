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

// start a struct that keeps track of both size and aligned size (Phase 2)

struct active_aligned_sizes {
    size_t sz; //need to keep track of real size for stats
    size_t aligned_sz; // need to keep track of aligned size for reuse
};

std::map<void*, active_aligned_sizes> active_sizes;
std::map<void*, size_t> freed_allocations;
std::map<void*, size_t> un_coalesced_freed_allocations;

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


//sz is known to be aligned sz
static void* m61_find_free_space(size_t sz) {
    // do we have a freed allocation that will work?

    for (auto it = freed_allocations.begin(); it != freed_allocations.end(); it++){
        if(it->second >= sz){
            void* ptr = it -> first;
            size_t block_size = it -> second;

            freed_allocations.erase(it); // could also do .erase(ptr)

            //test27

            if (block_size > sz) {
                void* remainder_ptr = (char*)ptr + sz;
                size_t remainder_size = block_size - sz;
                freed_allocations.insert({remainder_ptr, remainder_size});
            }
            return ptr;
        }
    }
    // otherwise fail
    return nullptr;
}


bool align_overflow(size_t sz){
    const size_t align = alignof(std::max_align_t);
    size_t rem = sz % align;
    size_t add = 0;
    if (rem !=0){
        add = align - rem;
    } 

    if (SIZE_MAX - sz < add) return true; // overflow has ocurred!

    return false;
}
//alignment
//using bitwise arithmetic
size_t align_size(size_t sz){
    size_t align = alignof(std::max_align_t);
    size_t aligned_sz = (sz + align - 1) & ~(align - 1);
    return aligned_sz;
}

/// m61_malloc(sz, file, line)
///    Returns a pointer to `sz` bytes of freshly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc may
///    return either `nullptr` or a pointer to a unique allocation.
///    The allocation request was made at source code location `file`:`line`.

void* m61_malloc(size_t sz, const char* file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.

    if (sz==0){
        ++global_stats.ntotal;
        return nullptr;
    }

    if (align_overflow(sz)){
        ++global_stats.nfail;
        global_stats.fail_size +=sz;
        return nullptr;
    }

    size_t aligned_sz = align_size(sz);

    void* ptr = m61_find_free_space(aligned_sz);


    if (ptr != nullptr) {
        active_sizes.insert({ptr, {sz, aligned_sz}});
        ++global_stats.ntotal;
        ++global_stats.nactive;
        global_stats.total_size+=sz;
        global_stats.active_size+=sz;

        //test 46 on
        //boundary write error detection

        size_t padded_sz = aligned_sz - sz;

        if (padded_sz != 0){
            char* padding_start_ptr = (char*)ptr + sz;
            memset(padding_start_ptr, 0xAB, padded_sz);
        }

        return ptr;
    }


    //test 18
    if (default_buffer.size - default_buffer.pos < aligned_sz){
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

    //don't use aligned size
    active_sizes.insert({ptr, {sz, aligned_sz}});

    //test 46 on
    //boundary write error detection

    size_t padded_sz = aligned_sz - sz;

    if (padded_sz != 0){
        char* padding_start_ptr = (char*)ptr + sz;
        memset(padding_start_ptr, 0xAB, padded_sz);
    }


    //test 10
    uintptr_t addr = (uintptr_t) ptr;

    if (global_stats.ntotal == 1){
        global_stats.heap_min = addr;
    }

    //using alignment
    if (global_stats.ntotal == 1){
        global_stats.heap_max = addr + aligned_sz - 1;
    }

    if (addr < global_stats.heap_min){
        global_stats.heap_min = addr;
    }

    //using alignment
    if (addr > global_stats.heap_max){
        global_stats.heap_max = addr + aligned_sz - 1;
    }

    //using alignment
    //CHANGED test 29
    if ((addr + aligned_sz - 1) > global_stats.heap_max){
        global_stats.heap_max = addr + aligned_sz - 1;
    }

    return ptr;
}

// helper function for coalesing test 27-33

void m61_coalesce(std::map<void*, size_t >& freed_allocations_local, void* ptr, size_t sz) {
    auto it = freed_allocations_local.find(ptr);
    if (it == freed_allocations_local.end()) return;
    
    char* new_ptr = (char*)ptr;
    size_t new_size = sz;

    if (it != freed_allocations_local.begin()){
        auto prev_it = std::prev(it);
        void* prev_ptr = prev_it -> first;
        size_t prev_sz = prev_it -> second;

        if ((char*)prev_ptr + prev_sz == new_ptr) {
            new_ptr = (char*)prev_ptr;
            new_size += prev_sz;
            freed_allocations_local.erase(prev_it);
    }
    }
    auto next_it = std::next(it);
    if (next_it != freed_allocations_local.end()){
        void* next_ptr = next_it -> first;
        size_t next_sz = next_it -> second;

        if (new_ptr + new_size == (char*)next_ptr) {
            // new_ptr stays the same
            new_size += next_sz;
            freed_allocations_local.erase(next_it);
        }

    }

    freed_allocations_local.erase(it);

    // WILDERNESS COALESCE - test 29ç
    uintptr_t buffer_begin = (uintptr_t) default_buffer.buffer + default_buffer.pos;
    if ((uintptr_t)new_ptr + new_size == buffer_begin){
        default_buffer.pos -= new_size;
        return;
    }

    freed_allocations_local.insert({new_ptr, new_size});

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

    //phase 3 tests 34-44
    // check if pointer is inside heap range test 35-37
    if ((char*)ptr < (char*)global_stats.heap_min || ((char*)ptr > (char*)global_stats.heap_max)){
            fprintf(stderr, "MEMORY BUG: %s:%d: invalid free of pointer %p, not in heap\n", file, line, ptr);
            abort();
    }

    auto it = active_sizes.find(ptr);
        
    // check if pointer is valid! () test 40-42 (?)
    if (it == active_sizes.end()){

        //for double free detection -- after you check it is not in active_sizes -- see if it is in freed_allocations
        //test 38-39
        // created un_coalesced_freed_allocations so that it works even with coalescing!!
        if (un_coalesced_freed_allocations.find(ptr) != un_coalesced_freed_allocations.end()){
            fprintf(stderr, "MEMORY BUG: %s:%d: invalid free of pointer %p, double free\n", file, line, ptr);
            abort();
        }

        fprintf(stderr, "MEMORY BUG: %s:%d: invalid free of pointer %p, not allocated\n", file, line, ptr);
        abort();
    }


    (void) ptr, (void) file, (void) line;

    // Your code here. The handout code does nothing!


    
    size_t user_sz = it->second.sz;
    size_t aligned_sz = it->second.aligned_sz;
    char* padding_start_ptr = (char*)ptr + user_sz;
    size_t padded_sz  = aligned_sz - user_sz;
    
    unsigned char* c = (unsigned char*) padding_start_ptr;
    for (size_t i = 0; i < padded_sz; i++) {
        if (c[i] != 0xAB) {
            fprintf(stderr,
                "MEMORY BUG: %s:%d: detected wild write during free of pointer %p\n", file, line, ptr);
            abort();
        }
    }


    --global_stats.nactive;

    active_aligned_sizes sz = it -> second;
    global_stats.active_size -= sz.sz;

    //need to use aligned size!!

    freed_allocations.insert({ptr, sz.aligned_sz});
    un_coalesced_freed_allocations.insert({ptr, sz.aligned_sz});

    m61_coalesce(freed_allocations, ptr, sz.aligned_sz);

    active_sizes.erase(it);
    
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
