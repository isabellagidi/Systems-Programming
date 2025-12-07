#include "kernel.hh"
#include "k-apic.hh"
#include "k-vmiter.hh"
#include "obj/k-firstprocess.h"
#include <atomic>

// kernel.cc
//
//    This is the kernel.


// INITIAL PHYSICAL MEMORY LAYOUT
//
//  +-------------- Base Memory --------------+
//  v                                         v
// +-----+--------------------+----------------+--------------------+---------/
// |     | Kernel      Kernel |       :    I/O | App 1        App 1 | App 2
// |     | Code + Data  Stack |  ...  : Memory | Code + Data  Stack | Code ...
// +-----+--------------------+----------------+--------------------+---------/
// 0  0x40000              0x80000 0xA0000 0x100000             0x140000
//                                             ^
//                                             | \___ PROC_SIZE ___/
//                                      PROC_START_ADDR

#define PROC_SIZE 0x40000       // initial state only

proc ptable[MAXNPROC];          // array of process descriptors
                                // Note that `ptable[0]` is never used.
proc* current;                  // pointer to currently executing proc

#define HZ 100                  // timer interrupt frequency (interrupts/sec)
[[maybe_unused]] static std::atomic<unsigned long> ticks; // # timer interrupts so far


// Memory state - see `kernel.hh`
physpageinfo physpages[NPAGES];


[[noreturn]] void schedule();
[[noreturn]] void run(proc* p);
void exception(regstate* regs);
uintptr_t syscall(regstate* regs);
void memshow();


// kernel_start(command)
//    Initialize the hardware and processes and start running. The `command`
//    string is an optional string passed from the boot loader.

static void process_setup(pid_t pid, const char* program_name);

void kernel_start(const char* command) {
    // initialize hardware
    init_hardware();
    log_printf("Starting WeensyOS\n");

    ticks = 1;
    init_timer(HZ);

    // clear screen
    console_clear();

    // (re-)initialize kernel page table
    // PHASE 1 START
    for (uintptr_t addr = 0; addr < MEMSIZE_PHYSICAL; addr += PAGESIZE) {
        int perm;
        if (addr == 0) {
            // nullptr is inaccessible even to the kernel
            perm = 0;
        }
        //if console address, user access (exception)
        else if (addr == CONSOLE_ADDR) {
            perm = PTE_P | PTE_W | PTE_U;
        }
        //if in the kernel space, then should NOT be user accessible
        else if (addr < PROC_START_ADDR) {
            perm = PTE_P | PTE_W;   // NOTE: no PTE_U
        }
        //if in the process region, then still user accessible
        else {
            perm = PTE_P | PTE_W | PTE_U;
        }

        // install identity mapping
        int r = vmiter(kernel_pagetable, addr).try_map(addr, perm);
        assert(r == 0); // mappings during kernel_start MUST NOT fail
                        // (Note that later mappings might fail!!)
    }
    // PHASE 1 END

    // set up process descriptors
    for (pid_t i = 0; i < MAXNPROC; i++) {
        ptable[i].pid = i;
        ptable[i].state = P_FREE;
    }
    if (!command) {
        command = WEENSYOS_FIRST_PROCESS;
    }
    if (!program_image(command).empty()) {
        process_setup(1, command);
    } else {
        process_setup(1, "allocator");
        process_setup(2, "allocator2");
        process_setup(3, "allocator3");
        process_setup(4, "allocator4");
    }


    // switch to first process using run()
    run(&ptable[1]);
}


// kalloc(sz)
//    Kernel physical memory allocator. Allocates at least `sz` contiguous bytes
//    and returns a pointer to the allocated memory, or `nullptr` on failure.
//    The returned pointer’s address is a valid physical address, but since the
//    WeensyOS kernel uses an identity mapping for virtual memory, it is also a
//    valid virtual address that the kernel can access or modify.
//
//    The allocator selects from physical pages that can be allocated for
//    process use (so not reserved pages or kernel data), and from physical
//    pages that are currently unused (`physpages[N].refcount == 0`).
//
//    On WeensyOS, `kalloc` is a page-based allocator: if `sz > PAGESIZE`
//    the allocation fails; if `sz < PAGESIZE` it allocates a whole page
//    anyway.
//
//    The returned memory is initially filled with 0xCC, which corresponds to
//    the `int3` instruction. Executing that instruction will cause a `PANIC:
//    Unhandled exception 3!` This may help you debug.

//PHASE 4
void* kalloc(size_t sz) {
    if (sz > PAGESIZE) {
        return nullptr;
    }

    int pageno = 0;
    //PHASE 4
    int page_increment = 3;
    // In the handout code, `kalloc` returns the first free page.
    // Alternate search strategies can be faster and/or expose bugs elsewhere.
    // This initialization returns a random free page:
    //     int pageno = rand(0, NPAGES - 1);
    // This initialization remembers the most-recently-allocated page and
    // starts the search from there:
    //     static int pageno = 0;
    // In Phase 4, you must change the allocation to use non-sequential pages.
    // The easiest way to do this is to set page_increment to 3, but you can
    // also set `pageno` randomly.

    for (int tries = 0; tries != NPAGES; ++tries) {
        uintptr_t pa = pageno * PAGESIZE;
        if (allocatable_physical_address(pa)
            && physpages[pageno].refcount == 0) {
            ++physpages[pageno].refcount;
            void* ptr = reinterpret_cast<void*>(pa);
            memset(ptr, 0xCC, PAGESIZE);
            return ptr;
        }
        pageno = (pageno + page_increment) % NPAGES;
    }

    return nullptr;
}


// kfree(kptr)
//    Free `kptr`, which must have been previously returned by `kalloc`.
//    If `kptr == nullptr` does nothing.
// PHASE 7
void kfree(void* kptr) {
    if (!kptr) {
        return;
    }
    uintptr_t pa = reinterpret_cast<uintptr_t>(kptr);

    // check page-aligned 
    assert((pa & PAGEOFFMASK) == 0);
    // check from allocatable pool
    assert(allocatable_physical_address(pa));

    size_t pageno = pa / PAGESIZE;
    assert(physpages[pageno].refcount > 0);   // must have been allocated
    --physpages[pageno].refcount;
}


// process_setup(pid, program_name)
//    Load application program `program_name` as process number `pid`.
//    This loads the application's code and data into memory, sets its
//    %rip and %rsp, gives it a stack page, and marks it as runnable.

//PHASE 2
//PHASE 3
//PHASE 5
//PHASE 8
void process_setup(pid_t pid, const char* program_name) {
    init_process(&ptable[pid], 0);

    // initialize process page table
    // PHASE 2 BULLET 1: alloc a new page table for the process with kalloc
    x86_64_pagetable* pt = kalloc_pagetable();
    assert(pt != nullptr);
    ptable[pid].pagetable = pt;

    // PHASE 2 BULLET 2: copy the mapings from kernel_pagetable into this new page table
    //using vmiter::try_map
    // match kernel_pagetable until PROC_START_ADDR
    // used a loop with two vmiters

    //PHASE 8 change
    for (uintptr_t a = 0; a < PROC_START_ADDR; a += PAGESIZE) {
        int perm;
        if (a == 0) {
            perm = 0; // null unmapped
        } else if (a == CONSOLE_ADDR) {
            perm = PTE_P | PTE_W | PTE_U; // console is user-visible
        } else {
            perm = PTE_P | PTE_W;  // kernel-only everywhere else
        }
        int r = vmiter(pt, a).try_map(a, perm); // identity: VA == PA
        assert(r == 0);
    }
    // get reference to program image
    program_image pgm(program_name);

    // allocate and map process memory as specified in program image
    // PHASE 3
    for (auto seg = pgm.begin(); seg != pgm.end(); ++seg) {
        //PHASE 2 NEW: keep process-owned pages!!
        // PHASE 8 CHANGE (make text and code pages truly read-only)
        const int uperm = PTE_P | PTE_U | (seg.writable() ? PTE_W : 0);
        
        // changing a to va
        for (uintptr_t va = round_down(seg.va(), PAGESIZE);
             va < seg.va() + seg.size();
             va += PAGESIZE) {
            // *** changed: `va` is the process virtual address for the next code/data page
            // (The handout code requires that the corresponding physical
            // address is currently free.)
            
            //PHASE 3: allocating any free physical page
            void* kptr = kalloc(PAGESIZE);
            assert(kptr != nullptr);
            uintptr_t pa = reinterpret_cast<uintptr_t>(kptr);

            //PHASE 2 NEW: must map VA to PA in the process's page table
            int r = vmiter(pt,va).try_map(pa, uperm);
            assert(r==0);
            
            // zero the whole page
            memset(reinterpret_cast<void*>(pa), 0, PAGESIZE);

            // Copy the portion of segment data that belongs on this page.
            // Compute where this page intersects the segment's "data" prefix.
            // seg.data() has seg.data_size() bytes starting at seg.va().
            uintptr_t seg_data_begin = seg.va();
            uintptr_t seg_data_end = seg.va() + seg.data_size();

            uintptr_t page_data_start;
            if (va < seg_data_begin) page_data_start = seg_data_begin;
            else page_data_start = va;

            uintptr_t page_data_end;
            if ((va + PAGESIZE) < seg_data_end) page_data_end = va + PAGESIZE;
            else  page_data_end = seg_data_end;

            if (page_data_start < page_data_end) {
                size_t nbytes = page_data_end - page_data_start;
                size_t dst_off = page_data_start - va;         // offset within page
                size_t src_off = page_data_start - seg_data_begin;  // offset within seg.data()

                memcpy(reinterpret_cast<char*>(reinterpret_cast<void*>(pa)) + dst_off, seg.data() + src_off, nbytes);
            }
            

        }
    }

    // copy instructions and data from program image into process memory
    //for (auto seg = pgm.begin(); seg != pgm.end(); ++seg) {
    //    memset(reinterpret_cast<void*>(seg.va()), 0, seg.size());
    //    memcpy(reinterpret_cast<void*>(seg.va()), seg.data(), seg.data_size());
    //}

    // mark entry point
    ptable[pid].regs.reg_rip = pgm.entry();

    // allocate and map stack segment
    // Compute process virtual address for stack page


    //PHASE 5
    uintptr_t stack_addr = MEMSIZE_VIRTUAL - PAGESIZE;

    {
        void* kptr = kalloc(PAGESIZE);
        assert(kptr != nullptr);
        uintptr_t pa = reinterpret_cast<uintptr_t>(kptr);
        //NEW: again must map VA to PA
        int r = vmiter(pt, stack_addr).try_map(pa, PTE_P | PTE_W | PTE_U);
        assert(r == 0);
        // zero out the stack page
        memset(reinterpret_cast<void*>(pa), 0, PAGESIZE);   
    }

    ptable[pid].regs.reg_rsp = stack_addr + PAGESIZE;

    // mark process as runnable
    ptable[pid].state = P_RUNNABLE;
}



// exception(regs)
//    Exception handler (for interrupts, traps, and faults).
//
//    The register values from exception time are stored in `regs`.
//    The processor responds to an exception by saving application state on
//    the kernel's stack, then jumping to kernel assembly code (in
//    k-exception.S). That code saves more registers on the kernel's stack,
//    then calls exception().
//
//    Note that hardware interrupts are disabled when the kernel is running.

void exception(regstate* regs) {
    // Copy the saved registers into the `current` process descriptor.
    current->regs = *regs;
    regs = &current->regs;

    // It can be useful to log events using `log_printf`.
    // Events logged this way are stored in the host's `log.txt` file.
    /* log_printf("proc %d: exception %d at rip %p\n",
                current->pid, regs->reg_intno, regs->reg_rip); */

    // Show the current cursor location and memory state
    // (unless this is a kernel fault).
    console_show_cursor();
    if (regs->reg_intno != INT_PF || (regs->reg_errcode & PTE_U)) {
        memshow();
    }

    // If Control-C was typed, exit the virtual machine.
    check_keyboard();


    // Actually handle the exception.
    switch (regs->reg_intno) {

    case INT_IRQ + IRQ_TIMER:
        ++ticks;
        lapicstate::get().ack();
        schedule();
        break;                  /* will not be reached */

    case INT_PF: {
        // Analyze faulting address and access type.
        uintptr_t addr = rdcr2();
        const char* operation = regs->reg_errcode & PTE_W
                ? "write" : "read";
        const char* problem = regs->reg_errcode & PTE_P
                ? "protection problem" : "missing page";

        if (!(regs->reg_errcode & PTE_U)) {
            proc_panic(current, "Kernel page fault on %p (%s %s, rip=%p)!\n",
                       addr, operation, problem, regs->reg_rip);
        }
        error_printf("PAGE FAULT on %p (pid %d, %s %s, rip=%p)!\n",
                     addr, current->pid, operation, problem, regs->reg_rip);
        log_print_backtrace(current);
        current->state = P_FAULTED;
        break;
    }

    default:
        proc_panic(current, "Unhandled exception %d (rip=%p)!\n",
                   regs->reg_intno, regs->reg_rip);

    }


    // Return to the current process (or run something else).
    if (current->state == P_RUNNABLE) {
        run(current);
    } else {
        schedule();
    }
}


int syscall_page_alloc(uintptr_t addr);
// PHASE 6 (forward declaration)
pid_t syscall_fork();
// PHASE 7 (forward declarations)
[[noreturn]] void sys_exit();
static void free_process_memory(proc* p);

static intptr_t sys_kill(proc* killer, pid_t victim_pid); //EC 
int syscall_page_free(uintptr_t addr); //EC 
[[noreturn]] void syscall_sleep(uintptr_t nticks); //EC  


// syscall(regs)
//    Handle a system call initiated by a `syscall` instruction.
//    The process’s register values at system call time are accessible in
//    `regs`.
//
//    If this function returns with value `V`, then the user process will
//    resume with `V` stored in `%rax` (so the system call effectively
//    returns `V`). Alternately, the kernel can exit this function by
//    calling `schedule()`, perhaps after storing the eventual system call
//    return value in `current->regs.reg_rax`.
//
//    It is only valid to return from this function if
//    `current->state == P_RUNNABLE`.
//
//    Note that hardware interrupts are disabled when the kernel is running.

uintptr_t syscall(regstate* regs) {
    // Copy the saved registers into the `current` process descriptor.
    current->regs = *regs;
    regs = &current->regs;

    // It can be useful to log events using `log_printf`.
    // Events logged this way are stored in the host's `log.txt` file.
    /* log_printf("proc %d: syscall %d at rip %p\n",
                  current->pid, regs->reg_rax, regs->reg_rip); */

    // Show the current cursor location and memory state.
    console_show_cursor();
    memshow();

    // If Control-C was typed, exit the virtual machine.
    check_keyboard();


    // Actually handle the exception.
    switch (regs->reg_rax) {

    case SYSCALL_PANIC:
        user_panic(current);
        break; // will not be reached

    case SYSCALL_GETPID:
        return current->pid;

    case SYSCALL_YIELD:
        current->regs.reg_rax = 0;
        schedule();             // does not return

    case SYSCALL_PAGE_ALLOC:
        return syscall_page_alloc(current->regs.reg_rdi);

    // PHASE 6
    case SYSCALL_FORK:
        return syscall_fork();
    
    // PHASE 7 
    case SYSCALL_EXIT:
        sys_exit(); 
    
    //EC
    case SYSCALL_KILL: 
        return sys_kill(current, static_cast<pid_t>(regs->reg_rdi));
    
    //EC
    case SYSCALL_PAGE_FREE:
        return syscall_page_free(current->regs.reg_rdi);
    
    //EC
    case SYSCALL_SLEEP:
        syscall_sleep(current->regs.reg_rdi);

    default:
        proc_panic(current, "Unhandled system call %ld (pid=%d, rip=%p)!\n",
                   regs->reg_rax, current->pid, regs->reg_rip);

    }

    panic("Should not get here!\n");
}


// syscall_page_alloc(addr)
//    Handles the SYSCALL_PAGE_ALLOC system call. This function
//    should implement the specification for `sys_page_alloc`
//    in `u-lib.hh` (but in the handout code, it does not).

// PHASE 1
// PHASE 3
int syscall_page_alloc(uintptr_t addr) {
    // PHASE 1: make sure sys_page_alloc preserves kernel isolation
    //additional check
    if ((addr & PAGEOFFMASK) != 0 || addr < PROC_START_ADDR || addr >= MEMSIZE_VIRTUAL)
        return -1;

    //PHASE 3 (using kalloc and new pages)
    void* new_kptr = kalloc(PAGESIZE);
    if (!new_kptr) {
        return -1;  // out of memory
    }
    uintptr_t new_pa = reinterpret_cast<uintptr_t>(new_kptr);
    memset(new_kptr, 0, PAGESIZE);

    vmiter it(current->pagetable, addr);
    uintptr_t old_pa;
    if (it.present()) old_pa = it.pa();
    else old_pa = static_cast<uintptr_t>(-1); 
    


    //PHASE 1: make sure sys_page_alloc preserves kernel isolation
    // writes page table entry
    int r = it.try_map(new_pa, PTE_P | PTE_W | PTE_U);
    //PHASE 3
    if (r < 0) {
        // undo allocation
        kfree(new_kptr); // PHASE 7
        return -1;
    }

    // if replace a page in trymap,  drop its reference
    if (old_pa != uintptr_t(-1)) {
        kfree(reinterpret_cast<void*>(old_pa)); // PHASE 7
    }

    return 0;
}

//syscall_fork
//    Handles the SYSCALL_FORK system call. 
//    Phase 6 and 7 and 8 implementation. This function
//    should implement the specification for `sys_fork`
//    in `u-lib.hh` (but in the handout code, it does not).
//    As requested in the pset, the code must complete the following
//      1) look for a free process slot (but don't use slot 0)
//      2) if a free slot is fount, copy paren't page table for addresses below PROC_START_ADDR
//      3) for addresses >= PROC_START_ADDR unique child page tables
//      4) initialize child's registers as copy of parent's (except reg_rax = 0)
//      5) make child process runnable
//      6) return 0 to the child and return the child's process ID to parent  
//      7) if fork fails, clean up (reset child process slot to free state) and return -1
// pHASE 6
//PHASE 7
//PHASE 8
pid_t syscall_fork() {
    // 1) look for a free process slot (but don't use slot 0)
    pid_t child = -1;
    for (pid_t i = 1; i < MAXNPROC; ++i) {
        if (ptable[i].state == P_FREE) { child = i; break; }
    }
    if (child < 0) {
        return -1;
    }

    // 2) if a free slot is fount, copy paren't page table for addresses below PROC_START_ADDR
    init_process(&ptable[child], 0);
    ptable[child].pid = child;

    x86_64_pagetable* cpt = kalloc_pagetable();
    if (!cpt) {
        ptable[child].state = P_FREE;
        return -1;
    }
    ptable[child].pagetable = cpt;

    // PHASE 8 edit
    for (uintptr_t a = 0; a < PROC_START_ADDR; a += PAGESIZE) {
        int perm;
        if (a == 0) {
            perm = 0;  // null unmapped
        } else if (a == CONSOLE_ADDR) {
            perm = PTE_P | PTE_W | PTE_U; // console user-accessible
        } else {
            perm = PTE_P | PTE_W; // kernel-only
        }
        int r = vmiter(cpt, a).try_map(a, perm); // identity mapping
        if (r < 0) goto fail;
    }

    // 3) for addresses >= PROC_START_ADDR unique child page tables
    // duplicate parent's user-accessible pages >= PROC_START_ADDR
    // allocate a fresh physical page, map it in the child at same VA, and copy the parent's bytes
    // copy kernel mappings, which are identical to kernel_pagetable up to PROC_START_ADDR
    // PHASE 8 edit!!
    for (uintptr_t va = PROC_START_ADDR; va < MEMSIZE_VIRTUAL; va += PAGESIZE) {
        vmiter src(current->pagetable, va);
        if (!src.present() || !src.user()) {
            continue; // skip unmapped/kernel-only
        }

        if (src.writable()) {
            // writable so make a private copy
            void* dst_kptr = kalloc(PAGESIZE);
            if (!dst_kptr) goto fail;

            int r = vmiter(cpt, va).try_map(reinterpret_cast<uintptr_t>(dst_kptr), src.perm()); // same perms, includes U
            if (r < 0) goto fail;

            const void* src_kptr = src.kptr<const void*>();
            assert(src_kptr != nullptr);
            memcpy(dst_kptr, src_kptr, PAGESIZE);
        } else {
            // read-only so share the physical page
            uintptr_t pa = src.pa();
            int r = vmiter(cpt, va).try_map(pa, src.perm() & ~PTE_W); // ensure RO
            if (r < 0) goto fail;

            // bump refcount for shared page
            ++physpages[pa / PAGESIZE].refcount;
        }
    }

    // 4) initialize child's registers as copy of parent's (except reg_rax = 0)
    ptable[child].regs = current->regs;
    ptable[child].regs.reg_rax = 0;

    // 5) make child process runnable
    ptable[child].state = P_RUNNABLE;

    // 6) return 0 to the child and return the child's process ID to parent
    return child;

fail:
    // PHASE 8 - free any partial mappings in child
    free_process_memory(&ptable[child]);
    // 7) if fork fails, clean up (reset child process slot to free state) and return -1
    ptable[child].state = P_FREE;
    return -1;
}


//free_process_memory
//    helper function
//    frees all user mappings >= PROC_START_ADDR and all page-table pages for p
// PHASE 7
static void free_process_memory(proc* p) {
    if (!p->pagetable) {
        return;
    }

    // free all user pages mapped by this process 
    for (uintptr_t va = PROC_START_ADDR; va < MEMSIZE_VIRTUAL; va += PAGESIZE) {
        vmiter it(p->pagetable, va);
        if (it.present() && it.user()) {
            uintptr_t pa = it.pa();
            it.map(it.pa(), 0);

            // free the physical page
            kfree(reinterpret_cast<void*>(pa));
        }
    }

    // free the page-table pages (except top level page)
    for (ptiter pit(p->pagetable); pit.va() < MEMSIZE_VIRTUAL; pit.next()) {
        kfree(pit.kptr());
    }

    // free the top-level page table page
    kfree(p->pagetable);
    p->pagetable = nullptr;
}


// sys_exit()
//    to exit the current process--free all of its memory (user pages and page tables),
//    mark it free, and schedule someone else
// PHASE 7
[[noreturn]] void sys_exit() {
    // free everything owned by current
    free_process_memory(current);

    // mark the slot free
    current->state = P_FREE;

    // switch to someone else (does not return)
    schedule();
}


// schedule
//    Pick the next process to run and then run it.
//    If there are no runnable processes, spins forever.

void schedule() {
    pid_t pid = current->pid;
    for (unsigned spins = 1; true; ++spins) {
        // First, wake any sleeping processes whose time has come
        unsigned long now = ticks.load();
        for (int i = 1; i < MAXNPROC; ++i) {
            if (ptable[i].state == P_BLOCKED
                && ptable[i].wakeup_tick <= now) {
                ptable[i].state = P_RUNNABLE;
            }
        }

        // Then pick the next runnable process in round-robin order
        pid = (pid + 1) % MAXNPROC;
        if (ptable[pid].state == P_RUNNABLE) {
            run(&ptable[pid]);
        }

        // If Control-C was typed, exit the virtual machine.
        check_keyboard();

        // If spinning forever, show the memviewer.
        if (spins % (1 << 12) == 0) {
            memshow();
        }
    }
}


// run(p)
//    Run process `p`. This involves setting `current = p` and calling
//    `exception_return` to restore its page table and registers.

void run(proc* p) {
    assert(p->state == P_RUNNABLE);
    current = p;

    // Check the process's current registers.
    check_process_registers(p);

    // Check the process's current pagetable.
    check_pagetable(p->pagetable);

    // This function is defined in k-exception.S. It restores the process's
    // registers then jumps back to user mode.
    exception_return(p);

    // should never get here
    while (true) {
    }
}


// memshow()
//    Draw a picture of memory (physical and virtual) on the CGA console.
//    Switches to a new process's virtual memory map every 0.25 sec.
//    Uses `console_memviewer()`, a function defined in `k-memviewer.cc`.

void memshow() {
    static unsigned last_ticks = 0;
    static int showing = 0;

    // switch to a new process every 0.25 sec
    if (last_ticks == 0 || ticks - last_ticks >= HZ / 2) {
        last_ticks = ticks;
        showing = (showing + 1) % MAXNPROC;
    }

    proc* p = nullptr;
    for (int search = 0; !p && search < MAXNPROC; ++search) {
        if (ptable[showing].state != P_FREE
            && ptable[showing].pagetable) {
            p = &ptable[showing];
        } else {
            showing = (showing + 1) % MAXNPROC;
        }
    }

    console_memviewer(p);
    if (!p) {
        console_printf(CPOS(10, 26), CS_WHITE "   VIRTUAL ADDRESS SPACE\n"
            "                          [All processes have exited]\n"
            "\n\n\n\n\n\n\n\n\n\n\n");
    }
}


//EC
static void destroy_process(proc* p) {
    if (p->state == P_FREE) {
        return;
    }

    // Reuse from Phase 7 
    free_process_memory(p);

    // Mark process as free
    p->state = P_FREE;
    p->pid = 0;      // reset PID
}

//EC
static intptr_t sys_kill(proc* killer, pid_t victim_pid) {
    // invalid PID (so return error)
    if (victim_pid <= 0 || victim_pid >= MAXNPROC) {
        return -1;
    }

    proc* victim = &ptable[victim_pid];

    // No process or already free
    if (victim->state == P_FREE) {
        return -1;
    }

    // To kill the victim: free its memory and mark it free
    destroy_process(victim);
    return 0;
}

//EC
// syscall_page_free(addr)
//    Handles the SYSCALL_PAGE_FREE system call.
//    Frees and unmaps a single user page at `addr` in the current process.
//    Returns 0 on success. On failure (invalid address or no mapped user page),
//    returns -1 and makes no changes.

int syscall_page_free(uintptr_t addr) {
    // Argument validation: must be page-aligned and in user range
    if ((addr & PAGEOFFMASK) != 0 || addr < PROC_START_ADDR || addr >= MEMSIZE_VIRTUAL) {
        return -1;
    }

    vmiter it(current->pagetable, addr);

    // There must be a present, user-accessible mapping here
    if (!it.present() || !it.user()) {
        return -1;
    }

    uintptr_t pa = it.pa();

    // only free allocatable physical addresses
    if (!allocatable_physical_address(pa)) {
        // Treat as error and leave mapping alone
        return -1;
    }

    // Unmap the page from this process
    it.map(pa, 0);

    // drop our reference to the physical page
    kfree(reinterpret_cast<void*>(pa));

    return 0;
}


// syscall_sleep(nticks)
//    Put the current process to sleep for at least `nticks` timer interrupts
//    Never returns directly; instead it marks the process blocked,
//    calls schedule(), and later the process is resumed with return value 0
[[noreturn]] void syscall_sleep(uintptr_t nticks) {
    // if nticks == 0, this is basically just a yield
    current->regs.reg_rax = 0;   // return value when we eventually wake

    if (nticks == 0) {
        // just yield to another process
        schedule();          // does not return
    }

    // compute wakeup time based on current ticks
    unsigned long now = ticks.load();
    current->wakeup_tick = now + nticks;

    // mark process as blocked so scheduler won't run it until wakeup
    current->state = P_BLOCKED;

    // qwitch to some other runnable process
    schedule();              // does not return
}

