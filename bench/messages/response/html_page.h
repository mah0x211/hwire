/*
 * HTML document response with the modern security-header cluster
 * (github.com shaped; no cookies).
 * Evidence summary in messages/README.md.
 */
#ifndef BENCH_MSG_HTML_PAGE
#define BENCH_MSG_HTML_PAGE

static const unsigned char MSG_HTML_PAGE[] =
    "HTTP/1.1 200 OK\r\n"
    "Server: GitHub.com\r\n"
    "Date: Thu, 04 Sep 2026 00:00:00 GMT\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Cache-Control: max-age=0, private, must-revalidate\r\n"
    "ETag: W/\"a1b2c3d4e5f6-7a8b9c\"\r\n"
    "Last-Modified: Wed, 03 Sep 2026 23:00:00 GMT\r\n"
    "Vary: Accept-Encoding, Accept, X-Requested-With\r\n"
    "Content-Encoding: gzip\r\n"
    "Strict-Transport-Security: max-age=31536000; includeSubdomains\r\n"
    "X-Content-Type-Options: nosniff\r\n"
    "X-Frame-Options: DENY\r\n"
    "Referrer-Policy: origin-when-cross-origin, strict-origin-when-cross-origin\r\n"
    "Content-Security-Policy: default-src 'none'; base-uri 'self'; "
    "block-all-mixed-content; connect-src 'self' https://api.example.com; "
    "img-src 'self' data: https://cdn.example.com; script-src 'self'; "
    "style-src 'unsafe-inline'\r\n"
    "X-GitHub-Request-Id: ABCD:123456:7890ABC:12DEF34:56789A\r\n"
    "Content-Length: 125060\r\n"
    "Connection: keep-alive\r\n"
    "\r\n";

#endif /* BENCH_MSG_HTML_PAGE */
