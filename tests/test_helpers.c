#include "test_helpers.h"

int g_tests_run = 0;
int g_tests_passed = 0;
int g_tests_failed = 0;

/* Common mock callback functions (success) */
int mock_header_cb(hwire_callbacks_t *cb, hwire_header_t *header) {
    (void)cb;
    (void)header;
    return 0;
}

int mock_request_cb(hwire_callbacks_t *cb, hwire_request_t *req) {
    (void)cb;
    (void)req;
    return 0;
}

int mock_response_cb(hwire_callbacks_t *cb, hwire_response_t *rsp) {
    (void)cb;
    (void)rsp;
    return 0;
}

int mock_param_cb(hwire_callbacks_t *cb, hwire_param_t *param) {
    (void)cb;
    (void)param;
    return 0;
}

int mock_chunksize_cb(hwire_callbacks_t *cb, uint32_t size) {
    (void)cb;
    (void)size;
    return 0;
}

int mock_chunksize_ext_cb(hwire_callbacks_t *cb, hwire_chunksize_ext_t *ext) {
    (void)cb;
    (void)ext;
    return 0;
}

/* Common mock callback functions (failure) */
int mock_header_cb_fail(hwire_callbacks_t *cb, hwire_header_t *header) {
    (void)cb;
    (void)header;
    return 1;
}

int mock_request_cb_fail(hwire_callbacks_t *cb, hwire_request_t *req) {
    (void)cb;
    (void)req;
    return 1;
}

int mock_response_cb_fail(hwire_callbacks_t *cb, hwire_response_t *rsp) {
    (void)cb;
    (void)rsp;
    return 1;
}

int mock_param_cb_fail(hwire_callbacks_t *cb, hwire_param_t *param) {
    (void)cb;
    (void)param;
    return 1;
}

int mock_chunksize_cb_fail(hwire_callbacks_t *cb, uint32_t size) {
    (void)cb;
    (void)size;
    return 1;
}

int mock_chunksize_ext_cb_fail(hwire_callbacks_t *cb, hwire_chunksize_ext_t *ext) {
    (void)cb;
    (void)ext;
    return 1;
}

void print_test_summary(void) {
    fprintf(stdout, "\nTest Summary:\n");
    fprintf(stdout, "  Run:    %d\n", g_tests_run);
    fprintf(stdout, "  Passed: %d\n", g_tests_passed);
    fprintf(stdout, "  Failed: %d\n", g_tests_failed);
}
