/*
 * CDN-served static asset response with the cache/edge-header cluster
 * (x.com / tiktok.com static shape: age, x-cache, via, edge ids).
 */
#ifndef BENCH_MSG_CDN_ASSET
#define BENCH_MSG_CDN_ASSET

static const unsigned char MSG_CDN_ASSET[] =
    "HTTP/1.1 200 OK\r\n"
    "Date: Thu, 04 Sep 2026 00:00:00 GMT\r\n"
    "Content-Type: image/jpeg\r\n"
    "Content-Length: 51200\r\n"
    "Connection: keep-alive\r\n"
    "Server: tls/1.0\r\n"
    "Cache-Control: public, max-age=31536000, immutable\r\n"
    "Age: 86400\r\n"
    "ETag: \"a1b2c3d4e5f6-7890\"\r\n"
    "Last-Modified: Wed, 02 Sep 2026 00:00:00 GMT\r\n"
    "Vary: Accept-Encoding\r\n"
    "x-cache: Hit from cloudfront\r\n"
    "x-amz-cf-id: SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c==\r\n"
    "x-amz-cf-pop: NRT57-C1\r\n"
    "Via: 1.1 8f14e45fceea167a5a36dedd4bea2543.cloudfront.net "
    "(CloudFront), 1.1 1122334455a6b7c8d9e0f.cloudfront.net (CloudFront)\r\n"
    "X-Served-By: cache-nrt-1234-NRT\r\n"
    "X-Cache: HIT\r\n"
    "X-Cache-Hits: 27\r\n"
    "X-Timer: S1762839604.123456,VS0,VE1\r\n"
    "Strict-Transport-Security: max-age=31536000; includeSubdomains\r\n"
    "\r\n";

#endif /* BENCH_MSG_CDN_ASSET */
