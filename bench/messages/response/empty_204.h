/*
 * Minimal 204 No Content response: the high-RPS infrastructure reply
 * (health probes, prefetch keepalives, tracking beacons).
 */
#ifndef BENCH_MSG_EMPTY_204
#define BENCH_MSG_EMPTY_204

static const unsigned char MSG_EMPTY_204[] =
    "HTTP/1.1 204 No Content\r\n"
    "Server: nginx/1.24.0\r\n"
    "Date: Thu, 04 Sep 2026 00:00:00 GMT\r\n"
    "Connection: keep-alive\r\n"
    "\r\n";

#endif /* BENCH_MSG_EMPTY_204 */
