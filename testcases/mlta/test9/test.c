//
// Created by prophe cheng on 2024/6/13.
//
typedef int (*func_t)(int, int);

int func1(int a, int b) {
    return a + b;
}

int func2(int a, int b) {
    return a - b;
}

int func3(int a, int b) {
    return a * b;
}

typedef struct S {
    func_t f;
    int num;
    int id;
} Stu;

Stu s1 = {func1, 0, 1};

int main() {
    Stu s_local_1 = {func1, 0, 1};
    Stu s_local_2 = {func2, 0, 2};
    int a = 2;
    Stu s_local4 = {func3, 0, a};
    Stu s_local_3;
    s_local_3.num = (int)(&func3);
    return 0;
}