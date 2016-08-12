add_custom_target(flash_tests.run
   COMMAND mocha --compilers coffee:coffee-script/register ${CMAKE_CURRENT_LIST_DIR}/flash_tests.coffee)

add_custom_target(crc_tests.run
   COMMAND mocha --compilers coffee:coffee-script/register ${CMAKE_CURRENT_LIST_DIR}/crc_tests.coffee)
