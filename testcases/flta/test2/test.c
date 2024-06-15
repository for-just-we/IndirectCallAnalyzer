//
// Created by prophe cheng on 2024/6/14.
//
typedef void (*error_t)(char* , const char *, ...);

// 可变参数比对
static void
isclog_error_callback(char *callbacks, const char *fmt, ...) {

}

static void*
isclog_error_callback1(char *callbacks, const char *fmt, ...) {

}

int main() {
    error_t callback1 = isclog_error_callback;
    error_t callback2 = isclog_error_callback1;
    callback1("a", "b", "c", "d");
    callback1("a", "b");
}