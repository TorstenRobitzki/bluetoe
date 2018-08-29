#ifndef BLUETOE_SLAVE_CONNECTION_INTERVAL_RANGE_HPP
#define BLUETOE_SLAVE_CONNECTION_INTERVAL_RANGE_HPP

namespace bluetoe {
    static constexpr std::uint16_t no_specific_slave_connection_minimum_interval = 0xFFFF;
    static constexpr std::uint16_t no_specific_slave_connection_maximum_interval = 0xFFFF;

    namespace details {
        struct slave_connection_interval_range_meta_type {};

        template <
            std::uint16_t MinInterval,
            std::uint16_t MaxInterval >
        struct check_slave_connection_interval_range_parameters
        {
            static_assert( ( MinInterval >= 0x0006 && MinInterval <= 0x0C80 ) || MinInterval == no_specific_slave_connection_minimum_interval,
                "Invalid value for slave_connection_interval_range first template parameter (MinInterval) used!" );

            static_assert( ( MaxInterval >= 0x0006 && MaxInterval <= 0x0C80 ) || MaxInterval == no_specific_slave_connection_maximum_interval,
                "Invalid value for slave_connection_interval_range second template parameter (MaxInterval) used!" );

            static_assert( MinInterval <= MaxInterval || MinInterval == no_specific_slave_connection_minimum_interval || MaxInterval == no_specific_slave_connection_maximum_interval,
                "when defining the slave_connection_interval_range, the minimum shall not be greater than the maximum value!" );

            typedef void type;
        };

        struct no_slave_connection_interval_range
        {
            typedef slave_connection_interval_range_meta_type meta_type;

            static std::uint8_t* advertising_data( std::uint8_t* begin, std::uint8_t* )
            {
                return begin;
            }
        };
    }

    /**
     * @brief add slave connection interval range to advertising data
     *
     * Adds \<\<Slave Connection Interval Range\>\> AD type to the advertising data.
     * MinInterval and MaxInterval must be within the range of 0x0006 to 0x0C80.
     * Using no_specific_slave_connection_minimum_interval or no_specific_slave_connection_maximum_interval
     * specifies, that the respective value is not used.
     */
    template <
        std::uint16_t MinInterval = no_specific_slave_connection_minimum_interval,
        std::uint16_t MaxInterval = no_specific_slave_connection_maximum_interval,
        typename = typename details::check_slave_connection_interval_range_parameters< MinInterval, MaxInterval >::type
    >
    struct slave_connection_interval_range
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::slave_connection_interval_range_meta_type,
            details::valid_server_option_meta_type {};

        static std::uint8_t* advertising_data( std::uint8_t* begin, std::uint8_t* end )
        {
            if ( end - begin >= 6 )
            {
                *begin = 0x05;
                ++begin;
                *begin = 0x12;
                ++begin;

                begin = details::write_16bit( begin, MinInterval );
                begin = details::write_16bit( begin, MaxInterval );
            }

            return begin;
        }

        /** @endcond */
    };
}

#endif
