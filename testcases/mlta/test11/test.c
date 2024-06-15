//
// Created by prophe cheng on 2024/6/15.
//

#define TRUE 1
#define bool _Bool
#define NULL 0

enum iostream_pump_status {
    /* pump succeeded - EOF received from istream and all output was
       written successfully to ostream. */
    IOSTREAM_PUMP_STATUS_INPUT_EOF,
    /* pump failed - istream returned an error */
    IOSTREAM_PUMP_STATUS_INPUT_ERROR,
    /* pump failed - ostream returned an error */
    IOSTREAM_PUMP_STATUS_OUTPUT_ERROR,
};

typedef void io_callback_t(void *context);
typedef void iostream_pump_callback_t(enum iostream_pump_status status, void *context);
typedef int stream_flush_callback_t(void *context);



struct ostream {
    /* Number of bytes sent via o_stream_send*() and similar functions.
       This is counting the input data. For example with a compressed
       ostream this is counting the uncompressed bytes. The compressed
       bytes could be counted from the parent ostream's offset.

       Seeking to a specified offset only makes sense if there is no
       difference between input and output data sizes (e.g. there are no
       wrapper ostreams changing the data). */
    unsigned int offset;

    /* errno for the last operation send/seek operation. cleared before
       each call. */
    int stream_errno;

    /* overflow is set when some of the data given to send()
       functions was neither sent nor buffered. It's never unset inside
       ostream code. */
    bool overflow:1;
    /* o_stream_send() writes all the data or returns failure */
    bool blocking:1;
    bool closed:1;

    struct ostream_private *real_stream;
};

struct ostream_private {
    void (*set_flush_callback)(struct ostream_private *stream,
                               stream_flush_callback_t *callback,
                               void *context);

    struct ostream *parent;

    stream_flush_callback_t *callback;

    void *context;
};

struct iostream_pump {
    int refcount;
    struct io* io;
    struct ostream *output;
    iostream_pump_callback_t *callback;
    void *context;

    bool waiting_output;
    bool completed;
};

struct io {
    const char *source_filename;
    unsigned int source_linenum;
    /* trigger I/O callback even if OS doesn't think there is input
       pending */
    bool pending;
    /* This IO event shouldn't be the only thing being waited on, because
       it would just result in infinite wait. */
    bool never_wait_alone;

    io_callback_t *callback;
    void *context;

    struct ioloop *ioloop;
    struct ioloop_context *ctx;
};

static int iostream_pump_flush(void *pump)
{
    int ret = 0;
    return ret;
}

static int iostream_pump_flush1(void *pump)
{
    int ret = 0;
    return ret;
}

// indirect invoked by _stream->set_flush_callback
static void
o_stream_default_set_flush_callback(struct ostream_private *_stream,
                                    stream_flush_callback_t *callback,
                                    void *context)
{
    _stream->callback = callback;
    _stream->context = context; // stream->context will be marked as escaped Type:Field
}

static void stream_send_io(struct ostream_private *stream) {
    // indirect-call
    stream->callback(stream->context);
}

int main() {
    struct ostream_private *stream;
    struct ostream_private *stream1;
    stream1->callback = &iostream_pump_flush1;
    void* context;
    o_stream_default_set_flush_callback(stream, iostream_pump_flush, context);
    stream_send_io(stream);
    return 0;
}