/* @title Browser GET Request
 * @description Synthetic desktop Chromium navigation with client hints, fetch metadata, a Cookie header and four query parameters, including a percent-encoded value.
 */
#ifndef BENCH_BROWSER_GET_H
#define BENCH_BROWSER_GET_H
static const unsigned char MSG_BROWSER_GET[] =
    "GET /search?q=cache%20locality&page=2&sort=recent&lang=ja HTTP/1.1\r\n"
    "Host: app.example.com\r\n"
    "Connection: keep-alive\r\n"
    "Upgrade-Insecure-Requests: 1\r\n"
    "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8\r\n"
    "Accept-Encoding: gzip, deflate, br, zstd\r\n"
    "Accept-Language: ja,en-US;q=0.9,en;q=0.8\r\n"
    "Sec-Fetch-Site: same-origin\r\n"
    "Sec-Fetch-Mode: navigate\r\n"
    "Sec-Fetch-User: ?1\r\n"
    "Sec-Fetch-Dest: document\r\n"
    "Sec-CH-UA: \"Chromium\";v=\"131\", \"Not_A Brand\";v=\"24\"\r\n"
    "Sec-CH-UA-Mobile: ?0\r\n"
    "Sec-CH-UA-Platform: \"macOS\"\r\n"
    "Referer: https://app.example.com/\r\n"
    "Priority: u=0, i\r\n"
    "Cookie: session=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef; csrf=abcdef0123456789abcdef0123456789; locale=ja_JP; theme=dark; consent=%7B%22analytics%22%3Atrue%7D\r\n"
    "\r\n";
#endif
