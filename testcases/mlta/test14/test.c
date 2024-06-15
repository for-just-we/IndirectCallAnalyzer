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

struct A ba = { .handler = &copy_with_check };

// Store functions with initializers，需要被特别关注
struct B b = { .a = &ba };

void handle_input(char *user_input) {
    char buf[MAX_LEN];

    (*b.a->handler)(buf, user_input); // safe
    // struct A aa = *b.a;
    b.a->handler(buf, user_input);
}

int main(int argc, char** argv) {
    char* user_input = argv[1];
    handle_input(user_input);
    return 0;
}