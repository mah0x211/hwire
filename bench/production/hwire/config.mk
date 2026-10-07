hwire_SOURCES := ../../src/hwire.c ../../src/hwire_table.c
hwire_NAME := hwire + hwire_table
ifeq ($(UNAME_M),x86_64)
hwire_VARIANTS := nosimd sse2 sse42 native siphash
else
hwire_VARIANTS := nosimd neon native siphash
endif
