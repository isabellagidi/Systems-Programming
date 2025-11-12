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
        //phase 3 loop: Change the loop to handle conditionals.
        auto pipeit = condit.pipeline_begin();
        bool have_prev = false;
        bool prev_success = false;
        int  last_status = 0;
        int  last_op = TYPE_EOL;

        //Phase 3 while loop
        while (pipeit != condit.end()) {
            //decide whether to run current pipeline pipeit based on the previous result and operator
            bool should_run = !have_prev || (last_op == TYPE_AND && prev_success) || (last_op == TYPE_OR  && !prev_success);

            if (should_run) { //if boolean true
                // Grab first command of this pipeline
                auto cmdit = pipeit.command_begin();

                // Build argv (Phase 2 -- just normal tokens)
                command* c = new command;
                if (cmdit != pipeit.end()) {
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
                }

                //skip conditional if no runnable words
                if (!c->args.empty()) {
                    // spawn the command
                    c->run();
                    // Phase 1 (wait for child to finish) 
                    if (c->pid > 0) {
                        int status;
                        // retry on EINTR 
                        while (waitpid(c->pid, &status, 0) < 0 && errno == EINTR) {
                            /* retry */
                        }
                        last_status = status;
                        prev_success = (WIFEXITED(status) && WEXITSTATUS(status) == 0);
                        have_prev = true;
                    }
                } 
                else{
                    // if c->args.empty()
                    // treat empty command as non-success!!
                    prev_success = false;
                    // mark that we've evaluated something
                    have_prev = true;
                }
                delete c;
            }

        // for the WHILE loop phase 3!! 
        //Operator to the NEXT pipeline and advance
        int op = pipeit.next_op();   // TYPE_AND or TYPE_OR or TYPE_EOL
        ++pipeit;
        last_op = op;

        }
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
