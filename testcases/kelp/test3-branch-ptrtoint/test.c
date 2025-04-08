//
// Created by prophe cheng on 2025/4/7.
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

typedef int(*ladd_t)(int, int);

int ffadd(long f, int a, int b) {
    return ((ladd_t)f)(a, b);
}


int main(int argc, char** argv) {
    long fadd = 0;
    if (argc == 0)
        fadd = (long)add1;
    else if (argc == 1)
        fadd = (long)add2;
    else
        fadd = (long)add3;
    int c = ffadd(fadd, 1, 2);
    return 0;
}