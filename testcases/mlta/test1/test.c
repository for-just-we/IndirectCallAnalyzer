//
// This testcase if from paper Where Does It Go? Refining Indirect-Call Targets with Multi-Layer Type Analysis
//
#include <stdio.h>
#include <string.h>

#define MAX_LEN 10

typedef void (*fptr_t)(char *, char *);
struct A { fptr_t handler; };
struct B { struct A a; }; // B is an outer layer of A
struct C { struct A a; }; // C is an outer layer of A

void copy_with_check(char *dst, char *src) {
    if (strlen(src) < MAX_LEN)
        strcpy(dst, src);
}

void copy_no_check(char *dst, char *src) {
    strcpy(dst, src);
}

// Store functions with initializers
struct B b = { .a = { .handler = &copy_with_check } };

// Store function with store instruction
struct C c;

void handle_input(char *user_input) {
    char buf[MAX_LEN];

    (*b.a.handler)(buf, user_input); // safe
    (*c.a.handler)(buf, user_input); // buffer overflow !!
}

int main(int argc, char **argv) {
    char* user_input = argv[1];
    c.a.handler = &copy_no_check;
    handle_input(user_input);
    return 0;
}