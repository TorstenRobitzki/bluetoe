# The support every Bluetoe firmware is built on. A firmware project includes
# cmake/firmware_project.cmake before project() and cmake/firmware.cmake after it, which adds
# this directory; see README.md.
add_subdirectory(assert)
add_subdirectory(runtime)
add_subdirectory(${BINDING})
