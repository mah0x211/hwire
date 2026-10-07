/* @title No Content Response
 * @description Minimal 204 response representing a beacon or acknowledgement, with no body or Content-Length field.
 */
#ifndef BENCH_NO_CONTENT_H
#define BENCH_NO_CONTENT_H
static const unsigned char MSG_NO_CONTENT[] =
    "HTTP/1.1 204 No Content\r\n"
    "Date: Tue, 06 Oct 2026 00:00:00 GMT\r\n"
    "Server: example\r\n"
    "Connection: keep-alive\r\n"
    "\r\n";
#endif
