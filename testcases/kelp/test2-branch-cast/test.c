//
// Created by prophe cheng on 2025/4/6.
//
int add1(int a, int b) {
    return a + b;
}

int add2(int a, int b) {
    return a + b;
}

int add3(int a, int b) {
    return a + b;
}

long ffadd(long (*f)(int, int), int a, int b) {
    return f(a, b);
}

typedef long(*ladd_t)(int, int);

int main(int argc, char** argv) {
    long (*fadd)(int, int) = 0;
    if (argc == 0)
        fadd = (ladd_t)add1;
    else if (argc == 1)
        fadd = (ladd_t)add2;
    else
        fadd = (ladd_t)add3;
    long c = ffadd(fadd, 1, 2);
    return 0;
}