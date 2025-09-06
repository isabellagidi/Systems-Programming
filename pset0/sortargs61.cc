#include <cstring>
// string functions
// fprintf
#include <cstdio>

// argc is the argument count -- how many command-line tokens there are (ex: ./sortargs61 e d c has argc = 4)
// argv is the argument vectr -- an array of pointers to char
// argv[0] is the program name
// argv[1] ... are the actual arguments
// each argv[i] is a C string of bytes ending with '\0'
// this is selection sort: pick the smallest, then from the remaining pick the next smallest
// print immediately instead of performing a full in-place sort
// O(n^2) total time complexity bc each selection scans O(k) times -- do it for n total

// selection sort style: find the smallest string, print it, remove it from the pool by swapping it w the last element and shrinking the pool size
int main(int argc, char* argv[]){
    // keep looping as long as there is at least one real argument (when argc ==1), only argv[0] the program name is left over
    while (argc >1) {
        //find the smallest argument
        // assume the first real argument is current the smallest -- argv[1]
        int smallest = 1;

        //scan the rest of the current argument range argv[2]... argv[argc-1]
        // strcmp compares two C strings lexicographically!! (<0 if a<b, ==0 if equal, >0 if a >b)
        // so if argv[i] is strictly smaller than the current smalles, update smallest = i
        for (int i =2; i < argc; ++i) {
            if (strcmp(argv[i], argv[smallest]) < 0) {
                smallest = i;
            }
        }

        // print the new chosen smallest argument followed by newline
        //%s expects a char* pointing to a NUL-terminated string
        // stoud is standard output (the terminal!)
        fprintf(stdout, "%s\n", argv[smallest]);

        // remove it from the argument set
        // remove the printed argumnet from the active set by overwriting its slot with the last active element then shrinking the active range by 1
        // "remove by swap with last"
        // before, the candidates are argv[1] ... argv[argc-1]
        // now they become argv[1] ... argv[argc-2]
        // don't care about maintaining internal order of remaining itemr s bc we will find the next minimum again on the next pass
        // not modifying the strings themselves, jsut the array of pointers (never write into the pointed-to memory)
        argv[smallest] = argv[argc -1];
        --argc;
    }
}

#include <cstring
#include <cstdio>

// sorts the actual arguments in lexicographic (dictionary) order using insertion sort
// prints them one per line to stdout
// argc = number of tokens on command line
// argv = arrays of char* (C-string pointers)
// argv[0] is the program name, and the rest are the arguments to sort live
// will rearrange the pointers in argv not the strings themselves -- is efficient to move addresses, not by bytes of text
int main(int argc, char* argv[]){
    //insertion sort
    // in insertion sort, we treat the first element of the to-be-sorted region as already sorted
    // here the sorted prefix initially is just argv[1]
    // start inserting from argv[2] then argv[3] up to argv[argc-1]

    for (int i = 2; i <argc; ++i) {
        // save the pointer to the stting we're inserting (x = the key in insertion-sort terms)
        char* x = argv[i];

        // here we are walking left through the shorted prefix shifting elements right until we find the correct place for x (variable named above)
        int j = i - 1;
        // while strcmp(argv[j], x)>0 -- saying that while the element at argv[j] is greater than x, shift it right -- ie make argv[j+1] == argv[j] and then continue left ie do --j
        while (j>= 0 && strcmp(argv[j], x)>0){
            argv[j+1] =. argv[j];
            --j;
        }

        // after the loop above, we have found the slot where x belongs -- in position j+1
        // so we place x there -- argv[j+1] = x
        argv[j+1] =x;
    }

    
    // now we print -- print only the real arguments (1 ... argc-1) -- one per line
    // %s prints out a NUL-terminated C string
    //stdout is the terminal
    for (int i = 1; i <argc; ++i) {
        fprintf(stoud, "%s\n", argv[i]);
    }
}

#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

int main(int argc, char** argv) {
    // create a vector of the arguments as C++ strings
    std::vector<std::string> args;
    for (int i = 1; i<argc; ++i){
        args.push_back(argv[i]);
    }

    //sort the vector's contents
    std::sort(args.begin(), args.end());

    // print the vector's contents
    for (size_t i = 0; i != args.size(); ++i){
        std::cout << args[i] << '\n';
    }
}


//now more advanced: using C++ library features like std::string

#include <string> //std::string
#include <vector> // std::vector
#include <algorithm> // std::sort
#include <iostream> // std::cout, std::endl / '\n'


// argc is number of command-line tokens
// argv is pointer to an array of C-string pointers
// argv[0] is the program name, argv[1]...argv[argc-1] are the arguments
// char** argv is the same as char* argv[]
int main(int argc, char** argv) {
    // create a vector of the arguments as C++ strings
    // args starts as empty
    // the loop skips argv[0] and pushes each argument into the vector
    // push_back(argv[i]) constructs an std::string from the C string char* -- copies bytes up to the first '\0' -- memory management is handled by std::string
    std::vector<std::string> args;
    for (int i = 1; i < argc ; ++i) {
        args.push_back(argv[i]);
    }

    // sort the vector's contents
    // args.begin() and args.end() are random-access iterators over the vector
    //std::sort uses operator< on std::string, which performs lexicographic byte-wise comparison
    std::sort(args.begin(), args.end());

    // print the vector's contents
    //iterates through the sorted vector by index and prints each string plus a newline
    // '\n' is a newline character -- it does not flush the stream
    //std::endl prints a newline AND flushes, which is slower
    // '\n' is preferred unless you need flushing
    // using i != args.size() works because i increases by 1 and args.size() is fixed
    for (size_t i = 0; i != args.size(); ++i) {
        std::cout << args[i] << '\n';
    }
}

#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

int main(int argc, char** argv) {
    //range initialization of a vector
    //uses the range constructor vector<T>(InputIt first, InputIt last)
    //&argv[1] is a char** pointing to the first real argument
    //&argv[argc] points one past the last argument (legal sentinel)
    //constructor iterates from first up to but not including last and copies each element into the vector
    // here char* is converted into an owning std::string
    std::vector<std::string> args(&argv[1], &argv[argc]);

    //std::sort from <algorithm> sorts the range [begin, end) in place
    //for std::string the default compareison is lexicographic
    std::sort(args.begin(), args.end());

    //range-based for loop -- iterates directly over the elements of args
    // auto& s binds each element by reference
    for (auto& s : args) { //range 'for'
        //prints each string following a newline
        std::cout << s<< '\n'    
    }
}