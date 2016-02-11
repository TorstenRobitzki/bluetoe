#ifndef BLUETOE_SERVICES_BOOTLOADER_HPP
#define BLUETOE_SERVICES_BOOTLOADER_HPP

#include <bluetoe/service.hpp>

namespace bluetoe
{
    namespace bootloader {
        using service_uuid       = bluetoe::service_uuid< 0x7D295F4D, 0x2850, 0x4F57, 0xB595, 0x837F5753F8A9 >;
        using control_point_uuid = bluetoe::characteristic_uuid< 0x7D295F4D, 0x2850, 0x4F57, 0xB595, 0x837F5753F8A9 >;

        namespace details {

            template < typename UserHandler >
            struct handler : UserHandler
            {
                std::pair< std::uint8_t, bool > bootloader_write_control_point( std::size_t write_size, const std::uint8_t* value )
                {
                    return std::pair< std::uint8_t, bool >{};
                }

                std::uint8_t bootloader_read_control_point( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
                {
                    return 0;
                }
            };

            template < typename ... Options >
            struct calculate_service {
                using implementation = handler< bluetoe::details::no_such_type >;

                using type = bluetoe::service<
                    bluetoe::bootloader::service_uuid,
                    bluetoe::characteristic<
                        bluetoe::bootloader::control_point_uuid,
                        bluetoe::mixin_write_notification_control_point_handler<
                            implementation, &implementation::bootloader_write_control_point, bluetoe::bootloader::control_point_uuid
                        >,
                        bluetoe::mixin_read_handler<
                            implementation, &implementation::bootloader_read_control_point
                        >,
                        bluetoe::no_read_access,
                        bluetoe::notify
                    >
                >;
            };
        }

        enum error_codes : std::uint8_t {};
    }

    template < typename ... Options >
    struct bootloader_service : bootloader::details::calculate_service< Options... >::type {};

    /**
     * @brief Prototype for a handler, that adapts the bootloader service to the actual hardware
     */
    class bootloader_handler_prototype
    {
    public:
        /*
         * Start to flash
         */
        bootloader::error_codes start_flash( const std::uint8_t* target, std::uint8_t* destination, std::size_t size );
        bootloader::error_codes start_copy( const std::uint8_t* target, std::uint8_t* destination, std::size_t size );

        bootloader::error_codes reset();

        bootloader::error_codes run( const std::uint8_t* start_addr );
    };
}

#endif