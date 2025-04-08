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

int ffadd(int (*f)(int, int), int a, int b) {
    return f(a, b);
}

int main(int argc, char** argv) {
    int (*fadd)(int, int) = 0;
    if (argc == 0)
        fadd = add1;
    else if (argc == 1)
        fadd = add2;
    else
        fadd = add3;
    int c = ffadd(fadd, 1, 2);
    return 0;
}