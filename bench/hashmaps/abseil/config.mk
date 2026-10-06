abseil_CPPFLAGS := -Iabseil/deps
abseil_SOURCES := \
    abseil/deps/absl/base/throw_delegate.cc \
    abseil/deps/absl/base/internal/raw_logging.cc \
    abseil/deps/absl/container/internal/raw_hash_set.cc \
    abseil/deps/absl/container/internal/hashtablez_sampler_force_weak_definition.cc \
    abseil/deps/absl/container/internal/hashtablez_sampler.cc \
    abseil/deps/absl/hash/internal/hash.cc \
    abseil/deps/absl/hash/internal/city.cc
