//
// Created by prophe cheng on 2025/4/4.
//

int add(int a, int b) {
    return a + b;
}


int fadd(int(*f)(int, int), int a, int b) {
    return f(a, b);
}

int ffadd(int(*f)(int, int), int a, int b) {
    return fadd(f, a, b);
}

int main() {
    int a = 1;
    int b = 2;
    int c = ffadd(add, 1, 2);
}