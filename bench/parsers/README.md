# Parser registrations for the benchmark suite
#
# Each subdirectory is one parser registration: the parser's C sources
# and headers plus the two uniform adapters request.c and response.c
# implementing
#
#     int <name>_request(const unsigned char *data, size_t len);   /* 0 = ok */
#     int <name>_response(const unsigned char *data, size_t len);  /* 0 = ok */
#
# Every *.c in a registered directory is compiled (C99) into the
# benchmark binary. <name> must be usable as a C identifier — first
# character a letter, the rest letters, digits, or underscores — because
# the adapters are compiled as <name>_request/<name>_response; other
# names are skipped with a warning (prefixing a directory name with "_"
# disables it without deleting it). Upstream files are kept unmodified;
# the adapters are ours. Record the provenance (version/URL) of every
# added parser below.

## hwire
- Source: this repository (../src; always compiled from the current
  sources, never copied here)
- Ours: request.c, response.c (adapters)
