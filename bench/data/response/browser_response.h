/* @title Browser Response
 * @description Synthetic HTML response with cache validators, security policies and repeated Set-Cookie and Link fields. The body is excluded.
 */
#ifndef BENCH_BROWSER_RESPONSE_H
#define BENCH_BROWSER_RESPONSE_H
static const unsigned char MSG_BROWSER_RESPONSE[] =
    "HTTP/1.1 200 OK\r\n"
    "Date: Tue, 06 Oct 2026 00:00:00 GMT\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Content-Length: 8192\r\n"
    "Content-Encoding: br\r\n"
    "Connection: keep-alive\r\n"
    "Cache-Control: private, max-age=0, must-revalidate\r\n"
    "ETag: W/\"page-20261006\"\r\n"
    "Last-Modified: Mon, 05 Oct 2026 12:00:00 GMT\r\n"
    "Vary: Accept-Encoding, Cookie\r\n"
    "Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n"
    "Content-Security-Policy: default-src 'self'; script-src 'self' 'nonce-benchmark123'; object-src 'none'; frame-ancestors 'none'\r\n"
    "X-Content-Type-Options: nosniff\r\n"
    "Referrer-Policy: strict-origin-when-cross-origin\r\n"
    "Permissions-Policy: camera=(), microphone=(), geolocation=()\r\n"
    "Set-Cookie: session=0123456789abcdef; Path=/; Secure; HttpOnly; SameSite=Lax\r\n"
    "Set-Cookie: csrf=abcdef0123456789; Path=/; Secure; SameSite=Lax\r\n"
    "Set-Cookie: locale=ja_JP; Path=/; Secure; SameSite=Lax\r\n"
    "Link: </assets/app.css>; rel=preload; as=style\r\n"
    "Link: </assets/app.js>; rel=preload; as=script\r\n"
    "\r\n";
#endif
