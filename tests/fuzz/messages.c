#include "check.h"

static int check_request(hwire_ctx_t *ctx, hwire_request_t *r)
{
    callback_state_t *s = ctx->uctx;
    hwire_str_t fields[] = {r->method, r->uri, r->scheme, r->userinfo,
                           r->host, r->port, r->path, r->query};
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        check_slice(fields[i], s->input, s->len);
    }
    return callback_done(s);
}

static int check_response(hwire_ctx_t *ctx, hwire_response_t *r)
{
    callback_state_t *s = ctx->uctx;
    check_slice(r->reason, s->input, s->len);
    return callback_done(s);
}

static void run(const uint8_t options[4], const uint8_t *data, size_t len)
{
    char *input = copy_input(data, len);
    size_t start = options[2] % (len + 1);
    size_t remaining = len - start;
    size_t budget = remaining + 1;
    if ((options[1] & 3) == 1) {
        budget = remaining;
    } else if ((options[1] & 3) == 2) {
        budget = options[3] % (remaining + 1);
    }
    callback_state_t state = {.input = input, .len = len,
        .stop_at = (options[1] & 8) ? 1 + options[3] % 4 : 0};
    hwire_ctx_t ctx = {.uctx = &state,
        .request_cb = check_request, .response_cb = check_response,
        .header_cb = check_pair};
    size_t pos = start;
    int code;
    switch (options[0] % 3) {
    case 0:
        code = hwire_parse_request(&ctx, input, len, &pos, budget);
        break;
    case 1:
        code = hwire_parse_response(&ctx, input, len, &pos, budget);
        break;
    default:
        code = hwire_parse_headers(&ctx, input, len, &pos, budget);
        break;
    }
    CHECK(pos >= start && pos <= len);
    if (code != HWIRE_OK) {
        CHECK(pos == start);
    } else {
        CHECK(pos - start <= budget);
    }
    if (state.stopped) {
        CHECK(code == HWIRE_ECALLBACK);
    }
    free(input);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 4) {
        return 0;
    }
    size -= 4;
    /* Each call has fresh callback state and an exact-size input allocation. */
    run(data, data + 4, size / 2);
    run(data, data + 4, size);
    return 0;
}
