add_custom_target(flash_tests.run
   COMMAND mocha --compilers coffee:coffee-script/register ${CMAKE_CURRENT_LIST_DIR}/flash_tests.coffee)
