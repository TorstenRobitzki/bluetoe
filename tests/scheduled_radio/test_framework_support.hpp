#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_TEST_FRAMEWORK_SUPPORT_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_TEST_FRAMEWORK_SUPPORT_HPP

#include <iosfwd>

namespace Catch {
    std::ostream& cout();
    std::ostream& cerr();
    std::ostream& clog();
}

#endif