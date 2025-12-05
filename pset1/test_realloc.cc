// EXTRA CREDIT
#include "m61.hh"
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstddef>
#include <climits>

static void test_basic_realloc() {
    // TEST1: realloc(nullptr, 10) should behave like malloc(10)!
    char* p = (char*) m61_realloc(nullptr, 10, __FILE__, __LINE__);
    assert(p);

    // Fill with a pattern
    for (int i = 0; i < 10; ++i) {
        p[i] = (char) (i + 1);   // 1,2,...,10
    }

    //TEST 2: When you shrink a block, the first min(old_size, new_size) bytes should be preserved
    // shrink to 5 bytes
    char* q = (char*) m61_realloc(p, 5, __FILE__, __LINE__);
    assert(q);

    // first 5 bytes must be preserved
    for (int i = 0; i < 5; ++i) {
        assert(q[i] == (char) (i + 1));
    }

    // TEST 3: When you grow a block, the first min(old_size, new_size) should be preserved
    // grow to 20 bytes
    char* r = (char*) m61_realloc(q, 20, __FILE__, __LINE__);
    assert(r);

    // first 5 bytes must still be preserved
    for (int i = 0; i < 5; ++i) {
        assert(r[i] == (char) (i + 1));
    }

    //TEST 4: The pointer we get back from realloc must be usable and can be passed to m61_free
    m61_free(r, __FILE__, __LINE__);
}

int main() {
    test_basic_realloc();

    printf("All realloc tests passed.\n");
    m61_print_statistics();
}
