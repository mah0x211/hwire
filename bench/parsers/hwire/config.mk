hwire_SOURCES := ../../src/hwire.c
ifeq ($(UNAME_M),x86_64)
hwire_VARIANTS := nosimd sse2 sse42 native
else
hwire_VARIANTS := nosimd neon native
endif
