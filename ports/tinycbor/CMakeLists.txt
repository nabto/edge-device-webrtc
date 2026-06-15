cmake_minimum_required(VERSION 3.20)
project(tinycbor C)

file(GLOB sources src/cbor*.c)
list(FILTER sources EXCLUDE REGEX "cbortojson.c$")
add_library(tinycbor ${sources})

# uClibc's <assert.h> does not define the C11 static_assert macro; alias it to
# the compiler-builtin _Static_assert keyword so the calls in cborencoder.c /
# cborparser.c resolve at compile time instead of becoming undefined link-time
# references to a function named "static_assert".
target_compile_definitions(tinycbor PRIVATE static_assert=_Static_assert)

install(TARGETS tinycbor ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
install(FILES src/cbor.h src/cborjson.h src/tinycbor-version.h DESTINATION include/tinycbor)
