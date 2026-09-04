/*
 * HTML response with a heavy Set-Cookie cluster (facebook.com /
 * tiktok.com shaped; captures show 2-4 KB of cookies). Cookie content
 * is synthetic.
 */
#ifndef BENCH_MSG_HTML_SET_COOKIES
#define BENCH_MSG_HTML_SET_COOKIES

static const unsigned char MSG_HTML_SET_COOKIES[] =
    "HTTP/1.1 200 OK\r\n"
    "Server: proxy\r\n"
    "Date: Thu, 04 Sep 2026 00:00:00 GMT\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Cache-Control: no-cache, no-store, must-revalidate, private\r\n"
    "Pragma: no-cache\r\n"
    "Expires: Sat, 01 Jan 2000 00:00:00 GMT\r\n"
    "Vary: Accept-Encoding, User-Agent, X-Forwarded-For, "
    "Accept-Language, Cookie\r\n"
    "Content-Encoding: br\r\n"
    "Strict-Transport-Security: max-age=15552000; preload\r\n"
    "X-Content-Type-Options: nosniff\r\n"
    "X-Frame-Options: SAMEORIGIN\r\n"
    "Set-Cookie: sessionid=8f14e45fceea167a5a36dedd4bea2543a9c;"
    " Domain=.social.example.net; Path=/; Secure; HttpOnly;"
    " SameSite=Lax; Max-Age=7776000\r\n"
    "Set-Cookie: tracker=tqid.998877665544332211.1762839204;"
    " Domain=.social.example.net; Path=/; Secure; SameSite=None;"
    " Max-Age=31536000\r\n"
    "Set-Cookie: consent=%7B%22analytics%22%3Atrue%2C%22marketing%22%3A"
    "false%2C%22version%22%3A%222026q3%22%7D; Path=/; Secure;"
    " SameSite=Lax; Max-Age=15552000\r\n"
    "Set-Cookie: ab_bucket=experiment_c_2026q3_variant_7; Path=/;"
    " Secure; SameSite=Lax; Max-Age=604800\r\n"
    "Set-Cookie: device_region=JP-13; Path=/; Secure; Max-Age=2592000\r\n"
    "X-Powered-By: PHP/8.3.11\r\n"
    "Content-Length: 89214\r\n"
    "Connection: keep-alive\r\n"
    "\r\n";

#endif /* BENCH_MSG_HTML_SET_COOKIES */
