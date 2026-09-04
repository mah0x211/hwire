/*
 * Mobile application API request with device/telemetry headers.
 * (tiktok / social-app client shaped.)
 */
#ifndef BENCH_MSG_MOBILE_APP_FEED
#define BENCH_MSG_MOBILE_APP_FEED

static const unsigned char MSG_MOBILE_APP_FEED[] =
    "GET /feed?page=1&limit=20&filter=following HTTP/1.1\r\n"
    "Host: mobile.example.net\r\n"
    "Connection: keep-alive\r\n"
    "Authorization: Bearer token_9f8e7d6c5b4a3f2e1d0c9b8a7f6e5d4c\r\n"
    "Accept: application/json\r\n"
    "Accept-Encoding: gzip, br\r\n"
    "Accept-Language: ja-JP\r\n"
    "User-Agent: MyApp/3.0 (iPhone; iOS 19.0; Scale/3.00)\r\n"
    "X-Device-Id: ABCD-1234-EFGH-5678-IJKL-9012\r\n"
    "X-App-Version: 3.0.1-build.2026090301\r\n"
    "X-Carrier: 44010\r\n"
    "X-Network-Type: wifi\r\n"
    "X-Abtest-Group: feed_v9_b\r\n"
    "\r\n";

#endif /* BENCH_MSG_MOBILE_APP_FEED */
