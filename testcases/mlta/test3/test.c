//
// This testcase is from paper: FINE-CFI: Fine-Grained Control-Flow Integrity for Operating System Kernels
//

typedef struct sb {
    int i, j;
    char* (*f)(int);
} sb;

typedef struct sa {
    int i;
    sb b;
} sa;

char* callee_A(int i) {
    return "";
}

char* callee_B(int i) {
    return "";
}

char* callee_C(long i) {
    return "";
}

char* callee_D(long i) {
    return "";
}

sa Ga = {0, {0, 0, callee_A}};
sb *pb = &Ga.b;

int flag;

void caller(sb* BB, char* (*func)(long)) {
    if (flag)
        func = callee_C;
    func(1);
    BB->f(2);
}

int main(int argc, char** argv) {
    flag = argc - 1;
    if (flag)
        pb->f = callee_B;
    caller(pb, callee_D);
    return 0;
}