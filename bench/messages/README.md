# Benchmark message fixtures
#
# One header per fixture under a direction directory: `request/*.h`
# and `response/*.h`, each defining `static const unsigned char
# MSG_<NAME>[]`. The symbol is derived from the file name; the
# directory selects the direction (report tables show a req_/rsp_
# prefix). Names must be lowercase [a-z][a-z0-9_]* (enforced by
# gen_fixtures.py).
#
# The set was designed on 2026-09-04 against real-world captures of
# major public services (unauthenticated requests to public endpoints;
# summarized in the table below): the previous fixtures (a 2010-era
# browser request and hand-written responses) understated modern
# messages by 2-3x in header count and 15-25x in header bytes, and were
# discarded.

## Evidence snapshot (2026-09-04, unauthenticated captures)

| site | status | headers | header bytes | max value |
|---|---|---:|---:|---:|
| github.com (HTML) | 200 | 19 | 5200 | 3766 |
| api.github.com (JSON) | 200 | 25 | 1264 | 292 |
| aws.amazon.com | 200 | 16 | 815 | 197 |
| x.com | 200 | 32 | 4686 | 2667 |
| www.facebook.com | 400 | 22 | 5337 | 2302 |
| www.tiktok.com | 200 | 25 | 6699 | 4003 |
| httpbin.org (control) | 200 | 6 | 187 | 29 |

Modern responses carry 16-32 headers (800-6700 bytes), dominated by
security headers (CSP, HSTS, permissions), cookie clusters, and
CDN/rate-limit header families. Modern requests add the Sec-Fetch /
Sec-CH-UA families; logged-in browsers carry multi-KB cookies.

## Fixtures

Requests (client scenarios, messages/request/):

- `browser_page_load.h` — a browser loading a page (Sec-*, client hints)
- `browser_logged_in.h` — a logged-in browser revisiting the site
  (heavy Cookie; github/facebook shaped, synthetic cookie content)
- `api_call.h` — authenticated API call (bearer token, request/trace ids)
- `mobile_app_feed.h` — mobile app fetching a feed (device/telemetry
  headers)
- `health_probe.h` — infrastructure health probe (high RPS, minimal)

Responses (server scenarios, messages/response/):

- `html_page.h` — web page with the security-header cluster (github
  shaped)
- `html_set_cookies.h` — response establishing sessions: a heavy
  Set-Cookie cluster (facebook/tiktok shaped, synthetic cookies)
- `api_json.h` — JSON API with platform headers (api.github.com shaped:
  rate limits, request ids, CORS)
- `cdn_asset.h` — CDN-cached static asset (age/x-cache/via edge cluster)
- `empty_204.h` — 204 No Content (probe/beacon reply)

Cookie, token, and identifier values are synthetic shapes, never real
credentials.
