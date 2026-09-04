/*
 * API fetch call from a browser or SDK client: bearer authorization,
 * correlation / W3C trace headers, JSON content negotiation.
 * (api.github.com and modern API-gateway shaped.)
 */
#ifndef BENCH_MSG_API_CALL
#define BENCH_MSG_API_CALL

static const unsigned char MSG_API_CALL[] =
    "GET /api/v1/users/12345/profile?fields=name,avatar,email HTTP/1.1\r\n"
    "Host: api.example.com\r\n"
    "Connection: keep-alive\r\n"
    "Authorization: Bearer "
    "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
    "eyJzdWIiOiIxMjM0NTY3ODkwIiwic2NvcGUiOiJyZWFkOnVzZXJzIn0."
    "SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c\r\n"
    "Accept: application/json\r\n"
    "Accept-Encoding: gzip, deflate, br\r\n"
    "User-Agent: example-app/2.1.0 (darwin; arm64) okhttp/4.12.0\r\n"
    "X-Request-Id: 550e8400-e29b-41d4-a716-446655440000\r\n"
    "X-Client-Trace-Id: 4bf92f3577b34da6a3ce929d0e0e4736\r\n"
    "traceparent: 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01\r\n"
    "If-None-Match: W/\"5f2d-52bd-eedd00\"\r\n"
    "\r\n";

#endif /* BENCH_MSG_API_CALL */
