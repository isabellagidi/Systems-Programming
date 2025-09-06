#include <cstdlib>
#include <cstdio>

//#include <cstdlib> provides access to the C standard library


int main() {

    unsigned long n = 0; 
    // Unsigned numbers cannot be negative, and are often used to count things

    while (fgetc(stdin) != EOF) {
        ++n;
    }

    fprintf(stdout, "%8lu\n", n);

    exit(0);
}