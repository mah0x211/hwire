#include "check.h"

static int check_query(hwire_ctx_t *ctx, hwire_kv_pair_t *pair)
{
    callback_state_t *s = ctx->uctx;
    check_slice(pair->key, s->decoded, s->decoded_size);
    check_slice(pair->value, s->decoded, s->decoded_size);
    return callback_done(s);
}

static int check_chunk(hwire_ctx_t *ctx, uint32_t size)
{
    (void)size;
    return callback_done(ctx->uctx);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 4) {
        return 0;
    }
    size_t len = size - 4;
    char *input = copy_input(data + 4, len);
    size_t start = data[2] % (len + 1);
    size_t capacity = (data[1] & 4) ? data[3] % (len + 1) : len;
    char *key = malloc(capacity ? capacity : 1);
    char *decoded = malloc(capacity ? capacity : 1);
    CHECK(key && decoded);
    callback_state_t state = {.input = input, .len = len,
        .decoded = decoded, .decoded_size = capacity,
        .stop_at = (data[1] & 8) ? 1 + data[3] % 4 : 0};
    hwire_ctx_t ctx = {.uctx = &state,
        .key_lc = {.buf = key, .size = capacity},
        .qrybuf = {.buf = decoded, .size = capacity},
        .param_cb = check_pair, .query_cb = check_query,
        .chunksize_cb = check_chunk, .chunksize_ext_cb = check_pair};
    size_t remaining = len - start;
    size_t budget = remaining + 1;
    if ((data[1] & 3) == 1) {
        budget = remaining;
    } else if ((data[1] & 3) == 2) {
        budget = data[3] % (remaining + 1);
    }
    size_t pos = start;
    int code = HWIRE_OK;
    switch (data[0] % 7) {
    case 0:
        code = hwire_parse_parameters(&ctx, input, len, &pos, budget, data[1] & 16);
        break;
    case 1:
        code = hwire_parse_query(&ctx, input, len, &pos, budget);
        break;
    case 2:
        code = hwire_parse_chunksize(&ctx, input, len, &pos, budget);
        if (code != HWIRE_OK) {
            CHECK(pos == start);
        }
        break;
    case 3:
        if (start < len && input[start] == '\"') {
            code = hwire_parse_quoted_string(input, len, &pos, budget);
        }
        break;
    default: {
        size_t count;
        unsigned int which = data[0] % 7;
        if (which == 4) {
            count = hwire_parse_tchar(input, len, &pos);
        } else if (which == 5) {
            count = hwire_parse_vchar(input, len, &pos);
        } else {
            count = hwire_parse_fcchar(input, len, &pos);
        }
        size_t expected = start;
        while (expected < len) {
            unsigned char c = (unsigned char)input[expected];
            int valid = which == 4 ? hwire_is_tchar(c) :
                        which == 5 ? hwire_is_vchar(c) : hwire_is_fcchar(c);
            if (!valid) {
                break;
            }
            expected++;
        }
        CHECK(pos == expected && count == expected - start);
        break;
    }
    }
    CHECK(pos >= start && pos <= len);
    CHECK(ctx.key_lc.len <= capacity && ctx.qrybuf.len <= capacity);
    if (state.stopped) {
        CHECK(code == HWIRE_ECALLBACK);
    }
    free(decoded);
    free(key);
    free(input);
    return 0;
}
