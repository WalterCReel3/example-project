# Test suite.
#
# wreel_add_test() (cmake/Testing.cmake) is the modern form of the original
# `test_module` macro, and keeps both of its important properties: one
# executable per test, and the repository root as the working directory so
# data/ fixture paths resolve.

# --- Currently building ------------------------------------------------------
#
# These need no graphics context, so they run on every target — including under
# qemu-user-static for cross builds.

wreel_add_test(test_ascii   LIBS wreel::util)
wreel_add_test(test_file    LIBS wreel::util wreel::posix)
wreel_add_test(test_logging LIBS wreel::util wreel::posix)
wreel_add_test(test_json    LIBS wreel::util)
wreel_add_test(test_number  LIBS wreel::util)
wreel_add_test(test_string  LIBS wreel::util)
wreel_add_test(test_xml     LIBS wreel::util wreel::posix)
wreel_add_test(test_audio   LIBS wreel::audio)

# --- Priority queue ----------------------------------------------------------
#
# test_string.cc above is the important one and is not yet complete.
# include/util/string.hpp is 688 lines — a fifth of the codebase — and its
# std::ptr_fun usage must be removed for C++17. Its only consumer is
# loaders/obj.cc, so a broken tokenizer edge case shows up as malformed geometry
# rather than a compile error. Cover it *before* that refactor.
#
# Still to write:
#   test_obj      — parse data/cube.obj, ico.obj, teapot.obj; check counts
#   test_errors   — posix::wrap() errno dispatch across the generated types
#
# See docs/TARGETS.md § "Where the test surface actually is".
