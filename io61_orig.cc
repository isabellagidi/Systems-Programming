#include "io61.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <climits>
#include <cerrno>

// io61.cc
//    YOUR CODE HERE!


// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.

struct io61_file_orig {
    int fd = -1;     // file descriptor
    int mode;        // open mode (O_RDONLY or O_WRONLY)
};

struct io61_file {
    int fd = -1;     // file descriptor
    int mode;        // open mode (O_RDONLY or O_WRONLY)

    //Phase 2 change!! implement single-slot read cache 
    unsigned char* rbuf = nullptr; // read buffer (single slot)
    size_t rbuf_cap = 0; // capacity
    size_t rbuf_len = 0; // valid bytes currently in rbuf
    size_t rbuf_pos = 0; // next unread index in rbuf
};


// io61_fdopen(fd, mode)
//    Returns a new io61_file for file descriptor `fd`. `mode` is either
//    O_RDONLY for a read-only file or O_WRONLY for a write-only file.
//    You need not support read/write files.

io61_file* io61_fdopen_orig(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = new io61_file;
    f->fd = fd;
    f->mode = mode;
    return f;
}

io61_file* io61_fdopen_phase2(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = new io61_file;
    f->fd = fd;
    f->mode = mode;

    //Phase 2 change!
    if ((mode & O_ACCMODE) == O_RDONLY) {
        static constexpr size_t IO61_RBUFSZ = 64 * 1024; // 64 KiB
        f->rbuf_cap = IO61_RBUFSZ;
        f->rbuf = new unsigned char[f->rbuf_cap];
        f->rbuf_len = 0;
        f->rbuf_pos = 0;
    }

    return f;
}


// io61_close(f)
//    Closes the io61_file `f` and releases all its resources.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    delete f;
    return r;
}


// io61_readc(f)
//    Reads a single (unsigned) byte from `f` and returns it. Returns EOF,
//    which equals -1, on end of file or error.

int io61_readc(io61_file* f) {
    unsigned char ch;
    ssize_t nr = read(f->fd, &ch, 1);
    if (nr != 1) {
        assert(nr == 0 || nr == -1);
        if (nr == 0) {
            errno = 0; // clear `errno` to indicate EOF
        }
        return -1;
    }
    return ch;
}

// PHASE 2
int io61_readc(io61_file* f) {
    // if buffer empty, refill
    while (f->rbuf_pos == f->rbuf_len) {
        if ((f->mode & O_ACCMODE) != O_RDONLY) {
            errno = EBADF;
            return -1;
        }
        // refill buffer
        ssize_t nr;
        do {
            nr = read(f->fd, f->rbuf, f->rbuf_cap);
        } while (nr < 0 && errno == EINTR);

        if (nr <= 0) {
            // EOF or error
            if (nr == 0) {
                errno = 0;  // clean EOF
            }
            return -1;
        }
        f->rbuf_len = (size_t) nr;
        f->rbuf_pos = 0;
    }

    // serve from cache
    return f->rbuf[f->rbuf_pos++];
}

// io61_read(f, buf, sz)
//    Reads up to `sz` bytes from `f` into `buf`. Returns the number of
//    bytes read on success. Returns 0 if end-of-file is encountered before
//    any bytes are read, and -1 if an error is encountered before any
//    bytes are read.
//
//    Note that the return value might be positive, but less than `sz`,
//    if end-of-file or error is encountered before all `sz` bytes are read.
//    This is called a “short read.”


ssize_t io61_read(io61_file* f, unsigned char* buf, size_t sz) {
    size_t nread = 0;
    while (nread != sz) {
        int ch = io61_readc(f);
        if (ch == EOF) {
            break;
        }
        buf[nread] = ch;
        ++nread;
    }
    if (nread == 0 && sz != 0 && errno != 0) {
        return -1;
    }
    return nread;
}

ssize_t io61_read_phase1(io61_file* f, unsigned char* buf, size_t sz) {
    if (sz == 0) return 0;
    size_t nread = 0;

    while (nread < sz) {
        ssize_t r = read(f->fd, buf + nread, sz - nread);
        if (r > 0) {
            nread += r;
            continue;
        }
        if (r == 0) {              // EOF
            if (nread == 0) errno = 0;   
            break;
        }
        // r < 0: error
        if (errno == EINTR) continue;          // retry on interrupt
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // nonblocking: return what we have (might be 0)
            return nread > 0 ? (ssize_t) nread : -1;
        }
        // permanent error
        return nread > 0 ? (ssize_t) nread : -1;
    }
    return (ssize_t) nread;
}

// PHASE 2
ssize_t io61_read(io61_file* f, unsigned char* buf, size_t sz) {
    if (sz == 0) return 0;
    size_t nread = 0;

    while (nread != sz) {
        // Drain the cache
        //draining the cache guarantees sequential order
        if (f->rbuf_pos < f->rbuf_len) {
            size_t avail = f->rbuf_len - f->rbuf_pos;
            size_t take = std::min(avail, sz - nread);
            std::memcpy(buf + nread, f->rbuf + f->rbuf_pos, take);
            f->rbuf_pos += take;
            nread += take;
            continue;
        }

        // 2) ff remaining request is large, read directly into user buffer
        size_t remain = sz - nread;
        if (remain >= f->rbuf_cap) {
            ssize_t nr = read(f->fd, buf + nread, remain);
            if (nr > 0) {
                nread += (size_t) nr;
                continue;
            }
            if (nr == 0) {  // EOF
                if (nread == 0) errno = 0; // clean EOF if nothing read
                break;
            }
            // error
            if (errno == EINTR) continue;  // retry interrupts
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return nread > 0 ? (ssize_t) nread : -1;
            return nread > 0 ? (ssize_t) nread : -1;
        }

        // otherwise, refill cache and copy from it
        ssize_t nr;
        do {
            nr = read(f->fd, f->rbuf, f->rbuf_cap);
        } while (nr < 0 && errno == EINTR);

        if (nr <= 0) {
            if (nr == 0) {  // EOF
                if (nread == 0) errno = 0; // clean EOF if nothing read
                break;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return nread > 0 ? (ssize_t) nread : -1;
            return nread > 0 ? (ssize_t) nread : -1;
        }

        f->rbuf_len = (size_t) nr;
        f->rbuf_pos = 0;
        // next loop iteration will drain from cache
    }

    return (ssize_t) nread;
}


// io61_writec(f)
//    Write a single character `c` to `f` (converted to unsigned char).
//    Returns 0 on success and -1 on error.

int io61_writec(io61_file* f, int c) {
    unsigned char ch = c;
    ssize_t nw = write(f->fd, &ch, 1);
    if (nw != 1) {
        assert(nw == 0 || nw == -1);
        return -1;
    }
    return 0;
}




// io61_write(f, buf, sz)
//    Writes `sz` characters from `buf` to `f`. Returns `sz` on success.
//    Can write fewer than `sz` characters when there is an error, such as
//    a drive running out of space. In this case io61_write returns the
//    number of characters written, or -1 if no characters were written
//    before the error occurred.

ssize_t io61_write(io61_file* f, const unsigned char* buf, size_t sz) {
    size_t nwritten = 0;
    while (nwritten != sz) {
        if (io61_writec(f, buf[nwritten]) == -1) {
            break;
        }
        ++nwritten;
    }
    if (nwritten == 0 && sz != 0) {
        return -1;
    }
    return nwritten;
}

ssize_t io61_write_phase1(io61_file* f, const unsigned char* buf, size_t sz) {
    size_t nwritten = 0;

    while (nwritten < sz) {
        ssize_t nw = write(f->fd, buf + nwritten, sz - nwritten);
        if (nw > 0) {
            nwritten += nw;
            continue;
        }
        if (nw == 0) {
            return nwritten > 0 ? (ssize_t) nwritten : -1;
        }
        // w < 0: error
        if (errno == EINTR) continue;                 // retry on interrupt
        if (errno == EAGAIN || errno == EWOULDBLOCK)  // nonblocking
            return nwritten > 0 ? (ssize_t) nwritten : -1;
        return nwritten > 0 ? (ssize_t) nwritten : -1;
    }
    return (ssize_t) nwritten;
}


// io61_flush(f)
//    If `f` was opened write-only, `io61_flush(f)` forces a write of any
//    cached data written to `f`. Returns 0 on success; returns -1 if an error
//    is encountered before all cached data was written.
//
//    If `f` was opened read-only, `io61_flush(f)` returns 0. It may also
//    drop any data cached for reading.

int io61_flush(io61_file* f) {
    (void) f;
    return 0;
}

// PHASE 2
int io61_flush(io61_file* f) {
    // phase 2 read cache change
    if ((f->mode & O_ACCMODE) == O_RDONLY) {
        // drop any read cache
        f->rbuf_len = f->rbuf_pos = 0;
        return 0;
    }
    return 0;
}

// io61_seek(f, off)
//    Changes the file pointer for file `f` to `off` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, off_t off) {
    off_t r = lseek(f->fd, (off_t) off, SEEK_SET);
    // Ignore the returned offset unless it’s an error.
    if (r == -1) {
        return -1;
    }
    return 0;
}


// You shouldn't need to change these functions.

// io61_open_check(filename, mode)
//    Opens the file corresponding to `filename` and returns its io61_file.
//    If `!filename`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != nullptr` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    if (filename) {
        fd = open(filename, mode, 0666);
    } else if ((mode & O_ACCMODE) == O_RDONLY) {
        fd = STDIN_FILENO;
    } else {
        fd = STDOUT_FILENO;
    }
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode & O_ACCMODE);
}


// io61_fileno(f)
//    Returns the file descriptor associated with `f`.

int io61_fileno(io61_file* f) {
    return f->fd;
}


// io61_filesize(f)
//    Returns the size of `f` in bytes. Returns -1 if `f` does not have a
//    well-defined size (for instance, if it is a pipe).

off_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r < 0 || !S_ISREG(s.st_mode)) {
        return -1;
    }
    return s.st_size;
}
