/*
 * Modern browser navigation request (2026, Chrome-shaped).
 * Evidence: 2026-09 public-service captures (summary in messages/README.md).
 * and Sec-CH-UA families are standard on current navigations.
 */
#ifndef BENCH_MSG_BROWSER_PAGE_LOAD
#define BENCH_MSG_BROWSER_PAGE_LOAD

static const unsigned char MSG_BROWSER_PAGE_LOAD[] =
    "GET /repository/search?q=http+parsing HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "Connection: keep-alive\r\n"
    "Upgrade-Insecure-Requests: 1\r\n"
    "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 "
    "Safari/537.36\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,"
    "image/avif,image/webp,*/*;q=0.8\r\n"
    "Accept-Language: ja,en-US;q=0.9,en;q=0.8\r\n"
    "Accept-Encoding: gzip, deflate, br, zstd\r\n"
    "Referer: https://example.com/\r\n"
    "Sec-Fetch-Site: same-origin\r\n"
    "Sec-Fetch-Mode: navigate\r\n"
    "Sec-Fetch-User: ?1\r\n"
    "Sec-Fetch-Dest: document\r\n"
    "Sec-CH-UA: \"Chromium\";v=\"131\", \"Not_A Brand\";v=\"24\"\r\n"
    "Sec-CH-UA-Mobile: ?0\r\n"
    "Sec-CH-UA-Platform: \"macOS\"\r\n"
    "\r\n";

#endif /* BENCH_MSG_BROWSER_PAGE_LOAD */
