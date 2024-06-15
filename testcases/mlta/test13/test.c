//
// Created by prophe cheng on 2024/6/15.
//
#define MAX_LEN 10

typedef void (*fptr_t)(char *, char *);
struct A { fptr_t handler; };
struct B { struct A* a; int b; }; // B is an outer layer of A
struct C { struct A* a; int c; }; // C is an outer layer of A

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

struct A a1 = {copy_with_check};
struct A a2 = {copy_no_check};

struct B b;

// Store function with store instruction
struct C c;

void handle_input(char *user_input) {
    char buf[MAX_LEN];

    (*b.a->handler)(buf, user_input); // safe
    (*c.a->handler)(buf, user_input); // buffer overflow !!
}

int main(int argc, char **argv) {
    char* user_input = argv[1];
    c.a = &a2;
    b.a = &a1;
    handle_input(user_input);
    return 0;
}