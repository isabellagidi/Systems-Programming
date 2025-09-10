// has string functions
#include <cstring>
// has assert
#include <cassert>
// has I/O like printf
#include <cstdio>


// this declares a function
// input to the function is two C strings
// a C string is a sequence of bytes ending with '/0' which is (NUL) terminator
// the output is a pointer to the first occurrence of the bytes of s2 within s1 or nullptr if NOT found
// must return a pointer into s1, not a new allocation or a copy
// s1 is the haystack and s2 is the needle
// const char* inputs but char* return bc returned pointer designates a position within the haystack
char* mystrstr(const char* s1, const char* s2) {
    //my code

    // loop over 's1' (C strings end at the character with value 0)
    // scans every start position in s1
    // i is an index into s1
    // loop stops when s1[i] hits the NUL terminator (0)
    for (size_t i = 0; s1[i] !=0; ++i){
        // loop over 's2' until 's2' ends or differs from 's1'
        // checks if needle matches starting at i
        // j walks through the needle s2
        // keep advancing the while loop while s2[j] != 0 --> the needle hasn't ended
        //AND keep advancing while the characters at s2[j] and s1[i+j] are equal
        size_t j = 0;
        while (s2[j] != 0 && s2[j] == s1[i+j]){
            ++j;
        }

        // success if we got to the end of 's2'
        // if we reached the end of the needle, we found a full match
        // !s2[j] means "the needle ended here"
        // returns a pointer to the start of the match inside s1 (ie &s1[i])
        // the cast (char* drops const to match the function's return type)
        // the cast (char*) drops const to match the function's return type -- must still treat the pointed to memory as read-only
        // !s2[j] is equivalent to s2[j]==0
        if (!s2[j]){
            return (char*) &s1[i];
        }
    }

    //special case
    // !s2[0] means "s2 is the empty string!"
    // same as s2[0] == 0
    // if we scanned all of s1 with no match... then must handle the empty-needle case
    // is s2 was was empty and s1 was empty too, we would not have entered the for loop
    if (!s2[0]) {
        return (char*) s1;
    }
    return nullptr;
}


// this is another way to hand-code it using a pointer into s1 where s2 first appears
// uses pointers NOT indices to walk the strings
// inputs are const char* bc dont want to modify the pointers
char* mystrstr(const char* s1, const char* s2) {
    // loop over s1
    // *s1 dereferences the pointer, yielding the current character
    // in C++ a nonzero value is "true" and a zero value is "false"
    // SO this loop continues while the current character is not '\0' (the NUL terminator)
    // This is equivalent to ' while (*s1 != '\0')
    while (*s1){
        //loop over s2 until s2 ends of differs from s1
        // here you create two walkers
        // s1try starts at the current candidate position in s1
        //s2try starts at the beginning of s2
        //both are const char* bc we don't mutate the strings
        const char* s1try = s1;
        const char* s2try = s2;

        // *s2try is same as (*s2try != '\O') -- this is only true if and only if the needle hasn't ended yet
        // the && is a logical and
        // *s2try == *s1try is basically saying that the characters are the same

        while (*s2try && *s2try == *s1try) {

            // this part advances both pointers one character forward
            // walking forward as long as the characters match and the needle hasn't ended
            ++s2try;
            ++s1try;
        }

        // success if we got to the end of 's2'
        // !*s2try is true iff *s2try == '\0' bc the ! is the logical NOT
        // bc this means we reached the end of s2 and if we reached teh end of s2 then we know that every char of s2 matched starting at s1
        if (!*s2try) {

            // we return the start of the match which is the current candidate s1
            // (char*) s1 casts away const to match the function's return type
            return (char*) s1;
        } 

        // if we reach here, this means that there was NO match at that starting position, so we advance the starting pointer in s1 by one and try again!! (another iteration)
        ++s1;
    }

    //special case
    // if s2 is empty then !*s2 is true!! bc *s2 == 0
    if (!*s2) {
        return (char*) s1;
    }

    // if we finish the outer loop without returning, we know the needle wasn't found and we can return nullptr

    return nullptr;
}

    
// code always starts with int main(){}
// but this time the main has arguments
// argc/argc hold command-line arguments
// argv[0] is program name, argv[1] is first argument, argv[2] is second argument
int main(int argc, char* argv []){

    // assert(argc==3) enforces exactly two user-supplied strings
    // args are as follows: ./your_program "abracadabra" "cada"
    assert(argc==3);

    // %s prints C strings up to the first '\0'
    // %p prints a pointer address
    printf("strstr(\"%s\", \"%s\") = %p\n",
            argv[1], argv[2], strstr(argv[1],argv[2]));
    printf("mystrstr(\"%s\", \"%s\") = %p\n",
            argv[1], argv[2], mystrstr(argv[1], argv[2]));
    // my function must return the exact same pointer value as the library's strstr
    // if the needle is found, both pointers much point to SAM address inside argv[1]
    // if not found, both must be nullptr
    // if function returnd a pointer to a copy of the substring, then the pointers will differ, and will fail
    assert(strstr(argv[1], argv[2]) == mystrstr(argv[1], argv[2]));
}

//strstr searches s1 for the first occurrence of the entire sequence
// returns a pointer to the start of that match in s1, or else returns nullpts
// empty needle rule: if s2 is the empty string (""), strstr(s1,"") returns s1


// char = a one-byte character
// char* = "pointer to char" --> a variable that stores a memory address where a char lives
// C-strings: have a char* pointing to the first byte of a sequence of bytes that ends with a zero byte '\0' (the NUL terminator)
// *p --> dereference pointer p (reads the char it points to)
// &x --> address-of variable x (making a pointer to x)
// p[i] == *(p+i) --> indexing is pointer arithmetic

// ex:
// const char* s = "cat"; // s points at 'c'
// *s --> 'c'
// s[1] --> 'a'
// *(s+2) --> 't'

// walkers are a nickname for pointers or indices that you advance while scanning data

// const is a type of qualifier that says "you can't modify through this name/type"
// const char* p; "pointer to const char" -- can move the pointer (p++) but cannot write the character
// char* const p = ... "const pointer to char" -- you may write tot he char (*p = 'X') but you may not reassign the pointer: p=q
// const char* const p = ... -- both the pointed to char and the pointer value are non-mutable via p