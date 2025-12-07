#include "u-lib.hh"

// Tests for sys_sleep(ticks)
//
// We can’t directly read kernel tick counters from user space, so these
// tests focus on:
//   1) sys_sleep(0) behaving like a yield and returning normally
//   2) sys_sleep(n) returning without crashing
//   3) sys_sleep working correctly across parent/child processes,
//      using the console as a simple observable side effect


// TEST 1: sys_sleep(0) should just behave like a yield and return 0
static void test_sleep_zero() {
    int counter = 0;
    for (int i = 0; i < 10; ++i) {
        ++counter;
        int r = sys_sleep(0);
        assert(r == 0);
    }
    // If sys_sleep(0) never returned, we wouldn't reach here
    assert(counter == 10);
}


// TEST 2: sys_sleep with positive ticks should return 0 and let us progress
static void test_sleep_positive() {
    int steps = 0;
    for (int i = 0; i < 5; ++i) {
        ++steps;
        int r = sys_sleep(5);   // sleep for a few timer ticks
        assert(r == 0);
    }
    assert(steps == 5);
}


// TEST 3: multi-process sleep behavior using the console as a shared observable
//
//  - Parent forks a child
//  - Child sleeps in chunks, then writes 'C' into the console
//  - Parent sleeps longer than the child’s total sleep and then checks that
//    the console cell was updated. If sys_sleep or waking is broken, the
//    console may never be updated, and the assert will fail
static void test_sleep_multi_process() {
    pid_t parent = sys_getpid();
    assert(parent > 0 && parent < MAXNPROC);

    // Use a known console position as our shared flag
    const int row = 10;
    const int col = 0;
    console[CPOS(row, col)] = ' ';   // clear it

    pid_t child = sys_fork();
    assert(child >= 0 && child < MAXNPROC);

    if (child == 0) {
        // child
        // Sleep in several chunks
        for (int i = 0; i < 5; ++i) {
            int r = sys_sleep(5);
            assert(r == 0);
        }
        // Mark that we successfully woke and ran
        console[CPOS(row, col)] = 'C';
        sys_exit();
    } else {
        // parent
        // Sleep longer than the child's total sleep (5 * 5 = 25 ticks)
        // 50 ticks should be plenty
        int r = sys_sleep(50);
        assert(r == 0);

        // By now, the child should have woken, run, and written 'C'
        assert(console[CPOS(row, col)] == 'C');
    }
}


void process_main() {
    pid_t self = sys_getpid();
    assert(self > 0 && self < MAXNPROC);

    // Run tests in a fixed order
    test_sleep_zero();
    test_sleep_positive();
    test_sleep_multi_process();

    // If we got here, all asserts passed and the kernel didn't crash us
    // Yield forever so the OS can keep running
    while (true) {
        sys_yield();
    }
}
