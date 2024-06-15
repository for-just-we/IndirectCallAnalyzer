//
// Created by prophe cheng on 2024/6/14.
//

#define MAX_LEN 10

typedef void (*fptr_t)(char *, char *);
struct A { fptr_t handler; };
struct B { struct A a; }; // B is an outer layer of A
struct C { struct A a; }; // C is an outer layer of A

int strlen(char* str) {
    int len = 0;
    char* p = str;
    while(*p != '\0') {
        p++;
        len++;
    }
    return len;
}

void copy_with_check(char *dst, char *src) {
    if (strlen(src) < MAX_LEN) {

    }
}

void copy_no_check(char *dst, char *src) {

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