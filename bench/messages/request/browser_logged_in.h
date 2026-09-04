/*
 * Logged-in browser navigation request with a heavy Cookie header
 * (github.com / facebook.com shaped; captures show 2-4 KB cookie
 * clusters). Cookie content is synthetic.
 */
#ifndef BENCH_MSG_BROWSER_LOGGED_IN
#define BENCH_MSG_BROWSER_LOGGED_IN

static const unsigned char MSG_BROWSER_LOGGED_IN[] =
    "GET /home HTTP/1.1\r\n"
    "Host: social.example.net\r\n"
    "Connection: keep-alive\r\n"
    "Upgrade-Insecure-Requests: 1\r\n"
    "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 "
    "Safari/537.36\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,"
    "image/avif,image/webp,*/*;q=0.8\r\n"
    "Accept-Language: ja,en-US;q=0.9,en;q=0.8\r\n"
    "Accept-Encoding: gzip, deflate, br, zstd\r\n"
    "Sec-Fetch-Site: same-origin\r\n"
    "Sec-Fetch-Mode: navigate\r\n"
    "Sec-Fetch-Dest: document\r\n"
    "Sec-CH-UA: \"Chromium\";v=\"131\", \"Not_A Brand\";v=\"24\"\r\n"
    "Sec-CH-UA-Mobile: ?0\r\n"
    "Sec-CH-UA-Platform: \"macOS\"\r\n"
    "Cookie: session=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
    "eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkphbiBEb2UiLCJhZG1pbiI6"
    "dHJ1ZX0.abc123def456; _ga=GA1.2.1234567890.1234567890; "
    "_gid=GA1.2.9876543210.1357913579; _fbp=fb.1.1234567890123."
    "987654321; _gat_gtag_UA_123456789_1=1; "
    "locale=ja_JP; theme=dark; tz=Asia%2FTokyo; "
    "consent=%7B%22analytics%22%3Atrue%2C%22marketing%22%3Afalse%7D; "
    "csrftoken=xYzAbC123456789012345678901234567890; "
    "recently_viewed=12345%2C67890%2C24680%2C13579%2C11223%2C44556; "
    "ab_bucket=experiment_c_2026q3_variant_7; "
    "device_id=8f14e45fceea167a5a36dedd4bea2543; "
    "tracking_id=tqid.998877665544332211.1762839204\r\n"
    "\r\n";

#endif /* BENCH_MSG_BROWSER_LOGGED_IN */
