# The scheduled radio 2 work is not bound to C++11 (README.md). Its targets set their own
# standard rather than inheriting the C++11 pin the continuous integration build applies to the
# rest of the library.
add_subdirectory(link)
add_subdirectory(host)
add_subdirectory(test_tools)
add_subdirectory(self_tests)
add_subdirectory(radio_tests)
