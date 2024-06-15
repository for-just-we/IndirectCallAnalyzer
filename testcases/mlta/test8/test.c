//
// Created by prophe cheng on 2024/1/8.
//
typedef struct A {
    int a;
    int b;
} A;

typedef struct B {
    int a;
    A sa;
} B;

typedef struct C {
    A sa;
    int a;
} C;

int main() {
    int a = 1;
    A sa;
    sa.a = a;
    sa.b = 1;
    B b;
    b.sa = sa;
    b.a = 1;
    C c;
    // 这个指令llvm默认编译会用llvm.memcpy.p0i8.p0i8.i64进行，
    // 首先b.sa和c.sa会用bitcast指令cast到i8类型。
    c.sa = b.sa;
    c.a = b.a;
    return 0;
}