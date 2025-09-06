#include <cstdlib>
#include <cstdio>
#include <cctype>

//#include <cstdlib> provides access to the C standard library

//every C++ code starts like this
int main() {

    //starting a counter for number of bytes, number of words, and number of lines
    // start counters with unsigned long 
    unsigned long nc = 0, nw = 0, nl = 0;

    // this is a boolean asking if we are starting a new word or in the middle of a word
    // if true, then the previous character was a whitespace and we are at the start of a word
    // if false, then we are in the middle of a word
    // starting at true bc we will be about to start first word!!
    bool in_spaces = true;

    // this is how you do a while loop in C++:
    // while (true) {}
    // we will do a while loop until the end of file (EOF)
    while (true) {

        // reads one byte from the standard input (stdin)
        // this returns the byte as an int in between 0 and 255, or a negative number which is EOF
        // fgetc reads one byte from a stream and returns an int
        int ch = fgetc(stdin);

        // break the loop when reach the end of file
        if (ch == EOF) {
            break;
        }

        // you have now counted every single character in the file (bc u add 1 for each int read)
        ++ nc;
        
        // isspace tells you if character is a whitespace
        // int isspace(int c) expects either c == EOF or c is an unsigned character
        // cast as unsigned char bc a neg number other than EOF causes problems so better than int


        bool this_space = isspace((unsigned char) ch);

        // && is a logical AND, so both must be true
        // ! is a logical NOT, so it negates a boolean
        if (in_spaces && !this_space) {
            ++nw;
        }

        in_spaces = this_space;

        // add 1 to line count whenever you see a new line
        // but note that a final line without '\n' won't increase final line count
        if (ch == '\n') {
            ++nl;
        }
    }

    // print the 3 outputs into a standard output (stdout) in a fixed, right aligned format
    // %8lu means print a unsigned long in a field at least 8 characters wide
    // %7lu means print an unsigned long in a field 7 characters wide
    // % is the start of a format specifier
    // number is the minimum field width
    // l, u is type of argument
    fprintf(stdout, "%8lu %7lu %7lu\n", nl, nw, nc);
}
