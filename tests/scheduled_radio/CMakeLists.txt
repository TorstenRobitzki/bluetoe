# The scheduled radio 2 work is not bound to C++11 (documentation/scheduled_radio_test_rig.md,
# decision 13). Its targets set their own standard rather than inheriting the C++11 pin the
# continuous integration build applies to the rest of the library.
add_subdirectory(link)
add_subdirectory(host)

# The instruments testing themselves; the tests of a scheduled radio implementation live in
# radio_tests/ and are not run by ctest (decision 21).
add_subdirectory(self_tests)
