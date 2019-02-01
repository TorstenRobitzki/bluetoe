#ifndef BLUETOE_TESTS_SECURITY_MANAGER_TEST_SM_HPP
#define BLUETOE_TESTS_SECURITY_MANAGER_TEST_SM_HPP

namespace test {

    template < class Manager, std::size_t MTU = 27 >
    struct security_manager : Manager
    {
        void expected(
            std::initializer_list< std::uint8_t > input,
            std::initializer_list< std::uint8_t > expected_output )
        {
            std::uint8_t buffer[ MTU ];
            std::size_t  size = MTU;

            this->l2cap_input( input.begin(), input.size(), &buffer[ 0 ], size );

            BOOST_CHECK_EQUAL_COLLECTIONS(
                expected_output.begin(), expected_output.end(),
                &buffer[ 0 ], &buffer[ size ] );
        }
    };

}

#endif
