#ifndef BLUETOE_EXAMPLES_UCONTROLLER_HPP
#define BLUETOE_EXAMPLES_UCONTROLLER_HPP

#if !defined BLUETOE_CONTROLLER
#   if defined BLUETOE_BOARD_PCA10056
#   elif defined BLUETOE_BOARD_PCA10040
#   else
#       error "can't deduce used microcontroller from board. Please define BLUETOE_CONTROLLER"
#   endif
#endif // include guard
