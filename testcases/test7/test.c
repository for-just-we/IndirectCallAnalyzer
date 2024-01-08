//
// Created by prophe cheng on 2024/1/8.
//
typedef void (*fptr_long)(long);
typedef void (*fptr_ptr)(fptr_long);

void f3(long a) {}

fptr_long callback;

void set_callback(fptr_long f) { callback = f; }

void scene3_b () { callback(0); } // call

void scene3_a () {
    fptr_ptr some_cb_target = set_callback ;
    some_cb_target(f3); // call3
}

void scene3_aa () {
    set_callback(f3); // call3
}

int main(){
    scene3_a();
    scene3_aa();
    return 0;
}