/*
 * JSON API response with the platform-header cluster
 * (api.github.com shaped: 25 headers, rate limits, request ids).
 * Evidence: 2026-09 public-service captures (summary in messages/README.md).
 */
#ifndef BENCH_MSG_API_JSON
#define BENCH_MSG_API_JSON

static const unsigned char MSG_API_JSON[] =
    "HTTP/1.1 200 OK\r\n"
    "Server: GitHub.com\r\n"
    "Date: Thu, 04 Sep 2026 00:00:00 GMT\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Cache-Control: public, max-age=60, s-maxage=60\r\n"
    "Vary: Accept, Authorization, Cookie, X-GitHub-OTP\r\n"
    "ETag: \"0d493a2f1284a2d1c8e89e6a1a2b3c4d5e6f7a8b\"\r\n"
    "Last-Modified: Wed, 03 Sep 2026 22:00:00 GMT\r\n"
    "X-GitHub-Media-Type: github.v3; format=json\r\n"
    "x-github-api-version-selected: 2022-11-28\r\n"
    "Access-Control-Expose-Headers: ETag, Link, Location, "
    "Retry-After, X-GitHub-OTP, X-RateLimit-Used, X-RateLimit-Limit, "
    "X-RateLimit-Remaining, X-RateLimit-Reset, X-OAuth-Scopes, "
    "X-Accepted-OAuth-Scopes, X-GitHub-Media-Type\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Strict-Transport-Security: max-age=31536000; includeSubdomains;"
    " preload\r\n"
    "X-Content-Type-Options: nosniff\r\n"
    "X-Frame-Options: deny\r\n"
    "X-XSS-Protection: 0\r\n"
    "Referrer-Policy: origin-when-cross-origin, strict-origin-when-cross-origin\r\n"
    "x-ratelimit-limit: 5000\r\n"
    "x-ratelimit-remaining: 4993\r\n"
    "x-ratelimit-reset: 1762840200\r\n"
    "x-ratelimit-used: 7\r\n"
    "x-ratelimit-resource: core\r\n"
    "Content-Encoding: gzip\r\n"
    "X-GitHub-Request-Id: ABCD:123456:7890ABC:12DEF34:56789A\r\n"
    "Content-Length: 1382\r\n"
    "\r\n";

#endif /* BENCH_MSG_API_JSON */
