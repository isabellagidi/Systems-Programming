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


char* mystrstr(const char* s1, const char* s2) {
    // loop over s1
    while (*s1){
        //loop over s2 until s2 ends of differs from s1
        const char* s1try = s1;
        const char* s2try = s2;
        while (*s2try && *s2try == *s1try) {
            ++s2try;
            ++s1try;
        }

        // success if we got to the end of 's2'
        if (!*s2try) {
            return (char*) s1;
        } 
        ++s1;
    }

    //special case
    if (!*s2) {
        return (char*) s1;
    }

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