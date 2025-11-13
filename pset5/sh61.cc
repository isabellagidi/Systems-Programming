#include "sh61.hh"
#include <cstring>
#include <cerrno>
#include <vector>
#include <sys/stat.h>
#include <sys/wait.h>

// For the love of God
#undef exit
#define exit __DO_NOT_CALL_EXIT__READ_PROBLEM_SET_DESCRIPTION__


// struct command
//    Data structure describing a command. Add your own stuff.

struct command {
    std::vector<std::string> args;
    pid_t pid = -1;      // process ID running this command, -1 if none

    command();
    ~command();

    void run();

    // Phase4!! pipeline I/O
    int in_fd = -1;   // becomes stdin
    int out_fd = -1;  // becomes stdout
};


// command::command()
//    This constructor function initializes a `command` structure. You may
//    add stuff to it as you grow the command structure.

command::command() {
}


// command::~command()
//    This destructor function is called to delete a command.

command::~command() {
}


// COMMAND EXECUTION

// command::run()
//    Creates a single child process running the command in `this`, and
//    sets `this->pid` to the pid of the child process.
//
//    If a child process cannot be created, this function should call
//    `_exit(EXIT_FAILURE)` (that is, `_exit(1)`) to exit the containing
//    shell or subshell. If this function returns to its caller,
//    `this->pid > 0` must always hold.
//
//    Note that this function must return to its caller *only* in the parent
//    process. The code that runs in the child process must `execvp` and/or
//    `_exit`.
//
//    PHASE 1: Fork a child process and run the command using `execvp`.
//       This will require creating a vector of `char*` arguments using
//       `this->args[N].c_str()`. Note that the last element of the vector
//       must be a `nullptr`.
//    PHASE 4: Set up a pipeline if appropriate. This may require creating a
//       new pipe (`pipe` system call), and/or replacing the child process's
//       standard input/output with parts of the pipe (`dup2` and `close`).
//       Draw pictures!
//    PHASE 7: Handle redirections.

void command::run() {
    assert(this->pid == -1);
    assert(!this->args.empty());
    // Your code here!

    //Phase 1
    // Build argv!!
    // argv = {arg0, arg1, ..., nullptr}
    std::vector<char*> argv;
    argv.reserve(this->args.size() + 1);
    for (std::string& s : this->args) {
        argv.push_back(const_cast<char*>(s.c_str()));
    }
    argv.push_back(nullptr);

    pid_t p = fork();
    if (p < 0) {
        // this means fork failed, so exit 
        fprintf(stderr, "sh61: fork: %s\n", strerror(errno));
        _exit(EXIT_FAILURE);
    } else if (p == 0) {
        // Phase 4 add!! set up pipeline redirection for this child process
        if (this->in_fd != -1) {
            if (dup2(this->in_fd, STDIN_FILENO) < 0) {
                fprintf(stderr, "dup2: %s\n", strerror(errno));
                _exit(1);
            }
        }
        if (this->out_fd != -1) {
            if (dup2(this->out_fd, STDOUT_FILENO) < 0) {
                fprintf(stderr, "dup2: %s\n", strerror(errno));
                _exit(1);
            }
        }
        // close original ends in the child after dup2
        if (this->in_fd != -1 && this->in_fd != STDIN_FILENO) {
            close(this->in_fd);
        }
        if (this->out_fd != -1 && this->out_fd != STDOUT_FILENO) {
            close(this->out_fd);
        }
        // Phase 1
        // child: replace with target program
        execvp(argv[0], argv.data());
        // if execvp returns, it failed
        fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
        _exit(1);
    } else {
        // parent: record child PID and return
        this->pid = p;
    }
}

//run_pipeline(condit)
//      PHASE 4
//      Runs one pipeline (joined by |) and returns the last command's wait status
template <typename PipelineParser>
static int run_pipeline(PipelineParser pipeit) {
    //collect all commands in this pipeline
    std::vector<command*> cmds;
    for (auto cmdit = pipeit.command_begin(); cmdit != pipeit.end(); ++cmdit) {
        command* c = new command;

        // Build argv (Phase 2 -- just normal tokens)
        for (auto tok = cmdit.token_begin(); tok != cmdit.token_end(); ++tok) {
            if (tok.type() == TYPE_NORMAL) /* TYPE_NORMAL tokens only */ {
                c->args.push_back(tok.str());
            } else if (tok.type() == TYPE_REDIRECT_OP) {
                // skip filename token that follows redirect operator
                ++tok;
                if (tok == cmdit.token_end()) break;
            } else {
                // ignore other token types inside a simple command
            }
        }

        cmds.push_back(c);
    }

    auto synthesized_status = [](int code) {
        return (code & 0xFF) << 8; // fabricate a wait status with given exit code
    };
    
    // create pipes and wire in_fd/out_fd across the chain
    int prev_read_fd = -1;

    for (size_t i = 0; i < cmds.size(); ++i) {
        int next_pipe[2] = {-1, -1};
        bool has_next = (i + 1 < cmds.size());

        if (has_next) {
            if (pipe(next_pipe) < 0) {
                fprintf(stderr, "pipe: %s\n", strerror(errno));
                for (auto* c : cmds) delete c; // cleanup
                return synthesized_status(1);  // fail status
            }
        }

        // connect stdin/stdout for this stage
        cmds[i]->in_fd = prev_read_fd;             // -1 for first stage
        cmds[i]->out_fd = has_next ? next_pipe[1] : -1;

        // start this stage
        // spawn!
        cmds[i]->run();

        // PARENT: close ends we no longer need immediately 
        if (prev_read_fd != -1) {
            close(prev_read_fd); // close the previous read end bc parent doesn't need
            prev_read_fd = -1;
        }
        if (has_next) {
            close(next_pipe[1]); // parent doesn't write to the new pipe
            prev_read_fd = next_pipe[0]; // keep read end to pass to the next child
        }
    }

    // after starting all children, parent no longer needs the final read end so close!
    if (prev_read_fd != -1) {
        close(prev_read_fd);
    }

    // wait for the last command
    // The status of a pipeline is the status of its LAST command
    int status = synthesized_status(1);
    if (cmds.back()->pid > 0) {
        while (waitpid(cmds.back()->pid, &status, 0) < 0 && errno == EINTR) {
            /* retry */
        }
    }

    // cleanup command objects
    for (auto* c : cmds) delete c;
    return status;
}


//run_conditional(condit)
//      Runs one conditional and returns its final wait status
//      calls  run_pipeline
//      PHASE 3
template <typename ConditionalParser>
static int run_conditional(ConditionalParser condit) {
    auto pipeit = condit.pipeline_begin();
    bool have_prev = false;
    bool prev_success = false;
    int  last_status = 0;
    int  last_op = TYPE_EOL;   // operator between previous and current pipeline

    while (pipeit != condit.end()) {
        // decide whether to run current pipeline based on the previous result and operator
        bool should_run = !have_prev || (last_op == TYPE_AND && prev_success) || (last_op == TYPE_OR  && !prev_success);

        if (should_run) {
            // get the last command's status
            // Phase 4 call run_pipeline 
            int status = run_pipeline(pipeit);
            last_status = status;
            prev_success = (WIFEXITED(status) && WEXITSTATUS(status) == 0);
            have_prev = true;
        }

        //Operator to the NEXT pipeline and advance
        int op = pipeit.next_op();   // TYPE_AND or TYPE_OR or TYPE_EOL
        ++pipeit;
        last_op = op;
    }

    return last_status;
}

// run_line(clp)
//    Run the command line contained in `command_line_parser clp`.
//
//    PHASE 1: Use `waitpid` to wait for the command started by `c->run()`
//        to finish.
//
//    The remaining phases may require that you introduce helper functions
//    (e.g., to process a pipeline), write code in `command::run`, and/or
//    change `struct command`.
//
//    It is possible, and not too ugly, to handle command lines, conditionals,
//    *and* pipelines entirely within `run_line`, but in general it is clearer
//    to introduce `run_conditional` and `run_pipeline` functions that
//    are called by `run_line`. It’s up to you.
//
//    PHASE 2: Introduce a loop so `run_line` can run command lists, which
//       consist of one or more commands separated by TYPE_SEQUENCE ';'. Wait
//       for each command to finish before going on to the next.
//    PHASE 3: Change the loop to handle conditionals.
//    PHASE 4: Change the loop to handle pipelines. Start all processes in
//       the pipeline in parallel. The status of a pipeline is the status of
//       its LAST command.
//    PHASE 5: Change the loop to handle background conditional chains.
//       This may require adding another call to `fork()`!

void run_line(command_line_parser clp) {
    // Phase 2 (Command lists -- iterate each condiition on the command line split by ; and &)
    // Phase 2 loop
    for (auto condit = clp.conditional_begin(); condit != clp.end(); ++condit) {
        //Phase 3: Change the loop to handle conditionals
        // Phase 4: NOTE run_conditional calls run_pipeline
        int _status = run_conditional(condit);
    }
}




int main(int argc, char* argv[]) {
    FILE* command_file = stdin;
    bool quiet = false;

    // Check for `-q` option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = true;
        --argc, ++argv;
    }

    // Check for filename option: read commands from file
    if (argc > 1) {
        command_file = fopen(argv[1], "rb");
        if (!command_file) {
            perror(argv[1]);
            return 1;
        }
    }

    // - Put the shell into the foreground
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    claim_foreground(0);
    set_signal_handler(SIGTTOU, SIG_IGN);

    char buf[BUFSIZ];
    int bufpos = 0;
    bool needprompt = true;

    while (!feof(command_file)) {
        // Print the prompt at the beginning of the line
        if (needprompt && !quiet) {
            printf("sh61[%d]$ ", getpid());
            fflush(stdout);
            needprompt = false;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == nullptr) {
            if (ferror(command_file) && errno == EINTR) {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            } else {
                if (ferror(command_file)) {
                    perror("sh61");
                }
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
            run_line(command_line_parser{buf});
            bufpos = 0;
            needprompt = 1;
        }

        // Handle zombie processes and/or interrupt requests
        // Your code here!
    }

    return 0;
}
