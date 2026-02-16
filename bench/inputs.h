/*
 * HTTP Parser Benchmark Data
 * Based on modern browser requests through CDN/Load Balancer
 *
 * Modern HTTP Request Structure (28 headers):
 *   A. Browser standard (8): Host, Connection, User-Agent, Accept, Accept-Encoding,
 *                            Accept-Language, Cache-Control, Upgrade-Insecure-Requests
 *   B. Security metadata (7): Sec-CH-UA*, Sec-Fetch-*
 *   C. Application/Auth (5): Referer, Cookie, Authorization, X-CSRF-Token, X-Requested-With
 *   D. Infrastructure/CDN (8): X-Forwarded-*, Via, CDN-Loop, CF-*
 *
 * Note: All domains use .test TLD (RFC 2606 reserved)
 *       All IPs use TEST-NET ranges (RFC 5737): 192.0.2.0/24, 198.51.100.0/24, 203.0.113.0/24
 */

/* ============================================================================
 * Category 1: Header Count (Modern Browser Structure)
 * Purpose: Measure scaling with realistic header configurations
 * Control: Progressive addition of header groups (A → A+B → A+B+C → A+B+C+D)
 * ============================================================================ */

/* A. Browser Standard (8 headers) */
static unsigned char REQ_HDR_8[] =
    "GET / HTTP/1.1\r\n"
    "Host: example.test\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8\r\n"
    "Accept-Encoding: gzip, deflate, br\r\n"
    "Accept-Language: ja,en-US;q=0.9,en;q=0.8\r\n"
    "Cache-Control: max-age=0\r\n"
    "Upgrade-Insecure-Requests: 1\r\n"
    "\r\n";

/* A+B. Browser + Security Metadata (15 headers) */
static unsigned char REQ_HDR_15[] =
    "GET / HTTP/1.1\r\n"
    /* A. Browser standard (8) */
    "Host: example.test\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8\r\n"
    "Accept-Encoding: gzip, deflate, br\r\n"
    "Accept-Language: ja,en-US;q=0.9,en;q=0.8\r\n"
    "Cache-Control: max-age=0\r\n"
    "Upgrade-Insecure-Requests: 1\r\n"
    /* B. Security metadata (7) */
    "Sec-CH-UA: \"Not A(Brand\";v=\"99\", \"Google Chrome\";v=\"121\", \"Chromium\";v=\"121\"\r\n"
    "Sec-CH-UA-Mobile: ?0\r\n"
    "Sec-CH-UA-Platform: \"macOS\"\r\n"
    "Sec-Fetch-Site: same-origin\r\n"
    "Sec-Fetch-Mode: navigate\r\n"
    "Sec-Fetch-User: ?1\r\n"
    "Sec-Fetch-Dest: document\r\n"
    "\r\n";

/* A+B+C. Browser + Security + Auth (20 headers) */
static unsigned char REQ_HDR_20[] =
    "GET /dashboard HTTP/1.1\r\n"
    /* A. Browser standard (8) */
    "Host: example.test\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8\r\n"
    "Accept-Encoding: gzip, deflate, br\r\n"
    "Accept-Language: ja,en-US;q=0.9,en;q=0.8\r\n"
    "Cache-Control: max-age=0\r\n"
    "Upgrade-Insecure-Requests: 1\r\n"
    /* B. Security metadata (7) */
    "Sec-CH-UA: \"Not A(Brand\";v=\"99\", \"Google Chrome\";v=\"121\", \"Chromium\";v=\"121\"\r\n"
    "Sec-CH-UA-Mobile: ?0\r\n"
    "Sec-CH-UA-Platform: \"macOS\"\r\n"
    "Sec-Fetch-Site: same-origin\r\n"
    "Sec-Fetch-Mode: navigate\r\n"
    "Sec-Fetch-User: ?1\r\n"
    "Sec-Fetch-Dest: document\r\n"
    /* C. Application/Auth (5) */
    "Referer: https://example.test/login\r\n"
    "Cookie: session_id=xyz123abc; _ga=GA1.1.123456789.1234567890; _fbp=fb.1.1700000000.1234567890\r\n"
    "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c\r\n"
    "X-CSRF-Token: abc123xyz456csrf\r\n"
    "X-Requested-With: XMLHttpRequest\r\n"
    "\r\n";

/* A+B+C+D. Complete Modern Browser + CDN (28 headers) */
static unsigned char REQ_HDR_28[] =
    "GET /dashboard HTTP/1.1\r\n"
    /* A. Browser standard (8) */
    "Host: example.test\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8\r\n"
    "Accept-Encoding: gzip, deflate, br\r\n"
    "Accept-Language: ja,en-US;q=0.9,en;q=0.8\r\n"
    "Cache-Control: max-age=0\r\n"
    "Upgrade-Insecure-Requests: 1\r\n"
    /* B. Security metadata (7) */
    "Sec-CH-UA: \"Not A(Brand\";v=\"99\", \"Google Chrome\";v=\"121\", \"Chromium\";v=\"121\"\r\n"
    "Sec-CH-UA-Mobile: ?0\r\n"
    "Sec-CH-UA-Platform: \"macOS\"\r\n"
    "Sec-Fetch-Site: same-origin\r\n"
    "Sec-Fetch-Mode: navigate\r\n"
    "Sec-Fetch-User: ?1\r\n"
    "Sec-Fetch-Dest: document\r\n"
    /* C. Application/Auth (5) */
    "Referer: https://example.test/login\r\n"
    "Cookie: session_id=xyz123abc; _ga=GA1.1.123456789.1234567890; _fbp=fb.1.1700000000.1234567890\r\n"
    "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c\r\n"
    "X-CSRF-Token: abc123xyz456csrf\r\n"
    "X-Requested-With: XMLHttpRequest\r\n"
    /* D. Infrastructure/CDN (8) */
    "X-Forwarded-For: 192.0.2.1, 198.51.100.2\r\n"
    "X-Forwarded-Proto: https\r\n"
    "X-Forwarded-Port: 443\r\n"
    "Via: 1.1 proxy.test\r\n"
    "CDN-Loop: cloudflare\r\n"
    "CF-Ray: 856d1234abcd-NRT\r\n"
    "CF-Connecting-IP: 192.0.2.1\r\n"
    "CF-Visitor: {\"scheme\":\"https\"}\r\n"
    "\r\n";

/* ============================================================================
 * Category 2: Header Value Length
 * Purpose: Measure scaling with header value size
 * Control: Same 28-header structure, only value lengths vary
 * ============================================================================ */

/* Short values (~20 chars each) */
static unsigned char REQ_VAL_SHORT[] =
    "GET / HTTP/1.1\r\n"
    "Host: example.test\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: Chrome/121.0.0.0\r\n"
    "Accept: text/html\r\n"
    "Accept-Encoding: gzip\r\n"
    "Accept-Language: ja\r\n"
    "Cache-Control: max-age=0\r\n"
    "Upgrade-Insecure-Requests: 1\r\n"
    "Sec-CH-UA: \"Chrome\";v=\"121\"\r\n"
    "Sec-CH-UA-Mobile: ?0\r\n"
    "Sec-CH-UA-Platform: \"macOS\"\r\n"
    "Sec-Fetch-Site: same-origin\r\n"
    "Sec-Fetch-Mode: navigate\r\n"
    "Sec-Fetch-User: ?1\r\n"
    "Sec-Fetch-Dest: document\r\n"
    "Referer: https://example.test\r\n"
    "Cookie: session=abc123\r\n"
    "Authorization: Bearer token123\r\n"
    "X-CSRF-Token: csrf123\r\n"
    "X-Requested-With: XMLHttpRequest\r\n"
    "X-Forwarded-For: 192.0.2.1\r\n"
    "X-Forwarded-Proto: https\r\n"
    "X-Forwarded-Port: 443\r\n"
    "Via: 1.1 proxy.test\r\n"
    "CDN-Loop: cloudflare\r\n"
    "CF-Ray: 856d1234abcd-NRT\r\n"
    "CF-Connecting-IP: 192.0.2.1\r\n"
    "CF-Visitor: {\"scheme\":\"https\"}\r\n"
    "\r\n";

/* Medium values (~100 chars each) - with realistic User-Agent, Accept */
static unsigned char REQ_VAL_MEDIUM[] =
    "GET /dashboard HTTP/1.1\r\n"
    "Host: www.example.test\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8\r\n"
    "Accept-Encoding: gzip, deflate, br\r\n"
    "Accept-Language: ja,en-US;q=0.9,en;q=0.8\r\n"
    "Cache-Control: max-age=0\r\n"
    "Upgrade-Insecure-Requests: 1\r\n"
    "Sec-CH-UA: \"Not A(Brand\";v=\"99\", \"Google Chrome\";v=\"121\", \"Chromium\";v=\"121\"\r\n"
    "Sec-CH-UA-Mobile: ?0\r\n"
    "Sec-CH-UA-Platform: \"macOS\"\r\n"
    "Sec-Fetch-Site: same-origin\r\n"
    "Sec-Fetch-Mode: navigate\r\n"
    "Sec-Fetch-User: ?1\r\n"
    "Sec-Fetch-Dest: document\r\n"
    "Referer: https://www.example.test/products/category/item-page\r\n"
    "Cookie: session_id=medium123; preferences=theme=dark; tracking=enabled\r\n"
    "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.medium\r\n"
    "X-CSRF-Token: medium_csrf_token_value_123\r\n"
    "X-Requested-With: XMLHttpRequest\r\n"
    "X-Forwarded-For: 192.0.2.1, 198.51.100.2\r\n"
    "X-Forwarded-Proto: https\r\n"
    "X-Forwarded-Port: 443\r\n"
    "Via: 1.1 proxy.test\r\n"
    "CDN-Loop: cloudflare\r\n"
    "CF-Ray: 856d1234abcd-NRT\r\n"
    "CF-Connecting-IP: 192.0.2.1\r\n"
    "CF-Visitor: {\"scheme\":\"https\"}\r\n"
    "\r\n";

/* Long values (~300 chars for Cookie) */
static unsigned char REQ_VAL_LONG[] =
    "GET /dashboard HTTP/1.1\r\n"
    "Host: www.example.test\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8\r\n"
    "Accept-Encoding: gzip, deflate, br\r\n"
    "Accept-Language: ja,en-US;q=0.9,en;q=0.8\r\n"
    "Cache-Control: max-age=0\r\n"
    "Upgrade-Insecure-Requests: 1\r\n"
    "Sec-CH-UA: \"Not A(Brand\";v=\"99\", \"Google Chrome\";v=\"121\", \"Chromium\";v=\"121\"\r\n"
    "Sec-CH-UA-Mobile: ?0\r\n"
    "Sec-CH-UA-Platform: \"macOS\"\r\n"
    "Sec-Fetch-Site: same-origin\r\n"
    "Sec-Fetch-Mode: navigate\r\n"
    "Sec-Fetch-User: ?1\r\n"
    "Sec-Fetch-Dest: document\r\n"
    "Referer: https://www.example.test/login?redirect=%2Fdashboard%3Ftab%3Dsettings\r\n"
    "Cookie: session_id=long_session_id_value_abc123def456ghi789jkl012mno345pqr; user_preferences=lang%3Den%26theme%3Ddark%26timezone%3DAsia%2FTokyo%26notifications%3Dtrue; analytics_tracking=_ga=GA1.2.123456789.1234567890; _fbp=fb.1.1234567890123.1234567890; cart_data=item1%3D2%26item2%3D1%26item3%3D5\r\n"
    "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiZW1haWwiOiJ1c2VyQGV4YW1wbGUudGVzdCIsInJvbGUiOiJ1c2VyIn0.long\r\n"
    "X-CSRF-Token: long_csrf_token_value_with_additional_entropy_123\r\n"
    "X-Requested-With: XMLHttpRequest\r\n"
    "X-Forwarded-For: 192.0.2.1, 198.51.100.2, 203.0.113.1\r\n"
    "X-Forwarded-Proto: https\r\n"
    "X-Forwarded-Port: 443\r\n"
    "Via: 1.1 proxy.test, 1.1 cdn.test\r\n"
    "CDN-Loop: cloudflare\r\n"
    "CF-Ray: 856d1234abcd-NRT\r\n"
    "CF-Connecting-IP: 192.0.2.1\r\n"
    "CF-Visitor: {\"scheme\":\"https\"}\r\n"
    "\r\n";

/* Extra long values (~1000+ chars for Cookie with JWT) */
static unsigned char REQ_VAL_XLONG[] =
    "GET /dashboard HTTP/1.1\r\n"
    "Host: www.example.test\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8\r\n"
    "Accept-Encoding: gzip, deflate, br\r\n"
    "Accept-Language: ja,en-US;q=0.9,en;q=0.8\r\n"
    "Cache-Control: max-age=0\r\n"
    "Upgrade-Insecure-Requests: 1\r\n"
    "Sec-CH-UA: \"Not A(Brand\";v=\"99\", \"Google Chrome\";v=\"121\", \"Chromium\";v=\"121\"\r\n"
    "Sec-CH-UA-Mobile: ?0\r\n"
    "Sec-CH-UA-Platform: \"macOS\"\r\n"
    "Sec-Fetch-Site: same-origin\r\n"
    "Sec-Fetch-Mode: navigate\r\n"
    "Sec-Fetch-User: ?1\r\n"
    "Sec-Fetch-Dest: document\r\n"
    "Referer: https://www.example.test/products/electronics/computers/laptops/gaming-laptops/premium-gaming-laptop-model-2024\r\n"
    "Cookie: session_id=xlong_session_value_with_many_characters_abc123def456ghi789jkl012mno345pqr678stu901vwx234yz; user_preferences=lang%3Den%26theme%3Ddark%26timezone%3DAsia%2FTokyo%26notifications%3Dtrue%26email_opt_in%3Dyes%26personalization%3Denabled; analytics_first_party=_ga=GA1.1.123456789.1234567890; analytics_third_party=_ga_ZYXWVUTSRQPONMLKJIHGFEDCBA=GS1.1.1234567890.1.1.1234567890.0.0.0; facebook_pixel=_fbp=fb.1.1234567890123.1234567890123; google_ads=_gcl_au=1.1.1234567890.1234567890; shopping_cart=items%3D%5B%7B%22id%22%3A12345%2C%22qty%22%3A2%7D%2C%7B%22id%22%3A67890%2C%22qty%22%3A1%7D%5D; recently_viewed=%2Fproducts%2F123%2C%2Fproducts%2F456%2C%2Fproducts%2F789%2C%2Fproducts%2F012%2C%2Fproducts%2F345\r\n"
    "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiZW1haWwiOiJ1c2VyQGV4YW1wbGUudGVzdCIsInJvbGUiOiJhZG1pbiIsInBlcm1pc3Npb25zIjpbInJlYWQiLCJ3cml0ZSIsImRlbGV0ZSIsImFkbWluIl0sImlhdCI6MTUxNjIzOTAyMiwiaXNzIjoiZXhhbXBsZS50ZXN0IiwiYXVkIjoiYXBpLmV4YW1wbGUudGVzdCIsImV4cCI6MTcxNjIzOTAyMiwianRpIjoidW5pcXVlLWlkLTEyMzQ1Njc4OTAifQ.very_long_signature_here_for_testing_purposes\r\n"
    "X-CSRF-Token: xlong_csrf_token_with_additional_entropy_and_random_characters_123456789\r\n"
    "X-Requested-With: XMLHttpRequest\r\n"
    "X-Forwarded-For: 192.0.2.1, 198.51.100.2, 203.0.113.1, 198.51.100.3\r\n"
    "X-Forwarded-Proto: https\r\n"
    "X-Forwarded-Port: 443\r\n"
    "Via: 1.1 proxy.test, 1.1 cdn.test, 1.1 lb.test\r\n"
    "CDN-Loop: cloudflare, fastly\r\n"
    "CF-Ray: 856d1234abcd-NRT\r\n"
    "CF-Connecting-IP: 192.0.2.1\r\n"
    "CF-Visitor: {\"scheme\":\"https\"}\r\n"
    "\r\n";

/* ============================================================================
 * Category 3: Case Sensitivity
 * Purpose: Measure header name normalization cost
 * Control: Same 28-header structure, different case styles
 * ============================================================================ */

/* All lowercase (HTTP/2 style) */
static unsigned char REQ_CASE_LOWER[] =
    "GET /dashboard HTTP/1.1\r\n"
    "host: example.test\r\n"
    "connection: keep-alive\r\n"
    "user-agent: mozilla/5.0 (macintosh; intel mac os x 10_15_7) applewebkit/537.36 chrome/121.0.0.0 safari/537.36\r\n"
    "accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8\r\n"
    "accept-encoding: gzip, deflate, br\r\n"
    "accept-language: ja,en-us;q=0.9,en;q=0.8\r\n"
    "cache-control: max-age=0\r\n"
    "upgrade-insecure-requests: 1\r\n"
    "sec-ch-ua: \"not a(brand\";v=\"99\", \"google chrome\";v=\"121\", \"chromium\";v=\"121\"\r\n"
    "sec-ch-ua-mobile: ?0\r\n"
    "sec-ch-ua-platform: \"macos\"\r\n"
    "sec-fetch-site: same-origin\r\n"
    "sec-fetch-mode: navigate\r\n"
    "sec-fetch-user: ?1\r\n"
    "sec-fetch-dest: document\r\n"
    "referer: https://example.test/login\r\n"
    "cookie: session_id=xyz123abc; _ga=ga1.1.123456789.1234567890\r\n"
    "authorization: bearer eyjhbgniwinr5ccg6ikpxvcj9.eyjzdwiioiixmjmonty3odkwiiwibmftzsi6ikpvaeguig9lijohn.doe@example.test\r\n"
    "x-csrf-token: abc123xyz456\r\n"
    "x-requested-with: xmlhttprequest\r\n"
    "x-forwarded-for: 192.0.2.1, 198.51.100.2\r\n"
    "x-forwarded-proto: https\r\n"
    "x-forwarded-port: 443\r\n"
    "via: 1.1 proxy.test\r\n"
    "cdn-loop: cloudflare\r\n"
    "cf-ray: 856d1234abcd-nrt\r\n"
    "cf-connecting-ip: 192.0.2.1\r\n"
    "cf-visitor: {\"scheme\":\"https\"}\r\n"
    "\r\n";

/* Mixed case (HTTP/1.1 through proxy style) */
static unsigned char REQ_CASE_MIXED[] =
    "GET /dashboard HTTP/1.1\r\n"
    "HOST: Example.TEST\r\n"
    "Connection: Keep-Alive\r\n"
    "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 Chrome/121.0.0.0 Safari/537.36\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8\r\n"
    "Accept-Encoding: gzip, deflate, br\r\n"
    "Accept-Language: ja,en-US;q=0.9,en;q=0.8\r\n"
    "Cache-Control: max-age=0\r\n"
    "Upgrade-Insecure-Requests: 1\r\n"
    "Sec-CH-UA: \"Not A(Brand\";v=\"99\", \"Google Chrome\";v=\"121\", \"Chromium\";v=\"121\"\r\n"
    "Sec-CH-UA-Mobile: ?0\r\n"
    "SEC-CH-UA-Platform: \"macOS\"\r\n"
    "Sec-Fetch-Site: same-origin\r\n"
    "SEC-FETCH-MODE: navigate\r\n"
    "Sec-Fetch-User: ?1\r\n"
    "Sec-Fetch-Dest: document\r\n"
    "Referer: https://example.test/login\r\n"
    "COOKIE: session_id=xyz123abc; _ga=GA1.1.123456789.1234567890\r\n"
    "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiZW1haWwiOiJ1c2VyQGV4YW1wbGUudGVzdCJ9.signature\r\n"
    "X-CSRF-Token: abc123xyz456\r\n"
    "X-Requested-With: XMLHttpRequest\r\n"
    "X-Forwarded-For: 192.0.2.1, 198.51.100.2\r\n"
    "X-FORWARDED-PROTO: https\r\n"
    "X-Forwarded-Port: 443\r\n"
    "Via: 1.1 proxy.test\r\n"
    "CDN-Loop: cloudflare\r\n"
    "CF-Ray: 856d1234abcd-NRT\r\n"
    "CF-Connecting-IP: 192.0.2.1\r\n"
    "CF-Visitor: {\"scheme\":\"https\"}\r\n"
    "\r\n";

/* ============================================================================
 * Category 4: Real-World Requests
 * Purpose: Reference for actual usage patterns
 * ============================================================================ */

/* Modern browser + Cloudflare (full 28 headers) */
static unsigned char REQ_REAL_BROWSER[] =
    "GET /products/electronics/laptops HTTP/1.1\r\n"
    /* A. Browser standard */
    "Host: www.techstore.test\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36\r\n"
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8\r\n"
    "Accept-Encoding: gzip, deflate, br\r\n"
    "Accept-Language: ja,en-US;q=0.9,en;q=0.8\r\n"
    "Cache-Control: max-age=0\r\n"
    "Upgrade-Insecure-Requests: 1\r\n"
    /* B. Security metadata */
    "Sec-CH-UA: \"Google Chrome\";v=\"121\", \"Not:A-Brand\";v=\"99\", \"Chromium\";v=\"121\"\r\n"
    "Sec-CH-UA-Mobile: ?0\r\n"
    "Sec-CH-UA-Platform: \"macOS\"\r\n"
    "Sec-Fetch-Site: same-origin\r\n"
    "Sec-Fetch-Mode: navigate\r\n"
    "Sec-Fetch-User: ?1\r\n"
    "Sec-Fetch-Dest: document\r\n"
    /* C. Application */
    "Referer: https://www.techstore.test/\r\n"
    "Cookie: session_id=user_session_abc123; cart_id=cart_xyz789; recently_viewed=prod_001,prod_002,prod_003; _ga=GA1.1.987654321.1700000000; _gid=GA1.2.123456789.1700000000\r\n"
    "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.logged_in_user\r\n"
    "X-CSRF-Token: store_csrf_protection_token\r\n"
    "X-Requested-With: XMLHttpRequest\r\n"
    /* D. Infrastructure */
    "X-Forwarded-For: 203.0.113.195, 198.51.100.1\r\n"
    "X-Forwarded-Proto: https\r\n"
    "X-Forwarded-Port: 443\r\n"
    "Via: 1.1 cdn.test, 1.1 lb.test\r\n"
    "CDN-Loop: cloudflare\r\n"
    "CF-Ray: 85abc123defghi-Tokyo\r\n"
    "CF-Connecting-IP: 203.0.113.195\r\n"
    "CF-Visitor: {\"scheme\":\"https\"}\r\n"
    "\r\n";

/* API request with security headers (15 headers) */
static unsigned char REQ_REAL_API[] =
    "GET /api/v2/users/me HTTP/1.1\r\n"
    "Host: api.example.test\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: MyApp/2.0.0 (https://myapp.test)\r\n"
    "Accept: application/json\r\n"
    "Accept-Encoding: gzip, deflate, br\r\n"
    "Accept-Language: en-US\r\n"
    "Cache-Control: no-cache\r\n"
    "Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJ1c2VyXzEyMzQ1Iiwic2NvcGUiOiJyZWFkIHdyaXRlIiwiZXhwIjoxNzAwMDAwMDAwfQ.api_signature\r\n"
    "X-Request-ID: 550e8400-e29b-41d4-a716-446655440000\r\n"
    "X-Correlation-ID: corr-550e8400-e29b-41d4\r\n"
    "X-Forwarded-For: 192.0.2.10\r\n"
    "X-Forwarded-Proto: https\r\n"
    "X-RateLimit-Remaining: 99\r\n"
    "X-API-Version: 2.0\r\n"
    "\r\n";

/* Mobile app request (20 headers) */
static unsigned char REQ_REAL_MOBILE[] =
    "GET /api/v2/sync?since=2024-01-01T00:00:00Z HTTP/1.1\r\n"
    "Host: api.example.test\r\n"
    "Connection: keep-alive\r\n"
    "User-Agent: MyApp/3.0.0 (iOS 17.2; iPhone15,3; Build/456)\r\n"
    "Accept: application/json\r\n"
    "Accept-Encoding: gzip, deflate, br\r\n"
    "Accept-Language: ja-JP\r\n"
    "Cache-Control: no-cache\r\n"
    "Authorization: Bearer mobile_token_xyz789abc\r\n"
    "X-Device-ID: ABC123-DEF456-GHI789-JKL012\r\n"
    "X-App-Version: 3.0.0\r\n"
    "X-Platform: iOS\r\n"
    "X-OS-Version: 17.2\r\n"
    "X-Network-Type: WiFi\r\n"
    "X-Request-ID: mobile-req-12345678\r\n"
    "X-Forwarded-For: 198.51.100.50\r\n"
    "X-Forwarded-Proto: https\r\n"
    "X-Forwarded-Port: 443\r\n"
    "CF-Ray: mobile-cf-ray-id\r\n"
    "CF-Connecting-IP: 198.51.100.50\r\n"
    "\r\n";

/* ============================================================================
 * Category 5: Baseline
 * Purpose: Minimum parsing cost for comparison
 * ============================================================================ */

/* Absolute minimum - parser overhead only */
static unsigned char REQ_MINIMAL[] =
    "GET / HTTP/1.1\r\n"
    "\r\n";

/* HTTP/1.1 minimum with Host (required for compliance) */
static unsigned char REQ_MINIMAL_HOST[] =
    "GET / HTTP/1.1\r\n"
    "Host: example.test\r\n"
    "\r\n";
