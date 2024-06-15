//
// Created by prophe cheng on 2024/6/14.
//

// 比对函数返回值不同时的参数签名比对
int match1(void* a, void *b) {
    return *((int*)a) == *((int*)b);
}

void match2(void* a, void *b) {
    return;
}

int match3(int* a, int *b) {
    return *a == *b;
}

typedef int(*match_func_t)(void* a, void *b);

int main() {
    match_func_t f = match1;
    match_func_t f1 = match2;
    match_func_t f2 = match3;
    int a, b;
    f(&a, &b);
    f1(&a, &b);
    return 0;
}