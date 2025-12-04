#include "io61.hh"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <thread>
#include <unistd.h>   
#include <fcntl.h>    

//  helper that prints a message and exits nonzero
static void my_assert(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "ASSERT FAILED: %s\n", msg);
        std::fflush(stderr);
        std::exit(1);
    }
}

// Test 1: two shared locks on the SAME range should both succeed!
void test_shared_shared(io61_file* f) {
    // Main thread: take a shared lock
    int r1 = io61_lock(f, 0, 16, LOCK_SH);
    my_assert(r1 == 0, "main LOCK_SH failed");

    bool t_ok = false;

    std::thread t([&] {
        // Second thread: take another SH lock on the same range
        int r2 = io61_lock(f, 0, 16, LOCK_SH);
        my_assert(r2 == 0, "second LOCK_SH failed");

        // Do a read to make sure the lock doesn't break normal I/O
        unsigned char buf[16];
        ssize_t n = io61_pread(f, buf, sizeof(buf), 0);
        my_assert(n >= 0, "io61_pread in SH/SH test failed");

        int u2 = io61_unlock(f, 0, 16);
        my_assert(u2 == 0, "second unlock failed");
        t_ok = true;
    });

    // Main thread can also read while holding SH
    unsigned char buf[16];
    ssize_t n = io61_pread(f, buf, sizeof(buf), 0);
    my_assert(n >= 0, "io61_pread in main SH/SH test failed");

    int u1 = io61_unlock(f, 0, 16);
    my_assert(u1 == 0, "main unlock failed (SH/SH)");

    t.join();
    my_assert(t_ok, "second thread didn't finish SH/SH test");
}

// Test 2: one shared lock the exclusive lock should fail!!
//  - main holds SH on [32, 48)
//  - worker calls try_lock(EX) on [32, 48): should fail with EAGAIN
//  - after main unlocks, worker's blocking LOCK_EX should succeed
void test_shared_then_exclusive(io61_file* f) {
    int r1 = io61_lock(f, 32, 16, LOCK_SH);
    my_assert(r1 == 0, "LOCK_SH in shared-then-exclusive test failed");

    bool ex_acquired = false;

    std::thread t([&] {
        // first: non-blocking EX should fail while SH is held
        errno = 0;
        int r_try = io61_try_lock(f, 32, 16, LOCK_EX);
        my_assert(r_try == -1, "LOCK_EX try_lock unexpectedly succeeded");
        my_assert(errno == EAGAIN, "LOCK_EX try_lock did not set EAGAIN");

        // Now block until we get the EX lock! (after main thread unlocks)
        int r_ex = io61_lock(f, 32, 16, LOCK_EX);
        my_assert(r_ex == 0, "blocking LOCK_EX failed after SH released");

        ex_acquired = true;

        int u = io61_unlock(f, 32, 16);
        my_assert(u == 0, "unlock after EX failed");
    });

    // sleep a bit 
    usleep(50000);  // 50 ms

    int u1 = io61_unlock(f, 32, 16);
    my_assert(u1 == 0, "unlock in shared-then-exclusive test failed");

    t.join();
    my_assert(ex_acquired, "exclusive lock was never acquired by worker thread");
}

int main() {
    const char* filename = "/tmp/io61_sharedlock_test.tmp";

    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
    my_assert(fd >= 0, "open() failed");

    // write some data into the file
    const char data[] = "Hello shared locks!\nThis is some test data.\n";
    ssize_t nw = write(fd, data, sizeof(data));
    my_assert(nw == (ssize_t) sizeof(data), "initial write failed");

    lseek(fd, 0, SEEK_SET);

    io61_file* f = io61_fdopen(fd, O_RDWR);
    my_assert(f != nullptr, "io61_fdopen failed");

    test_shared_shared(f);
    test_shared_then_exclusive(f);

    io61_close(f);
    unlink(filename);

    std::printf("All shared-lock tests passed.\n");
    return 0;
}
