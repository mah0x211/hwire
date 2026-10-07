/* @title S3 API Request
 * @description Synthetic S3 PutObject header block with AWS Signature Version 4, checksum, encryption, tagging and user metadata fields. The upload body is excluded; credentials and signature are examples.
 */
#ifndef BENCH_S3_API_H
#define BENCH_S3_API_H
static const unsigned char MSG_S3_API[] =
    "PUT /reports/quarter%20one.bin HTTP/1.1\r\n"
    "Host: benchmark-bucket.s3.us-east-1.amazonaws.com\r\n"
    "Content-Length: 1024\r\n"
    "Content-Type: application/octet-stream\r\n"
    "Cache-Control: max-age=3600\r\n"
    "Content-Disposition: attachment; filename=\"quarter one.bin\"\r\n"
    "If-None-Match: *\r\n"
    "x-amz-date: 20261006T000000Z\r\n"
    "x-amz-content-sha256: 2edc986847e209b4016e141a6dc8716d3207350f416969382d431539bf292e4a\r\n"
    "x-amz-checksum-sha256: LtyYaEfiCbQBbhQabchxbTIHNQ9BaWk4LUMVOb8pLko=\r\n"
    "x-amz-storage-class: STANDARD\r\n"
    "x-amz-server-side-encryption: AES256\r\n"
    "x-amz-meta-project: hwire-benchmark\r\n"
    "x-amz-meta-owner: benchmark\r\n"
    "x-amz-tagging: project=hwire&environment=benchmark\r\n"
    "Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20261006/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-content-sha256;x-amz-date, Signature=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\r\n"
    "User-Agent: aws-sdk-example/1.0\r\n"
    "Accept-Encoding: identity\r\n"
    "Connection: keep-alive\r\n"
    "\r\n";
#endif
