# Benchmark data

HTTP messages live under `request/` and `response/`. Each fixture defines
`static const unsigned char MSG_<NAME>[]`; the symbol is derived from the file
name. Names must be lowercase `[a-z][a-z0-9_]*`, as enforced by
`shared/scripts/gen_fixtures.py`. Reports use `req_` / `rsp_` prefixes for direction.

## Hashmap headers

`headers/http_headers.txt` is the supplied field-name corpus. It includes IANA
registrations and additional application/infrastructure fields, including legacy
names. It is not a traffic-frequency dataset.

`headers/headers_512.h` contains the committed pairs and separate lookup buffers.
Normal builds and measurements use it directly, without input generation.
To regenerate it after editing the corpus, run `python3 scripts/gen_headers.py` from
`bench/` (seed 42).

| Property | Value |
|---|---:|
| Pairs / configured capacity | 512 / 512 |
| Supplied distinct names | 452 |
| Synthetic distinct `X-` names | 44 |
| Additional duplicate pairs | 16 |
| Distinct names, exact / ASCII case-insensitive | 496 / 496 |
| Key length, minimum / maximum | 2 / 40 bytes |
| Mean key length | 15.96 bytes |

All supplied names retain their spelling. Add eight `Set-Cookie`, four
`WWW-Authenticate` and four `Link` pairs, each with a distinct synthetic value.
Fill the remainder with `X-` plus 6–22 random lowercase ASCII letters, rejecting
case-insensitive collisions. Shuffle the complete input with the same fixed
seed. Lookup hits use separate buffers; misses replace the first byte with `!`
and CI hits swap ASCII case. The hashmap driver deduplicates field names under
ASCII case folding, shuffles them with seed 42 and selects 32, 64, 128 or 256
unique keys without replacement. Every selected key receives equal lookup
frequency; the corpus's additional duplicate pairs are excluded from these
workloads. Values are synthetic identifiers; their content is not examined
in timed operations.

## HTTP message fixtures

Four synthetic HTTP/1.1 header blocks represent distinct workloads. The parser
suite stops at the header terminator; bodies are excluded. Inputs use CRLF line
endings, and report byte counts exclude the C string terminator. Displayed
messages use ordinary Markdown line breaks.

| Fixture | Scenario |
| --- | --- |
| `request/browser_get.h` | Desktop Chromium navigation, Cookie and four query parameters |
| `request/s3_api.h` | S3 PutObject with signing, checksum, tagging, encryption and metadata |
| `response/browser_response.h` | HTML response with validators, security policies and repeated fields |
| `response/no_content.h` | Minimal 204 acknowledgement |

The S3 example follows the [PutObject request syntax](https://docs.aws.amazon.com/AmazonS3/latest/API/API_PutObject.html).
It contains example credentials and a synthetic signature. Browser version,
cookie, token and identifier values are fixed examples, rather than a traffic capture.
Header names draw on the shared field-name corpus and the S3 API documentation.

`scenarios/browser_cdn.h` is the Production Usage Benchmark input: the browser
GET plus CF-Connecting-IP, X-Forwarded-For, X-Forwarded-Proto and Cf-Ray,
with Accept-Encoding changed to br, gzip. These follow the documented
[Cloudflare origin-request header behavior](https://developers.cloudflare.com/fundamentals/reference/http-headers/).
It is a constructed 21-field CDN-to-origin example, not a traffic capture.
Query text is consumed as part of the request target, without decomposition,
decoding or query storage. Artificial growth inputs are not measured.

Fixture comments provide `@title` and `@description` for the report. Messages
are displayed directly from their C string definitions, avoiding a separate
copy of the benchmark input.
