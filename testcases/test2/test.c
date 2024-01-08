//
// This testcase is from paper: TyPro: Forward CFI for C-Style Indirect Function Calls Using Type Propagation
//

typedef void (*fptr_long)(long);
typedef void (*fptr_int)(int);
typedef void (*fptr_ptr)(fptr_long);
void f1(long a) {}
void f2(long a) {}
void f3(long a) {}

void scene1_b(fptr_int f) { f(0); } // call1

void scene1_a () {
   fptr_int f = (fptr_int)&f1;
   scene1_b(f);
}

struct S {
    fptr_long one;
    fptr_int two;
};

void scene2_b (struct S* s) { s->one(0); } // call2

void scene2_a () {
    struct S s = {&f2 , 0};
    scene2_b(&s);
}

fptr_long callback;

void set_callback(fptr_long f) { callback = f; }

void scene3_b () { callback(0); } // call

void scene3_a () {
    fptr_ptr some_cb_target = &set_callback ;
    some_cb_target(&f3); // call3
}

int main(){
    scene1_a();
    scene2_a();
    scene3_a();
    return 0;
}