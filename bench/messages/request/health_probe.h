/*
 * Minimal health-check probe: the high-RPS infrastructure request
 * (load balancers, k8s liveness/readiness, monitoring agents).
 */
#ifndef BENCH_MSG_HEALTH_PROBE
#define BENCH_MSG_HEALTH_PROBE

static const unsigned char MSG_HEALTH_PROBE[] =
    "GET /healthz HTTP/1.1\r\n"
    "Host: internal.example.com\r\n"
    "User-Agent: kube-probe/1.31\r\n"
    "Accept: */*\r\n"
    "\r\n";

#endif /* BENCH_MSG_HEALTH_PROBE */
