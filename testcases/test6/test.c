//
// Created by prophe cheng on 2024/1/8.
//
#include <stdio.h>
#include <string.h>

#define MAX_LEN 10

typedef void (*fptr_t)(char *, char *);
struct A { void* handler; };
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
struct B b = { .a = { .handler = (void*)copy_with_check } };

// Store function with store instruction
struct C c;

void handle_input(char *user_input) {
    char buf[MAX_LEN];

    ((fptr_t)(b.a.handler))(buf, user_input); // safe
    ((fptr_t)(c.a.handler))(buf, user_input); // buffer overflow !!
}

int main(int argc, char **argv) {
    char* user_input = argv[1];
    c.a.handler = (void*)copy_no_check;
    handle_input(user_input);
    return 0;
}