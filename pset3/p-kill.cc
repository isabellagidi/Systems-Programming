#include "u-lib.hh"

//  tests for the sys_kill system call
// This program checks:
//   1) sys_kill rejects invalid PIDs
//   2) sys_kill can kill a single child
//   3) Killing the same PID twice fails (process was freed)
//   4) sys_kill can kill multiple children
//   5) The parent survives all of this and keeps running

// TEST 1: invalid PID values should all fail
static void test_invalid_pids() {
    // 0 and negative PIDs are invalid
    assert(sys_kill(0) < 0);
    assert(sys_kill(-1) < 0);

    // PIDs >= MAXNPROC are invalid
    assert(sys_kill(MAXNPROC) < 0);
    assert(sys_kill(MAXNPROC + 10) < 0);
}

// TEST 2: fork exactly one child, kill it, verify behavior via return values
static void test_kill_single_child() {
    pid_t parent_pid = sys_getpid();
    assert(parent_pid > 0 && parent_pid < MAXNPROC);

    pid_t child = sys_fork();
    assert(child >= 0 && child < MAXNPROC);

    if (child == 0) {
        // Child: spin forever so that only sys_kill can get rid of us
        // We never call sys_exit() here
        while (true) {
            sys_yield();
        }
    }

    // Parent only reaches here
    // First kill should succeed
    int r1 = sys_kill(child);
    assert(r1 == 0);

    // Second kill of the same PID should fail: process is now free
    int r2 = sys_kill(child);
    assert(r2 < 0);
}

// TEST 3: fork several children, kill them all, and verify return values
static void test_kill_multiple_children() {
    const int NCHILD = 4;
    pid_t children[NCHILD];
    int nchildren = 0;

    for (int i = 0; i < NCHILD; ++i) {
        pid_t p = sys_fork();
        assert(p >= 0 && p < MAXNPROC);

        if (p == 0) {
            // Child: spin forever--Parent will eventually kill 
            while (true) {
                sys_yield();
            }
        } else {
            // Parent: remember this child PID
            children[nchildren++] = p;
        }
    }

    // Only the original parent gets here: all children are stuck in their loops
    for (int i = 0; i < nchildren; ++i) {
        // First kill should succeed
        int r1 = sys_kill(children[i]);
        assert(r1 == 0);

        // Second kill on same PID should fail
        int r2 = sys_kill(children[i]);
        assert(r2 < 0);
    }
}

void process_main() {
    // Basic interface tests (invalid PIDs)
    test_invalid_pids();

    // Test killing exactly one child
    test_kill_single_child();

    // Test killing several children in a row
    test_kill_multiple_children();

    // If we got here, every assert passed, and the kernel did not kill us
    // Just yield forever so the kernel can run other tests if needed
    while (true) {
        sys_yield();
    }
}
