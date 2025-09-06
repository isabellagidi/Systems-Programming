#include <cstdlib>
#include <cstdio>

//#include <cstdlib> provides access to the C standard library
//It exits without printing anything. 
// The 0 argument to exit means that the program exits “normally”; 
// exit statuses other than 0 are typically used for errors

int main() {

    fprintf(stdout, "hello, kitty\n");

    exit(0);
}

