#include "io61.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <climits>
#include <cerrno>
#include <cstring> // memcpy (Phase 2)
#include <algorithm> // std::min (Phase 2)
#include <unistd.h>    // read, write, lseek


// io61.cc
//    YOUR CODE HERE!



// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.
//PHASE 2
// PHASE 3
struct io61_file {
    int fd = -1;     // file descriptor
    int mode;        // open mode (O_RDONLY or O_WRONLY)

    //Phase 2 change!! implement single-slot read cache 
    unsigned char* rbuf = nullptr; // read buffer (single slot)
    size_t rbuf_cap = 0; // capacity
    size_t rbuf_len = 0; // valid bytes currently in rbuf
    size_t rbuf_pos = 0; // next unread index in rbuf

    unsigned char* wbuf = nullptr;
    size_t wbuf_cap = 0;
    size_t wbuf_len = 0; // bytes staged in wbuf
    size_t wbuf_pos = 0;  // bytes already flushed from the front
};


// io61_fdopen(fd, mode)
//    Returns a new io61_file for file descriptor `fd`. `mode` is either
//    O_RDONLY for a read-only file or O_WRONLY for a write-only file.
//    You need not support read/write files.
// PHASE 2
io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = new io61_file;
    f->fd = fd;
    f->mode = (mode & O_ACCMODE);

    //Phase 2 change! (read cache)
    if (f->mode == O_RDONLY) {
        static constexpr size_t IO61_RBUFSZ = 64 * 1024; // 64 KiB
        f->rbuf_cap = IO61_RBUFSZ;
        f->rbuf = new unsigned char[f->rbuf_cap];
        f->rbuf_len = 0;
        f->rbuf_pos = 0;
    }

    // Phase 2 change! (write cache)
    if (f->mode == O_WRONLY) {
        static constexpr size_t IO61_WBUFSZ = 64 * 1024;   // try 64–128 KiB
        f->wbuf_cap = IO61_WBUFSZ;
        f->wbuf = new unsigned char[f->wbuf_cap];
        f->wbuf_len = 0;
        f->wbuf_pos = 0;
    }

    return f;
}


// io61_close(f)
//    Closes the io61_file `f` and releases all its resources.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    delete[] f->rbuf;   // Phase 2: free read buffer if allocated
    delete[] f->wbuf; // Phase 3!
    delete f;
    return r;
}


// io61_readc(f)
//    Reads a single (unsigned) byte from `f` and returns it. Returns EOF,
//    which equals -1, on end of file or error.
// PHASE 2
int io61_readc(io61_file* f) {
    //guard
    if (f->mode != O_RDONLY) { 
        errno = EBADF; 
        return -1; 
    }
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

// PHASE 1
// PHASE 2
ssize_t io61_read(io61_file* f, unsigned char* buf, size_t sz) {
    // guard
    if ((f->mode & O_ACCMODE) != O_RDONLY) {
        errno = EBADF;
        return -1;
    }

    if (sz == 0) return 0;
    size_t nread = 0;

    while (nread != sz) {
        // drain the cache
        // guarantees sequential order
        if (f->rbuf_pos < f->rbuf_len) {
            size_t avail = f->rbuf_len - f->rbuf_pos;
            size_t take = std::min(avail, sz - nread);
            std::memcpy(buf + nread, f->rbuf + f->rbuf_pos, take);
            f->rbuf_pos += take;
            nread += take;
            continue;
        }

        // ff remaining request is large, read directly into user buffer
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

        // if request small, refill cache and copy from it
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
        // next loop iteration will drain from cache!!
    }

    return (ssize_t) nread;
}




// io61_writec(f)
//    Write a single character `c` to `f` (converted to unsigned char).
//    Returns 0 on success and -1 on error.
// PHASE 3
// write cache

int io61_writec(io61_file* f, int c) {
    if ((f->mode & O_ACCMODE) != O_WRONLY) { 
        errno = EBADF; 
        return -1; 
    }

    // if full, flush first
    if (f->wbuf_len == f->wbuf_cap) {
        if (io61_flush(f) < 0) {
            return -1; // for retry
        }
    }
    f->wbuf[f->wbuf_len++] = (unsigned char) c;
    return 0;
}



// io61_write(f, buf, sz)
//    Writes `sz` characters from `buf` to `f`. Returns `sz` on success.
//    Can write fewer than `sz` characters when there is an error, such as
//    a drive running out of space. In this case io61_write returns the
//    number of characters written, or -1 if no characters were written
//    before the error occurred.

//PHASE 1
//PHASE 3 (mirrors io61_read)
ssize_t io61_write(io61_file* f, const unsigned char* buf, size_t sz) {
    // guard
    if ((f->mode & O_ACCMODE) != O_WRONLY) {
        errno = EBADF;
        return -1;
    }
    size_t nwritten = 0;

    while (nwritten < sz) {
        size_t remain = sz - nwritten;

        // if request is large, flush any staged bytes, then write directly
        if (remain >= f->wbuf_cap) {
            if (f->wbuf_len > f->wbuf_pos) {
                if (io61_flush(f) < 0) {
                    return nwritten > 0 ? (ssize_t) nwritten : -1;
                }
            }
            // direct write loop 
            while (remain >= f->wbuf_cap) {
                ssize_t w = write(f->fd, buf + nwritten, remain);
                if (w > 0) {
                    nwritten += (size_t) w;
                    remain = sz - nwritten;
                    continue;
                }
                if (w == 0) {
                    return nwritten > 0 ? (ssize_t) nwritten : -1;
                }
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return nwritten > 0 ? (ssize_t) nwritten : -1;
                }
                return nwritten > 0 ? (ssize_t) nwritten : -1;
            }
            // fall through to small path below for  tail < wbuf_cap
        }

        // if request small, refill cache and copy from it
        size_t space = f->wbuf_cap - f->wbuf_len;
        if (space == 0) {
            if (io61_flush(f) < 0) {
                return nwritten > 0 ? (ssize_t) nwritten : -1;
            }
            continue;
        }
        size_t take = std::min(space, remain);
        std::memcpy(f->wbuf + f->wbuf_len, buf + nwritten, take);
        f->wbuf_len += take;
        nwritten += take;
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
// PHASE 2
// PHASE 3
int io61_flush(io61_file* f) {
    // phase 2 read cache change
    if ((f->mode & O_ACCMODE) == O_RDONLY) {
        // drop any read cache
        f->rbuf_len = f->rbuf_pos = 0;
        return 0;
    }

    // phase 3 write cache change
    // write-only -- push staged bytes
    if ((f->mode & O_ACCMODE) == O_WRONLY) {
        while (f->wbuf_pos < f->wbuf_len) {
            ssize_t w = write(f->fd, f->wbuf + f->wbuf_pos, f->wbuf_len - f->wbuf_pos);
            if (w > 0) {
                f->wbuf_pos += (size_t) w;
                continue;
            }
            if (w == 0) {
                // is a stalled write-- treat as error  (keep data) 
                return -1;
            }
            // w < 0
            if (errno == EINTR) continue;               // retry
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // eestartable error (keep data)
                return -1;
            }
            // error!!! (keep data) 
            return -1;
        }
        f->wbuf_pos = f->wbuf_len = 0;
        return 0;
    }

    return 0;
}


// io61_seek(f, off)
//    Changes the file pointer for file `f` to `off` bytes into the file.
//    Returns 0 on success and -1 on failure.

//PHASE 2
//PHASE 3
int io61_seek(io61_file* f, off_t off) {

    // Phase 3 change! (writing)
    // for writing, make sure pending bytes go to the current position
    if ((f->mode & O_ACCMODE) == O_WRONLY) {
        if (io61_flush(f) < 0) {
            return -1;
        }
    }

    off_t r = lseek(f->fd, (off_t) off, SEEK_SET);
    // Ignore the returned offset unless it’s an error.
    if (r == -1) {
        return -1;
    }

    // Phase 2 change! (reading)
    // and Phase 3 change!
    // seeking invalidates the read cache
    if ((f->mode & O_ACCMODE) == O_RDONLY) {
        f->rbuf_len = f->rbuf_pos = 0;
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
