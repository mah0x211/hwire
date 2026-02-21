/*
 * HTTP Response Parser Benchmark Data
 *
 * Modeled after realistic server responses through CDN/reverse proxy.
 *
 * Note: All domains use .test TLD (RFC 2606 reserved)
 *       All IPs use TEST-NET ranges (RFC 5737): 192.0.2.0/24
 */

/* ============================================================================
 * Category 1: Header Count (Typical Server Response Structure)
 * Purpose: Measure scaling with realistic response header configurations
 * Control: Progressive addition of header groups (A → A+B → A+B+C → A+B+C+D)
 * ============================================================================
 */

/* A. Minimal server (4 headers) ~144B */
static unsigned char RSP_HDR_4[] = "HTTP/1.1 200 OK\r\n"
                                   "Date: Thu, 20 Feb 2026 09:00:00 GMT\r\n"
                                   "Content-Type: text/html; charset=utf-8\r\n"
                                   "Content-Length: 1234\r\n"
                                   "Connection: keep-alive\r\n"
                                   "\r\n";

/* A+B. Basic server (8 headers) ~263B */
static unsigned char RSP_HDR_8[] =
    "HTTP/1.1 200 OK\r\n"
    "Date: Thu, 20 Feb 2026 09:00:00 GMT\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Content-Length: 1234\r\n"
    "Connection: keep-alive\r\n"
    "Server: nginx/1.25.3\r\n"
    "Cache-Control: no-cache, no-store, must-revalidate\r\n"
    "ETag: \"abc123def456\"\r\n"
    "Vary: Accept-Encoding\r\n"
    "\r\n";

/* A+B+C. App server (12 headers) ~400B */
static unsigned char RSP_HDR_12[] =
    "HTTP/1.1 200 OK\r\n"
    "Date: Thu, 20 Feb 2026 09:00:00 GMT\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Content-Length: 2048\r\n"
    "Connection: keep-alive\r\n"
    "Server: nginx/1.25.3\r\n"
    "Cache-Control: no-cache, no-store, must-revalidate\r\n"
    "ETag: \"abc123def456\"\r\n"
    "Vary: Accept-Encoding, Authorization\r\n"
    "Content-Encoding: gzip\r\n"
    "Access-Control-Allow-Origin: https://app.example.test\r\n"
    "X-Content-Type-Options: nosniff\r\n"
    "X-RateLimit-Limit: 1000\r\n"
    "\r\n";

/* A+B+C+D. CDN-proxied (20 headers) ~680B */
static unsigned char RSP_HDR_20[] =
    "HTTP/1.1 200 OK\r\n"
    "Date: Thu, 20 Feb 2026 09:00:00 GMT\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Content-Length: 5678\r\n"
    "Connection: keep-alive\r\n"
    "Server: cloudflare\r\n"
    "Cache-Control: max-age=0, must-revalidate\r\n"
    "ETag: \"abc123def456789\"\r\n"
    "Vary: Accept-Encoding\r\n"
    "Content-Encoding: gzip\r\n"
    "Access-Control-Allow-Origin: https://app.example.test\r\n"
    "X-Content-Type-Options: nosniff\r\n"
    "X-Frame-Options: SAMEORIGIN\r\n"
    "X-XSS-Protection: 1; mode=block\r\n"
    "Strict-Transport-Security: max-age=31536000; includeSubDomains; "
    "preload\r\n"
    "Referrer-Policy: strict-origin-when-cross-origin\r\n"
    "CF-Ray: 1234567890abcdef-NRT\r\n"
    "CF-Cache-Status: DYNAMIC\r\n"
    "Age: 0\r\n"
    "Via: 1.1 cloudflare\r\n"
    "\r\n";

/* ============================================================================
 * Category 2: Header Value Length
 * Purpose: Measure parser behavior across different value lengths
 * Control: Fixed 8 headers, varying value length
 * ============================================================================
 */

/* Short values (~10-15B each) ~198B */
static unsigned char RSP_VAL_SHORT[] = "HTTP/1.1 200 OK\r\n"
                                       "Date: Thu, 20 Feb 2026 09:00:00 GMT\r\n"
                                       "Content-Type: text/html\r\n"
                                       "Content-Length: 512\r\n"
                                       "Connection: keep-alive\r\n"
                                       "Server: nginx\r\n"
                                       "Cache-Control: no-cache\r\n"
                                       "ETag: \"a1b2c3\"\r\n"
                                       "Vary: Accept\r\n"
                                       "\r\n";

/* Medium values (~35-60B each) ~330B */
static unsigned char RSP_VAL_MEDIUM[] =
    "HTTP/1.1 200 OK\r\n"
    "Date: Thu, 20 Feb 2026 09:00:00 GMT\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Content-Length: 4096\r\n"
    "Connection: keep-alive\r\n"
    "Server: Apache/2.4.54 (Ubuntu)\r\n"
    "Cache-Control: public, max-age=3600, stale-while-revalidate=86400\r\n"
    "ETag: W/\"abc123def456789012345678\"\r\n"
    "Vary: Accept-Encoding, Accept-Language\r\n"
    "\r\n";

/* Long values (~100-200B each) ~710B */
static unsigned char RSP_VAL_LONG[] =
    "HTTP/1.1 200 OK\r\n"
    "Date: Thu, 20 Feb 2026 09:00:00 GMT\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Content-Length: 8192\r\n"
    "Connection: keep-alive\r\n"
    "Server: cloudflare\r\n"
    "Strict-Transport-Security: max-age=31536000; includeSubDomains; "
    "preload\r\n"
    "Set-Cookie: "
    "session_token=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
    "eyJzdWIiOiJ1c3IxMjM0NTY3ODkwIiwibmFtZSI6IlRlc3QgVXNlciIsImlhdCI6MTcwODQyND"
    "I0MCwiZXhwIjoxNzA4NTEwNjQwfQ; Path=/; HttpOnly; Secure; "
    "SameSite=Strict\r\n"
    "Content-Security-Policy: default-src 'self'; script-src 'self' "
    "'unsafe-inline' https://cdn.example.test; style-src 'self' "
    "'unsafe-inline'; img-src 'self' data: https:; font-src 'self'; "
    "connect-src 'self' https://api.example.test; frame-ancestors 'none'\r\n"
    "\r\n";

/* Very long values (~250-520B each) ~1940B */
static unsigned char RSP_VAL_XLONG[] =
    "HTTP/1.1 200 OK\r\n"
    "Date: Thu, 20 Feb 2026 09:00:00 GMT\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Content-Length: 32768\r\n"
    "Connection: keep-alive\r\n"
    "Server: cloudflare\r\n"
    "Permissions-Policy: accelerometer=(), camera=(), geolocation=(), "
    "gyroscope=(), magnetometer=(), microphone=(), payment=(), usb=()\r\n"
    "Content-Security-Policy: default-src 'self' https://cdn.example.test; "
    "script-src 'self' 'unsafe-inline' 'unsafe-eval' https://cdn.example.test "
    "https://analytics.example.test; style-src 'self' 'unsafe-inline' "
    "https://fonts.googleapis.com https://cdn.example.test; img-src 'self' "
    "data: blob: https:; font-src 'self' https://fonts.gstatic.com; "
    "connect-src 'self' https://api.example.test wss://ws.example.test; "
    "media-src 'self' https://media.example.test; frame-ancestors 'none'; "
    "base-uri 'self'; form-action 'self'\r\n"
    "Set-Cookie: "
    "session_token=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
    "eyJzdWIiOiJ1c3IxMjM0NTY3ODkwIiwibmFtZSI6IlRlc3QgVXNlciIsInJvbGVzIjpbImFkbW"
    "luIiwidXNlciJdLCJpYXQiOjE3MDg0MjQyNDAsImV4cCI6MTcwODUxMDY0MCwianRpIjoiYWJj"
    "MTIzZGVmNDU2In0.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c; Path=/; "
    "HttpOnly; Secure; SameSite=Strict\r\n"
    "Set-Cookie: "
    "user_prefs=lang=en-US&theme=dark&timezone=Asia%2FTokyo&density="
    "comfortable&sidebar=expanded; Path=/; Max-Age=31536000; SameSite=Lax; "
    "Domain=example.test\r\n"
    "Set-Cookie: _ga_XXXXXXXX=GS1.1.1708424240.1.1.1708424895.60.0.0; Path=/; "
    "Expires=Sat, 21 Feb 2028 09:00:00 GMT; SameSite=Lax; "
    "Domain=.example.test\r\n"
    "Set-Cookie: _fbp=fb.1.1708424240.1234567890; Path=/; SameSite=Lax; "
    "Secure; Max-Age=7776000; Domain=.example.test\r\n"
    "Link: </static/css/app.abc123.css>; rel=preload; as=style, "
    "</static/css/critical.def456.css>; rel=preload; as=style, "
    "</static/js/runtime.ghi789.js>; rel=preload; as=script, "
    "</static/js/vendor.jkl012.js>; rel=preload; as=script, "
    "</static/js/app.mno345.js>; rel=preload; as=script, "
    "</static/fonts/roboto.pqr678.woff2>; rel=preload; as=font; crossorigin\r\n"
    "Strict-Transport-Security: max-age=31536000; includeSubDomains; "
    "preload\r\n"
    "\r\n";

/* ============================================================================
 * Category 3: Case Sensitivity
 * Purpose: Measure parser behavior with all-lowercase vs mixed-case headers
 * Control: Fixed 8 headers (~330B), varying header name case
 * ============================================================================
 */

/* All lowercase header names (~330B) */
static unsigned char RSP_CASE_LOWER[] =
    "HTTP/1.1 200 OK\r\n"
    "date: Thu, 20 Feb 2026 09:00:00 GMT\r\n"
    "content-type: text/html; charset=utf-8\r\n"
    "content-length: 4096\r\n"
    "connection: keep-alive\r\n"
    "server: nginx/1.25.3\r\n"
    "cache-control: no-cache, no-store, must-revalidate\r\n"
    "etag: \"abc123def456\"\r\n"
    "vary: accept-encoding\r\n"
    "\r\n";

/* Mixed case header names (~330B) */
static unsigned char RSP_CASE_MIXED[] =
    "HTTP/1.1 200 OK\r\n"
    "Date: Thu, 20 Feb 2026 09:00:00 GMT\r\n"
    "CONTENT-TYPE: text/html; charset=utf-8\r\n"
    "Content-Length: 4096\r\n"
    "CONNECTION: keep-alive\r\n"
    "Server: nginx/1.25.3\r\n"
    "CACHE-CONTROL: no-cache, no-store, must-revalidate\r\n"
    "ETag: \"abc123def456\"\r\n"
    "VARY: accept-encoding\r\n"
    "\r\n";

/* ============================================================================
 * Category 4: Real-World Scenarios
 * Purpose: Measure parser with realistic production-like responses
 * Reference: Common patterns from browsers, APIs, and CDN-proxied content
 * ============================================================================
 */

/* Modern browser HTML page through Cloudflare CDN (20 headers) ~900B */
static unsigned char RSP_REAL_HTML[] =
    "HTTP/1.1 200 OK\r\n"
    "Date: Thu, 20 Feb 2026 09:00:00 GMT\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Transfer-Encoding: chunked\r\n"
    "Connection: keep-alive\r\n"
    "Server: cloudflare\r\n"
    "Cache-Control: no-store, no-cache\r\n"
    "Vary: Accept-Encoding\r\n"
    "Content-Encoding: gzip\r\n"
    "X-Frame-Options: SAMEORIGIN\r\n"
    "X-Content-Type-Options: nosniff\r\n"
    "Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n"
    "Referrer-Policy: strict-origin-when-cross-origin\r\n"
    "Content-Security-Policy: default-src 'self'; script-src 'self' "
    "'unsafe-inline' https://cdn.example.test; style-src 'self' "
    "'unsafe-inline' https://fonts.googleapis.com; img-src 'self' data: "
    "https:; font-src 'self' https://fonts.gstatic.com; connect-src 'self' "
    "https://api.example.test; frame-ancestors 'none'\r\n"
    "Set-Cookie: session=abc123xyz789; Path=/; HttpOnly; Secure; "
    "SameSite=Lax\r\n"
    "CF-Ray: 1234567890abcdef-NRT\r\n"
    "CF-Cache-Status: DYNAMIC\r\n"
    "Age: 0\r\n"
    "Via: 1.1 cloudflare\r\n"
    "X-Request-Id: req-1234567890abcdef\r\n"
    "\r\n";

/* REST API JSON response (10 headers) ~370B */
static unsigned char RSP_REAL_API[] =
    "HTTP/1.1 200 OK\r\n"
    "Date: Thu, 20 Feb 2026 09:00:00 GMT\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
    "Content-Length: 1024\r\n"
    "Connection: keep-alive\r\n"
    "Server: nginx/1.25.3\r\n"
    "Cache-Control: no-cache, no-store, must-revalidate\r\n"
    "Access-Control-Allow-Origin: https://app.example.test\r\n"
    "X-RateLimit-Limit: 1000\r\n"
    "X-RateLimit-Remaining: 998\r\n"
    "X-Request-Id: req-api-1234567890ab\r\n"
    "\r\n";

/* Static asset (JS) through CDN with long-term caching (15 headers) ~490B */
static unsigned char RSP_REAL_STATIC[] =
    "HTTP/1.1 200 OK\r\n"
    "Date: Thu, 20 Feb 2026 09:00:00 GMT\r\n"
    "Content-Type: application/javascript; charset=utf-8\r\n"
    "Content-Length: 102400\r\n"
    "Content-Encoding: br\r\n"
    "Connection: keep-alive\r\n"
    "Server: cloudflare\r\n"
    "Cache-Control: public, max-age=31536000, immutable\r\n"
    "ETag: \"sha256-abc123def456789012345678\"\r\n"
    "Last-Modified: Mon, 01 Jan 2026 00:00:00 GMT\r\n"
    "Vary: Accept-Encoding\r\n"
    "CF-Ray: fedcba9876543210-NRT\r\n"
    "CF-Cache-Status: HIT\r\n"
    "Age: 43200\r\n"
    "Cross-Origin-Resource-Policy: same-origin\r\n"
    "Via: 1.1 cloudflare\r\n"
    "\r\n";

/* ============================================================================
 * Category 5: Baseline
 * Purpose: Measure parser overhead with minimal input
 * ============================================================================
 */

/* No headers (status line + empty headers only) ~19B */
static unsigned char RSP_MINIMAL[] = "HTTP/1.1 200 OK\r\n"
                                     "\r\n";

/* One header (status + Date) ~57B */
static unsigned char RSP_MINIMAL_DATE[] =
    "HTTP/1.1 200 OK\r\n"
    "Date: Thu, 20 Feb 2026 09:00:00 GMT\r\n"
    "\r\n";
