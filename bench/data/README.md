# Benchmark data

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
