#ifndef BLUETOE_SERVER_HPP
#define BLUETOE_SERVER_HPP

#include <bluetoe/codes.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/bits.hpp>
#include <bluetoe/filter.hpp>
#include <bluetoe/server_name.hpp>
#include <bluetoe/adv_service_list.hpp>
#include <bluetoe/slave_connection_interval_range.hpp>
#include <bluetoe/server_meta_type.hpp>
#include <bluetoe/client_characteristic_configuration.hpp>
#include <bluetoe/write_queue.hpp>
#include <bluetoe/gap_service.hpp>
#include <bluetoe/appearance.hpp>
#include <bluetoe/mixin.hpp>
#include <bluetoe/find_notification_data.hpp>
#include <bluetoe/outgoing_priority.hpp>
#include <bluetoe/link_state.hpp>
#include <bluetoe/attribute_handle.hpp>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <iterator>
#include <cassert>

namespace bluetoe {
    /**
     * @brief Root of the declaration of a GATT server.
     *
     * The server serves one or more services configured by the given Options. To configure the server, pass one or more bluetoe::service types as parameters.
     *
     * example:
     * @code
    unsigned temperature_value = 0;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
            bluetoe::characteristic<
                bluetoe::bind_characteristic_value< decltype( temperature_value ), &temperature_value >,
                bluetoe::no_write_access
            >
        >
    > small_temperature_service;
     * @endcode
     * @sa service
     * @sa shared_write_queue
     * @sa extend_server
     * @sa server_name
     * @sa appearance
     * @sa requires_encryption
     */
    template < typename ... Options >
    class server : private details::write_queue< typename details::find_by_meta_type< details::write_queue_meta_type, Options... >::type >,
        public details::derive_from< typename details::collect_mixins< Options... >::type >
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        using services_without_gap = typename details::find_all_by_meta_type< details::service_meta_type, Options... >::type;

        // append gap serivce for gatt servers
        using gap_service_definition = typename details::find_by_meta_type< details::gap_service_definition_meta_type,
            Options..., gap_service_for_gatt_servers >::type;
        using services = typename gap_service_definition::template add_service< services_without_gap, Options... >::type;

        static constexpr std::size_t number_of_client_configs = details::sum_by< services, details::sum_by_client_configs >::value;

        using write_queue_type = typename details::find_by_meta_type< details::write_queue_meta_type, Options... >::type;

        using notification_priority = typename details::find_by_meta_type< details::outgoing_priority_meta_type, Options..., higher_outgoing_priority<> >::type;

        static_assert( std::tuple_size< typename details::find_all_by_meta_type< details::outgoing_priority_meta_type, Options... >::type >::value <= 1,
            "Only one of bluetoe::higher_outgoing_priority<> or bluetoe::lower_outgoing_priority<> per server allowed!" );

        using cccd_indices = typename details::find_notification_data_in_list< notification_priority, services >::cccd_indices;

        using handle_mapping = details::handle_index_mapping< server< Options... > >;

        /** @endcond */

        /**
         * @brief per connection data
         *
         * The underlying layer have to provide the memory for a connection and pass the connection_data to l2cap_input().
         * The purpose of this class is to store all connection related data that must be keept per connection and must
         * be reset with a new connection.
         */
        using connection_data = details::link_state<
            details::client_characteristic_configurations< number_of_client_configs > >;

        /**
         * @brief a server takes no runtime construction parameters
         */
        server();

        /**
         * @brief notifies all connected clients about this value
         *
         * There is no check whether there was actual a change to the value or not. It's safe to call this function from a different
         * thread or from an interrupt service routine. But there is a check whether or not clients enabled notifications.
         *
         * The characteristic<> must have been given the notify parameter.
         *
         * @sa notify
         * @sa characteristic
         *
         * Example:
         @code
        std::int32_t temperature;

        typedef bluetoe::server<
            bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
            bluetoe::characteristic<
                bluetoe::bind_characteristic_value< decltype( temperature ), &temperature >,
                bluetoe::notify
            >
        > temperature_service;

        int main()
        {
            temperature_service server;

            server.notify( temperature );
        }
        @endcode

         * @return The function will return false, if the given notification was ignored, because the
         *         characteristic is already queued for notification, but not yet send out.
         */
        template < class T >
        bool notify( const T& value );

        /**
         * Notify a characteristic, by giving the characteristic UUID.
         *
         * The charactieristic to be notify, must have been configured for notificaton.
         * If multiple characteristics exists with the given UUID, the first characteristic will be notified.
         *
         * @sa notify
         * @sa characteristic
         *
         * Example:
         @code
        std::int32_t temperature;

        typedef bluetoe::server<
            bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x0815 >,
                bluetoe::bind_characteristic_value< decltype( temperature ), &temperature >,
                bluetoe::notify
            >
        > temperature_service;

        int main()
        {
            temperature_service server;

            server.notify< bluetoe::characteristic_uuid16< 0x0815 > >();
        }
        @endcode

         * @return The function will return false, if the given notification was ignored, because the
         *         characteristic is already queued for notification, but not yet send out.
         */
        template < class CharacteristicUUID >
        bool notify();

        /**
         * @brief sends indications to all connceted clients.
         *
         * The function is mostly similar to notify(). Instead of an ATT notification, an ATT indication is send.
         *
         * @return The function will return false, if the given indication was ignored, because the
         *         characteristic is already queued for indication, but not yet send out or the confirmation
         *         to a previous send out indication of this characteristic was not received yet.
         */
        template < class T >
        bool indicate( const T& value );

        /**
         * @brief sends indications to all connceted clients.
         *
         * The function is mostly similar to notify(). Instead of an ATT notification, an ATT indication is send.
         *
         * @return The function will return false, if the given indication was ignored, because the
         *         characteristic is already queued for indication, but not yet send out or the confirmation
         *         to a previous send out indication of this characteristic was not received yet.
         */
        template < class CharacteristicUUID >
        bool indicate();

        /**
         * @brief returns true, if the given connection is configured to send indications for the given characteristic
         */
        template < class CharacteristicUUID >
        bool configured_for_indications( const details::client_characteristic_configuration& ) const;

        /**
         * @brief returns true, if the given connection is configured to send notifications for the given characteristic
         */
        template < class CharacteristicUUID >
        bool configured_for_notifications( const details::client_characteristic_configuration& ) const;

        /**
         * @brief returns true, if the given connection is configured to send indications or notifications for the given characteristic
         */
        template < class CharacteristicUUID >
        bool configured_for_notifications_or_indications( const details::client_characteristic_configuration& ) const;

        /** @cond HIDDEN_SYMBOLS */
        // function relevant only for l2cap layers
        /**
         * @brief function to be called by a L2CAP implementation to provide the input from the L2CAP layer and the data assiziate with the connection
         */
        template < typename ConnectionData >
        void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& );

        /**
         * @brief returns the advertising data to the L2CAP implementation
         */
        std::size_t advertising_data( std::uint8_t* buffer, std::size_t buffer_size ) const;


        enum notification_type {
            notification,
            indication,
            confirmation
        };

        typedef bool (*lcap_notification_callback_t)( const details::notification_data& item, void* usr_arg, notification_type type );

        /**
         * @brief sets the callback for the l2cap layer to receive notifications and indications
         *
         * The server will inform the l2cap layer about that fact, that there are outstanding notifications for all connection for the
         * characteristic given by the item pointer. It's up to the l2cap layer to call notification_output.
         * The usr_arg is stored and passed to the given callback, when the callback is called.
         */
        void notification_callback( lcap_notification_callback_t, void* usr_arg );

        void notification_output( std::uint8_t* output, std::size_t& out_size, connection_data&, const details::notification_data& data );
        void notification_output( std::uint8_t* output, std::size_t& out_size, connection_data& connection, std::size_t client_characteristic_configuration_index );

        void indication_output( std::uint8_t* output, std::size_t& out_size, connection_data& connection, std::size_t client_characteristic_configuration_index );

        /**
         * @attention this function must be called with every client that got disconnected.
         */
        void client_disconnected( connection_data& );

        typedef details::server_meta_type meta_type;

        static details::attribute attribute_at( std::size_t index );
        /** @endcond */

    private:
        static constexpr std::size_t number_of_attributes       = details::sum_by< services, details::sum_by_attributes >::value;

        static_assert( std::tuple_size< services >::value > 0, "A server should at least contain one service." );

        void error_response( std::uint8_t opcode, details::att_error_codes error_code, std::uint16_t handle, std::uint8_t* output, std::size_t& out_size );
        void error_response( std::uint8_t opcode, details::att_error_codes error_code, std::uint8_t* output, std::size_t& out_size );

        static details::att_error_codes access_result_to_att_code( details::attribute_access_result, details::att_error_codes default_att_code );

        /**
         * for a PDU what starts with an opcode, followed by a pair of handles, the function checks the size of the PDU (must be A or B) and checks the handles.
         * The starting handle must not be 0, must be greate than ending_handle and must be with in the range of attributes available.
         * If the function returns true, everything is fine and starting_handle and ending_handle are filled correctly. Otherwise, an error response was generated.
         */
        template < std::size_t A, std::size_t B = A >
        bool check_size_and_handle_range( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, std::uint16_t& starting_handle, std::uint16_t& ending_handle );

        template < std::size_t A, std::size_t B = A >
        bool check_size_and_handle( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, std::uint16_t& handle );
        bool check_handle( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, std::uint16_t& handle );

        template < typename ConnectionData >
        void handle_exchange_mtu_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& );
        void handle_find_information_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );
        void handle_find_by_type_value_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );
        template < typename ConnectionData >
        void handle_read_by_type_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& );
        template < typename ConnectionData >
        void handle_read_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& );
        template < typename ConnectionData >
        void handle_read_blob_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& );
        void handle_read_by_group_type_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );
        template < typename ConnectionData >
        void handle_read_multiple_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& );
        template < typename ConnectionData >
        void handle_write_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& );
        template < typename ConnectionData >
        void handle_write_command( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& );

        void handle_prepair_write_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data&, const details::no_such_type& );
        template < typename WriteQueue >
        void handle_prepair_write_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data&, const WriteQueue& );
        void handle_execute_write_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data&, const details::no_such_type& );
        template < typename WriteQueue >
        void handle_execute_write_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data&, const WriteQueue& );
        void handle_value_confirmation( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data& );

        template < class Iterator, class Filter = details::all_uuid_filter >
        void all_attributes( std::uint16_t starting_handle, std::uint16_t ending_handle, Iterator&, const Filter& filter = details::all_uuid_filter() );

        template < class Iterator, class Filter = details::all_uuid_filter >
        bool all_services_by_group( std::uint16_t starting_handle, std::uint16_t ending_handle, Iterator&, const Filter& filter = details::all_uuid_filter() );

        std::uint8_t* collect_handle_uuid_tuples( std::uint16_t start, std::uint16_t end, bool only_16_bit, std::uint8_t* output, std::uint8_t* output_end );

        static void write_128bit_uuid( std::uint8_t* out, const details::attribute& char_declaration );

        // mapping of a last handle to a valid attribute index
        std::size_t last_handle_index( std::uint16_t ending_handle );

        // data
        lcap_notification_callback_t l2cap_cb_;
        void*                        l2cap_arg_;

        static_assert(
            std::is_same<
                typename details::find_by_not_meta_type<
                    details::valid_server_option_meta_type,
                    Options...
                >::type, details::no_such_type
            >::value, "Option passed to a server that is not a valid server option." );
    protected: // for testing
        /** @cond HIDDEN_SYMBOLS */

        details::notification_data find_notification_data( const void* ) const;
        details::notification_data find_notification_data_by_index( std::size_t client_characteristic_configuration_index ) const;

        /** @endcond */
    };

    /**
     * @example server_example.cpp
     * This is a very small sample showing, how to define a very small GATT server.
     */

    /**
     * @brief adds additional options to a given server definition
     *
     * example:
     * @code
    unsigned temperature_value = 0;

    typedef bluetoe::server<
    ...
    > small_temperature_server;

    typedef bluetoe::extend_server<
        small_temperature_server,
        bluetoe::server_name< name >
    > small_named_temperature_service;

     * @endcode
     * @sa server
     */
    template < typename Server, typename ... Options >
    struct extend_server;

    template < typename ... ServerOptions, typename ... Options >
    struct extend_server< server< ServerOptions... >, Options... > : server< ServerOptions..., Options... >
    {
    };

    /*
     * Implementation
     */
    /** @cond HIDDEN_SYMBOLS */
    template < typename ... Options >
    server< Options... >::server()
        : l2cap_cb_( nullptr )
    {

    }

    template < typename ... Options >
    template < typename ConnectionData >
    void server< Options... >::l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& connection )
    {
        // clip the output size to the negotiated mtu
        out_size = std::min< std::size_t >( out_size, connection.negotiated_mtu() );

        assert( in_size != 0 );
        assert( out_size >= details::default_att_mtu_size );

        const details::att_opcodes opcode = static_cast< details::att_opcodes >( input[ 0 ] );

        switch ( opcode )
        {
        case details::att_opcodes::exchange_mtu_request:
            handle_exchange_mtu_request( input, in_size, output, out_size, connection );
            break;
        case details::att_opcodes::find_information_request:
            handle_find_information_request( input, in_size, output, out_size );
            break;
        case details::att_opcodes::find_by_type_value_request:
            handle_find_by_type_value_request( input, in_size, output, out_size );
            break;
        case details::att_opcodes::read_by_type_request:
            handle_read_by_type_request( input, in_size, output, out_size, connection );
            break;
        case details::att_opcodes::read_request:
            handle_read_request( input, in_size, output, out_size, connection );
            break;
        case details::att_opcodes::read_blob_request:
            handle_read_blob_request( input, in_size, output, out_size, connection );
            break;
        case details::att_opcodes::read_by_group_type_request:
            handle_read_by_group_type_request( input, in_size, output, out_size );
            break;
        case details::att_opcodes::read_multiple_request:
            handle_read_multiple_request( input, in_size, output, out_size, connection );
            break;
        case details::att_opcodes::write_request:
            handle_write_request( input, in_size, output, out_size, connection );
            break;
        case details::att_opcodes::write_command:
            handle_write_command( input, in_size, output, out_size, connection );
            break;
        case details::att_opcodes::prepare_write_request:
            handle_prepair_write_request( input, in_size, output, out_size, connection, write_queue_type() );
            break;
        case details::att_opcodes::execute_write_request:
            handle_execute_write_request( input, in_size, output, out_size, connection, write_queue_type() );
            break;
        case details::att_opcodes::confirmation:
            handle_value_confirmation( input, in_size, output, out_size, connection );
            break;
        default:
            error_response( *input, details::att_error_codes::request_not_supported, output, out_size );
            break;
        }
    }

    namespace details {
        // all this hassel to stop gcc from complaining about constant argument to if
        template < bool >
        struct copy_name
        {
            static std::uint8_t* impl( std::uint8_t* begin, std::uint8_t* end, const char* const name )
            {
                if ( ( end - begin ) <= 2 )
                    return begin;

                const std::size_t name_length  = std::strlen( name );
                const std::size_t max_name_len = std::min< std::size_t >( name_length, end - begin - 2 );

                if ( name_length > 0 )
                {
                    begin[ 0 ] = max_name_len + 1;
                    begin[ 1 ] = max_name_len == name_length
                        ? bits( details::gap_types::complete_local_name )
                        : bits( details::gap_types::shortened_local_name );

                    std::copy( name + 0, name + max_name_len, &begin[ 2 ] );
                    begin += max_name_len + 2;
                }

                return begin;
            }
        };

        template <>
        struct copy_name< false >
        {
            static std::uint8_t* impl( std::uint8_t* begin, std::uint8_t*, const char* const )
            {
                return begin;
            }
        };

    }

    template < typename ... Options >
    std::size_t server< Options... >::advertising_data( std::uint8_t* begin, std::size_t buffer_size ) const
    {
        std::uint8_t* const end = begin + buffer_size;

        if ( buffer_size >= 3 )
        {
            begin[ 0 ] = 2;
            begin[ 1 ] = bits( details::gap_types::flags );
            // LE General Discoverable Mode | BR/EDR Not Supported
            begin[ 2 ] = 6;

            begin += 3;
        }

        typedef typename details::find_by_meta_type< details::server_name_meta_type, Options..., server_name< nullptr > >::type name;

        begin = details::copy_name< name::name != nullptr >::impl( begin, end, name::name );

        typedef typename details::find_by_meta_type<
            details::list_of_16_bit_service_uuids_tag,
            Options...,
            details::default_list_of_16_bit_service_uuids< services >
        >::type service_list_uuid16;

        begin = service_list_uuid16::advertising_data( begin, end );

        typedef typename details::find_by_meta_type<
            details::list_of_128_bit_service_uuids_tag,
            Options...,
            details::default_list_of_128_bit_service_uuids< services >
        >::type service_list_uuid128;

        begin = service_list_uuid128::advertising_data( begin, end );

        typedef typename details::find_by_meta_type<
            details::slave_connection_interval_range_meta_type,
            Options...,
            details::no_slave_connection_interval_range
        >::type slave_connection_interval_range_ad;

        begin = slave_connection_interval_range_ad::advertising_data( begin, end );

        // add aditional empty AD to be visible to Nordic sniffer
        if ( static_cast< unsigned >( end - begin ) >= 2u )
        {
            *begin = 0;
            ++begin;
            *begin = 0;
            ++begin;
        }

        return buffer_size - ( end - begin );
    }

    template < typename ... Options >
    template < class T >
    bool server< Options... >::notify( const T& value )
    {
        static_assert( number_of_client_configs != 0, "there is no characteristic that is configured for notification or indication" );

        const details::notification_data data = find_notification_data( &value );
        assert( data.valid() );

        if ( l2cap_cb_ )
            return l2cap_cb_( data, l2cap_arg_, notification );

        return false;
    }

    template < typename ... Options >
    template < class CharacteristicUUID >
    bool server< Options... >::notify()
    {
        static_assert( number_of_client_configs != 0, "there is no characteristic that is configured for notification or indication" );

        using characteristic = typename details::find_characteristic_data_by_uuid_in_service_list< services, CharacteristicUUID >::type;

        static_assert( !std::is_same< characteristic, details::no_such_type >::value, "Notified characteristic not found by UUID." );
        static_assert( characteristic::has_notification, "Characteristic must be configured for notification!" );

        const auto data = details::find_notification_by_uuid< notification_priority, services, typename characteristic::characteristic_t >::data();

        if ( l2cap_cb_ )
            return l2cap_cb_( data, l2cap_arg_, notification );

        return false;
    }

    template < typename ... Options >
    template < class T >
    bool server< Options... >::indicate( const T& value )
    {
        static_assert( number_of_client_configs != 0, "there is no characteristic that is configured for notification or indication" );

        const details::notification_data data = find_notification_data( &value );
        assert( data.valid() );

        if ( l2cap_cb_ )
            return l2cap_cb_( data, l2cap_arg_, indication );

        return false;
    }

    template < typename ... Options >
    template < class CharacteristicUUID >
    bool server< Options... >::indicate()
    {
        static_assert( number_of_client_configs != 0, "there is no characteristic that is configured for notification or indication" );

        using characteristic = typename details::find_characteristic_data_by_uuid_in_service_list< services, CharacteristicUUID >::type;

        static_assert( !std::is_same< characteristic, details::no_such_type >::value, "Indicated characteristic not found by UUID." );
        static_assert( characteristic::has_indication, "Characteristic must be configured for indication!" );

        const auto data = details::find_notification_by_uuid< notification_priority, services, typename characteristic::characteristic_t >::data();

        if ( l2cap_cb_ )
            return l2cap_cb_( data, l2cap_arg_, indication );

        return false;
    }

    template < typename ... Options >
    template < class CharacteristicUUID >
    bool server< Options... >::configured_for_indications( const details::client_characteristic_configuration& connection ) const
    {
        using characteristic = typename details::find_characteristic_data_by_uuid_in_service_list< services, CharacteristicUUID >::type;
        const auto data = details::find_notification_by_uuid< notification_priority, services, typename characteristic::characteristic_t >::data();

        return connection.flags( data.client_characteristic_configuration_index() ) & details::client_characteristic_configuration_indication_enabled;
    }

    template < typename ... Options >
    template < class CharacteristicUUID >
    bool server< Options... >::configured_for_notifications( const details::client_characteristic_configuration& connection ) const
    {
        using characteristic = typename details::find_characteristic_data_by_uuid_in_service_list< services, CharacteristicUUID >::type;
        const auto data = details::find_notification_by_uuid< notification_priority, services, typename characteristic::characteristic_t >::data();

        return connection.flags( data.client_characteristic_configuration_index() ) & details::client_characteristic_configuration_notification_enabled;
    }

    template < typename ... Options >
    template < class CharacteristicUUID >
    bool server< Options... >::configured_for_notifications_or_indications( const details::client_characteristic_configuration& connection ) const
    {
        using characteristic = typename details::find_characteristic_data_by_uuid_in_service_list< services, CharacteristicUUID >::type;
        const auto data = details::find_notification_by_uuid< notification_priority, services, typename characteristic::characteristic_t >::data();

        static const auto both = ( details::client_characteristic_configuration_notification_enabled | details::client_characteristic_configuration_indication_enabled );

        return connection.flags( data.client_characteristic_configuration_index() ) & both;
    }

    template < typename ... Options >
    void server< Options... >::notification_callback( lcap_notification_callback_t cb, void* usr_arg )
    {
        l2cap_cb_  = cb;
        l2cap_arg_ = usr_arg;
    }

    template < typename ... Options >
    void server< Options... >::notification_output( std::uint8_t* output, std::size_t& out_size, connection_data& connection, const details::notification_data& data )
    {
        assert( data.valid() );

        if ( connection.client_configurations().flags( data.client_characteristic_configuration_index() ) & details::client_characteristic_configuration_notification_enabled &&
             out_size >= 3 )
        {
            auto read = details::attribute_access_arguments::read( output + 3, output + out_size, 0, connection.client_configurations(), connection.security_attributes(), this );
            auto attr = attribute_at( data.handle() - 1 );
            auto rc   = attr.access( read, data.handle() );

            if ( rc == details::attribute_access_result::success )
            {
                *output = bits( details::att_opcodes::notification );
                details::write_handle( output +1, data.handle() );
            }

            out_size = 3 + read.buffer_size;
        }
        else
        {
            out_size = 0;
        }
    }

    template < typename ... Options >
    void server< Options... >::notification_output( std::uint8_t* output, std::size_t& out_size, connection_data& connection, std::size_t client_characteristic_configuration_index )
    {
        notification_output( output, out_size, connection, find_notification_data_by_index( client_characteristic_configuration_index ) );
    }

    template < typename ... Options >
    void server< Options... >::indication_output( std::uint8_t* output, std::size_t& out_size, connection_data& connection, std::size_t client_characteristic_configuration_index )
    {
        const auto details = find_notification_data_by_index( client_characteristic_configuration_index );
        assert( details.valid() );

        if ( connection.client_configurations().flags( details.client_characteristic_configuration_index() ) & details::client_characteristic_configuration_indication_enabled &&
             out_size >= 3 )
        {
            auto read = details::attribute_access_arguments::read( output + 3, output + out_size, 0, connection.client_configurations(), connection.security_attributes(), this );
            auto attr = attribute_at( details.handle() - 1 );
            auto rc   = attr.access( read, details.handle() );

            if ( rc == details::attribute_access_result::success )
            {
                *output = bits( details::att_opcodes::indication );
                details::write_handle( output +1, details.handle() );
            }

            out_size = 3 + read.buffer_size;
        }
        else
        {
            out_size = 0;
        }
    }

    template < typename ... Options >
    void server< Options... >::client_disconnected( connection_data& client )
    {
        this->free_write_queue( client );
    }

    template < typename ... Options >
    details::attribute server< Options... >::attribute_at( std::size_t index )
    {
        return details::attribute_from_service_list< services, server< Options... >, cccd_indices >::attribute_at( index );
    }

    template < typename ... Options >
    void server< Options... >::error_response( std::uint8_t opcode, details::att_error_codes error_code, std::uint16_t handle, std::uint8_t* output, std::size_t& out_size )
    {
        if ( out_size >= 5 )
        {
            output[ 0 ] = bits( details::att_opcodes::error_response );
            output[ 1 ] = opcode;
            output[ 2 ] = handle & 0xff;
            output[ 3 ] = handle >> 8;
            output[ 4 ] = bits( error_code );
            out_size = 5;
        }
        else
        {
            out_size = 0 ;
        }
    }

    template < typename ... Options >
    void server< Options... >::error_response( std::uint8_t opcode, details::att_error_codes error_code, std::uint8_t* output, std::size_t& out_size )
    {
        error_response( opcode, error_code, 0, output, out_size );
    }


    template < typename ... Options >
    details::att_error_codes server< Options... >::access_result_to_att_code( details::attribute_access_result access_code, details::att_error_codes default_att_code )
    {
        // if it can be copied lossles into a att_error_codes, it is a att_error_codes
        const details::att_error_codes result = static_cast< details::att_error_codes >( access_code );

        return static_cast< details::attribute_access_result >( result ) == access_code
            ? result
            : default_att_code;
    }

    template < typename ... Options >
    template < std::size_t A, std::size_t B >
    bool server< Options... >::check_size_and_handle_range(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, std::uint16_t& starting_handle, std::uint16_t& ending_handle )
    {
        if ( in_size != A && in_size != B )
        {
            error_response( *input, details::att_error_codes::invalid_pdu, output, out_size );
            return false;
        }

        starting_handle = details::read_handle( &input[ 1 ] );
        ending_handle   = details::read_handle( &input[ 3 ] );

        if ( starting_handle == 0 || starting_handle > ending_handle )
        {
            error_response( *input, details::att_error_codes::invalid_handle, starting_handle, output, out_size );
            return false;
        }

        if ( handle_mapping::first_index_by_handle( starting_handle ) == details::invalid_attribute_index )
        {
            error_response( *input, details::att_error_codes::attribute_not_found, starting_handle, output, out_size );
            return false;
        }

        return true;
    }

    template < typename ... Options >
    template < std::size_t A, std::size_t B >
    bool server< Options... >::check_size_and_handle( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, std::uint16_t& handle )
    {
        if ( in_size != A && in_size != B )
        {
            error_response( *input, details::att_error_codes::invalid_pdu, output, out_size );
            return false;
        }

        return check_handle( input, in_size, output, out_size, handle );
    }

    template < typename ... Options >
    bool server< Options... >::check_handle( const std::uint8_t* input, std::size_t, std::uint8_t* output, std::size_t& out_size, std::uint16_t& handle )
    {
        handle = details::read_handle( &input[ 1 ] );

        if ( handle == 0 )
        {
            error_response( *input, details::att_error_codes::invalid_handle, handle, output, out_size );
            return false;
        }

        if ( handle > number_of_attributes )
        {
            error_response( *input, details::att_error_codes::attribute_not_found, handle, output, out_size );
            return false;
        }

        return true;
    }

    template < typename ... Options >
    template < typename ConnectionData >
    void server< Options... >::handle_exchange_mtu_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& connection )
    {
        if ( in_size != 3 )
            return error_response( *input, details::att_error_codes::invalid_pdu, output, out_size );

        const std::uint16_t mtu = details::read_16bit( input + 1 );

        if ( mtu < details::default_att_mtu_size )
            return error_response( *input, details::att_error_codes::invalid_pdu, output, out_size );

        connection.client_mtu( mtu );

        *output = bits( details::att_opcodes::exchange_mtu_response );
        details::write_16bit( output + 1, connection.server_mtu() );

        out_size = 3u;
    }

    template < typename ... Options >
    void server< Options... >::handle_find_information_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        std::uint16_t starting_handle, ending_handle;

        if ( !check_size_and_handle_range< 5u >( input, in_size, output, out_size, starting_handle, ending_handle ) )
            return;

        const bool only_16_bit_uuids = attribute_at( starting_handle -1 ).uuid != bits( details::gatt_uuids::internal_128bit_uuid );

        std::uint8_t*        write_ptr = &output[ 0 ];
        std::uint8_t* const  write_end = write_ptr + out_size;

        *write_ptr = bits( details::att_opcodes::find_information_response );
        ++write_ptr;

        if ( write_ptr != write_end )
        {
            *write_ptr = bits(
                only_16_bit_uuids
                    ? details::att_uuid_format::short_16bit
                    : details::att_uuid_format::long_128bit );

            ++write_ptr;

        }

        write_ptr = collect_handle_uuid_tuples( starting_handle, ending_handle, only_16_bit_uuids, write_ptr, write_end );

        out_size = write_ptr - &output[ 0 ];
    }

    namespace details {
        template < class Server >
        struct value_filter
        {
            value_filter( const std::uint8_t* begin, const std::uint8_t* end, Server& s )
                : begin_( begin )
                , end_( end )
                , server_( s )
            {
            }

            bool operator()( std::uint16_t, const details::attribute& attr ) const
            {
                auto read = details::attribute_access_arguments::compare_value( begin_, end_, &server_ );
                return attr.access( read, 1 ) == details::attribute_access_result::value_equal;
            }

            const std::uint8_t* const begin_;
            const std::uint8_t* const end_;
            Server&                   server_;
        };

        struct collect_find_by_type_groups
        {
            collect_find_by_type_groups( std::uint8_t* begin, std::uint8_t* end )
                : begin_( begin )
                , end_( end )
                , current_( begin )
            {
            }

            template < typename Service >
            bool operator()( std::uint16_t start_handle, std::uint16_t end_handle, const details::attribute& )
            {
                if ( end_ -  current_ >= 4 )
                {
                    current_ = details::write_handle( current_, start_handle );
                    current_ = details::write_handle( current_, end_handle );

                    return true;
                }

                return false;
            }

            std::uint8_t size() const
            {
                return current_ - begin_;
            }

            std::uint8_t* const begin_;
            std::uint8_t* const end_;
            std::uint8_t*       current_;
        };
    }

    template < typename ... Options >
    void server< Options... >::handle_find_by_type_value_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        std::uint16_t starting_handle, ending_handle;

        if ( !check_size_and_handle_range< 9u, 23u >( input, in_size, output, out_size, starting_handle, ending_handle ) )
            return;

        // the spec (v4.2) doesn't define, what to return in this case, but this seems to be a resonable response
        if ( details::read_handle( &input[ 5 ] ) != bits( details::gatt_uuids::primary_service ) )
            return error_response( *input, details::att_error_codes::unsupported_group_type, starting_handle, output, out_size );

        details::collect_find_by_type_groups iterator( output + 1 , output + out_size );

        if ( all_services_by_group( starting_handle, ending_handle, iterator, details::value_filter< server< Options... > >( &input[ 7 ], &input[ in_size ], *this ) ) )
        {
            *output  = bits( details::att_opcodes::find_by_type_value_response );
            out_size = iterator.size() + 1;
        }
        else
        {
            error_response( *input, details::att_error_codes::attribute_not_found, starting_handle, output, out_size );
        }

    }

    template < typename ... Options >
    template < typename ConnectionData >
    void server< Options... >::handle_read_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& connection )
    {
        std::uint16_t handle;

        if ( !check_size_and_handle< 3 >( input, in_size, output, out_size, handle ) )
            return;

        auto read = details::attribute_access_arguments::read( output + 1, output + out_size, 0, connection.client_configurations(), connection.security_attributes(), this );
        auto rc   = attribute_at( handle - 1 ).access( read, handle - 1 );

        if ( rc == details::attribute_access_result::success )
        {
            *output  = bits( details::att_opcodes::read_response );
            out_size = 1 + read.buffer_size;
        }
        else
        {
            error_response( *input, details::att_error_codes::read_not_permitted, handle, output, out_size );
        }
    }

    template < typename ... Options >
    template < typename ConnectionData >
    void server< Options... >::handle_read_blob_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& connection )
    {
        std::uint16_t handle;

        if ( !check_size_and_handle< 5 >( input, in_size, output, out_size, handle ) )
            return;

        const std::uint16_t offset = details::read_16bit( input + 3 );

        auto read = details::attribute_access_arguments::read( output + 1, output + out_size, offset, connection.client_configurations(), connection.security_attributes(), this );
        auto rc   = attribute_at( handle - 1 ).access( read, handle );

        if ( rc == details::attribute_access_result::success )
        {
            *output  = bits( details::att_opcodes::read_blob_response );
            out_size = 1 + read.buffer_size;
        }
        else if ( rc == details::attribute_access_result::invalid_offset )
        {
            error_response( *input, details::att_error_codes::invalid_offset, handle, output, out_size );
        }
        else
        {
            error_response( *input, details::att_error_codes::read_not_permitted, handle, output, out_size );
        }
     }

    namespace details {
        template < typename Server >
        struct collect_attributes
        {
            void operator()( std::size_t index, const details::attribute& attr )
            {
                static constexpr std::size_t maximum_pdu_size = 253u;
                static constexpr std::size_t header_size      = 2u;

                if ( end_ - current_ >= static_cast< std::ptrdiff_t >( header_size ) )
                {
                    const std::size_t max_data_size = std::min< std::size_t >( end_ - current_, maximum_pdu_size + header_size ) - header_size;

                    auto read = attribute_access_arguments::read( current_ + header_size, current_ + header_size + max_data_size, 0, config_, security_, &server_ );
                    auto rc   = attr.access( read, index );

                    if ( rc == details::attribute_access_result::success )
                    {
                        assert( read.buffer_size <= maximum_pdu_size );

                        if ( first_ )
                        {
                            size_   = read.buffer_size + header_size;
                            first_  = false;
                        }

                        if ( read.buffer_size + header_size == size_ )
                        {
                            current_ = details::write_handle( current_, handle_index_mapping< Server >::handle_by_index( index ) );
                            current_ += static_cast< std::uint8_t >( read.buffer_size );
                        }
                    }
                }
            }

            collect_attributes( std::uint8_t* begin, std::uint8_t* end,
                const details::client_characteristic_configuration& config,
                const connection_security_attributes& security,
                Server& server )
                : begin_( begin )
                , current_( begin )
                , end_( end )
                , size_( 0 )
                , first_( true )
                , config_( config )
                , security_( security )
                , server_( server )
            {
            }

            std::uint8_t size() const
            {
                return current_ - begin_;
            }

            std::uint8_t data_size() const
            {
                return size_;
            }

            bool empty() const
            {
                return current_ == begin_;
            }

            std::uint8_t*   begin_;
            std::uint8_t*   current_;
            std::uint8_t*   end_;
            std::uint8_t    size_;
            bool            first_;
            details::client_characteristic_configuration config_;
            connection_security_attributes security_;
            Server&         server_;
        };
    }

    template < typename ... Options >
    template < typename ConnectionData >
    void server< Options... >::handle_read_by_type_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& connection )
    {
        std::uint16_t starting_handle, ending_handle;

        if ( !check_size_and_handle_range< 5 + 2, 5 + 16 >( input, in_size, output, out_size, starting_handle, ending_handle ) )
             return;

        details::collect_attributes< server< Options... > > iterator( output + 2, output + out_size,
            connection.client_configurations(), connection.security_attributes(), *this );

        all_attributes( starting_handle, ending_handle, iterator, details::uuid_filter( input + 5, in_size == 5 + 16 ) );

        if ( !iterator.empty() )
        {
            output[ 0 ] = bits( details::att_opcodes::read_by_type_response );
            output[ 1 ] = iterator.data_size();
            out_size = 2 + iterator.size();
        }
        else
        {
            error_response( *input, details::att_error_codes::attribute_not_found, starting_handle, output, out_size );
        }
    }

    namespace details {
        template < typename CCCDIndices, typename ServiceList, typename Server >
        struct collect_primary_services
        {
            collect_primary_services( std::uint8_t*& output, std::uint8_t* end, std::uint16_t starting_index, std::uint16_t starting_handle, std::uint16_t ending_handle, std::uint8_t& attribute_data_size, Server& server )
                : output_( output )
                , end_( end )
                , index_( starting_index )
                , starting_handle_( starting_handle )
                , ending_handle_( ending_handle )
                , first_( true )
                , is_128bit_uuid_( true )
                , attribute_data_size_( attribute_data_size )
                , server_( server )
            {
            }

            template< typename Service >
            void each()
            {
                if ( starting_handle_ <= index_ && index_ <= ending_handle_ )
                {
                    if ( first_ )
                    {
                        is_128bit_uuid_         = Service::uuid::is_128bit;
                        first_                  = false;
                        attribute_data_size_    = is_128bit_uuid_ ? 16 + 4 : 2 + 4;
                    }

                    /// TODO: ClientCharacteristicIndex is derivable from Service and ServiceList, if 0 is used,
                    /// some templates are most likely more than once instanciated
                    output_ = Service::template read_primary_service_response< CCCDIndices, 0, ServiceList, Server >( output_, end_, index_, is_128bit_uuid_, server_ );
                }

                index_ += Service::number_of_attributes;
            }

                  std::uint8_t*&  output_;
                  std::uint8_t*   end_;
                  std::uint16_t   index_;
            const std::uint16_t   starting_handle_;
            const std::uint16_t   ending_handle_;
                  bool            first_;
                  bool            is_128bit_uuid_;
                  std::uint8_t&   attribute_data_size_;
                  Server&         server_;
        };
    }

    template < typename ... Options >
    void server< Options... >::handle_read_by_group_type_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        std::uint16_t starting_handle, ending_handle;

        if ( !check_size_and_handle_range< 5 + 2, 5 + 16 >( input, in_size, output, out_size, starting_handle, ending_handle ) )
            return;

        if ( in_size == 5 + 16 || details::read_handle( &input[ 5 ] ) != bits( details::gatt_uuids::primary_service ) )
            return error_response( *input, details::att_error_codes::unsupported_group_type, starting_handle, output, out_size );

        std::uint8_t*       begin = output;
        std::uint8_t* const end   = output + out_size;

        begin = details::write_opcode( begin, details::att_opcodes::read_by_group_type_response );
        ++begin; // gap for the size

        std::uint8_t* const data_begin = begin;
        details::for_< services >::each( details::collect_primary_services< cccd_indices, services, server< Options... > >( begin, end, 1, starting_handle, ending_handle, *(begin -1 ), *this ) );

        if ( begin == data_begin )
        {
            error_response( *input, details::att_error_codes::attribute_not_found, starting_handle, output, out_size );
        }
        else
        {
            out_size = begin - output;
        }
    }

    template < typename ... Options >
    template < typename ConnectionData >
    void server< Options... >::handle_read_multiple_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* const output, std::size_t& out_size, ConnectionData& cc )
    {
        if ( in_size < 5 || in_size % 2 == 0 )
            return error_response( *input, details::att_error_codes::invalid_pdu, output, out_size );

        const std::uint8_t opcode = *input;
        ++input;
        --in_size;

        std::uint8_t* const end_output = output + out_size;
        std::uint8_t*       out_ptr    = output;

        *out_ptr = bits( details::att_opcodes::read_multiple_response );
        ++out_ptr;

        for ( const std::uint8_t* const end_input = input + in_size; input != end_input; input += 2 )
        {
            const std::uint16_t handle = details::read_handle( input );

            if ( handle == 0 )
                return error_response( opcode, details::att_error_codes::invalid_handle, handle, output, out_size );

            if ( handle > number_of_attributes )
                return error_response( opcode, details::att_error_codes::attribute_not_found, handle, output, out_size );

            auto read = details::attribute_access_arguments::read( out_ptr, end_output, 0, cc.client_configurations(), cc.security_attributes(), this );
            auto rc   = attribute_at( handle - 1 ).access( read, handle - 1 );

            if ( rc == details::attribute_access_result::success )
            {
                out_ptr += read.buffer_size;
                assert( out_ptr <= end_output );
            }
            else
            {
                return error_response( opcode, details::att_error_codes::read_not_permitted, handle, output, out_size );
            }
        }

        out_size = out_ptr - output;
    }

    template < typename ... Options >
    template < typename ConnectionData >
    void server< Options... >::handle_write_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& connection )
    {
        if ( in_size < 3 )
            return error_response( *input, details::att_error_codes::invalid_pdu, output, out_size );

        std::uint16_t handle;

        if ( !check_handle( input, in_size, output, out_size, handle ) )
            return;

        auto write = details::attribute_access_arguments::write( input + 3, input + in_size, 0, connection.client_configurations(), connection.security_attributes(), this );
        auto rc    = attribute_at( handle - 1 ).access( write, handle );

        if ( rc == details::attribute_access_result::success )
        {
            *output  = bits( details::att_opcodes::write_response );
            out_size = 1;
        }
        else if ( rc == details::attribute_access_result::invalid_attribute_value_length )
        {
            error_response( *input, details::att_error_codes::invalid_attribute_value_length, handle, output, out_size );
        }
        else
        {
            error_response( *input, access_result_to_att_code( rc, details::att_error_codes::write_not_permitted ), handle, output, out_size );
        }
    }

    template < typename ... Options >
    template < typename ConnectionData >
    void server< Options... >::handle_write_command( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& cc )
    {
        // just like a write request
        handle_write_request( input, in_size, output, out_size, cc );

        // but ignore all output
        out_size = 0;
    }

    template < typename ... Options >
    void server< Options... >::handle_prepair_write_request( const std::uint8_t* input, std::size_t, std::uint8_t* output, std::size_t& out_size, connection_data&, const details::no_such_type& )
    {
        error_response( *input, details::att_error_codes::request_not_supported, output, out_size );
    }

    template < typename ... Options >
    template < typename WriteQueue >
    void server< Options... >::handle_prepair_write_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data& client, const WriteQueue& )
    {
        if ( in_size < 5 )
            return error_response( *input, details::att_error_codes::invalid_pdu, output, out_size );

        std::uint16_t handle;

        if ( !check_handle( input, in_size, output, out_size, handle ) )
            return;

        auto write = details::attribute_access_arguments::check_write( this );
        auto rc    = attribute_at( handle - 1 ).access( write, handle );

        if ( rc == details::attribute_access_result::write_not_permitted )
            return error_response( *input, details::att_error_codes::write_not_permitted, handle, output, out_size );

        // find size in the queue to write all but the opcode
        std::uint8_t* const queue_element = this->allocate_from_write_queue( in_size - 1, client );

        if ( queue_element == nullptr )
            return error_response( *input, details::att_error_codes::prepare_queue_full, handle, output, out_size );

        std::copy( input + 1, input + in_size, queue_element );

        *output = bits( details::att_opcodes::prepare_write_response );

        out_size = std::min< std::size_t >( out_size, in_size );
        std::copy( input + 1, input + out_size, output + 1 );
    }

    template < typename ... Options >
    void server< Options... >::handle_execute_write_request( const std::uint8_t* input, std::size_t, std::uint8_t* output, std::size_t& out_size, connection_data&, const details::no_such_type& )
    {
        error_response( *input, details::att_error_codes::request_not_supported, output, out_size );
    }

    template < typename ... Options >
    template < typename WriteQueue >
    void server< Options... >::handle_execute_write_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data& client, const WriteQueue& )
    {
        if ( in_size != 2 || ( input[ 1 ] != 0 && input[ 1 ] != 1 ) )
            return error_response( *input, details::att_error_codes::invalid_pdu, output, out_size );

        // the write queue will be freed in any case
        details::write_queue_guard< connection_data, details::write_queue< write_queue_type > > queue_guard( client, *this );

        const std::uint8_t execute_flag = input[ 1 ];
        if ( execute_flag )
        {

            for ( std::pair< std::uint8_t*, std::size_t > queue = this->first_write_queue_element( client ); queue.first; queue = this->next_write_queue_element( queue.first, client ) )
            {
                const uint16_t handle = details::read_handle( queue.first );
                const uint16_t offset = details::read_16bit( queue.first + 2 );

                auto write = details::attribute_access_arguments::write( queue.first + 4 , queue.first + queue.second,
                                offset, client.client_configurations(), client.security_attributes(), this );
                auto rc    = attribute_at( handle -1 ).access( write, handle );

                if ( rc != details::attribute_access_result::success )
                {
                    if ( rc == details::attribute_access_result::invalid_attribute_value_length )
                        return error_response( *input, details::att_error_codes::invalid_attribute_value_length, handle, output, out_size );

                    return error_response( *input, details::att_error_codes::invalid_offset, handle, output, out_size );
                }
            }
        }

        *output  = bits( details::att_opcodes::execute_write_response );
        out_size = 1;
    }

    template < typename ... Options >
    void server< Options... >::handle_value_confirmation( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data& )
    {
        if ( in_size != 1 )
            return error_response( *input, static_cast< details::att_error_codes >( 0x04 ), output, out_size );

        out_size = 0;

        if ( l2cap_cb_ )
            l2cap_cb_( details::notification_data(), l2cap_arg_, confirmation );
    }


    template < typename ... Options >
    template < class Iterator, class Filter >
    void server< Options... >::all_attributes( std::uint16_t starting_handle, std::uint16_t ending_handle, Iterator& iter, const Filter& filter )
    {
        const std::size_t last_index = last_handle_index( ending_handle );

        // TODO: Incrementing might be very inefficient when handle are very sparse
        for ( std::size_t index = handle_mapping::first_index_by_handle( starting_handle ); index <= last_index; ++index )
        {
            const details::attribute attr = attribute_at( index );

            if ( filter( index, attr ) )
                iter( index, attr );
        }
    }

    namespace details {
        template < class Iterator, class Filter, class AllServices, class Server >
        struct services_by_group
        {
            services_by_group( std::uint16_t starting_handle, std::uint16_t ending_handle, Iterator& iterator, const Filter& filter, bool& found )
                : starting_index_( details::handle_index_mapping< Server >::first_index_by_handle( starting_handle ) )
                , ending_index_( details::handle_index_mapping< Server >::first_index_by_handle( ending_handle ) )
                , index_( 0 )
                , iterator_( iterator )
                , filter_( filter )
                , found_( found )
            {

            }

            template< typename Service >
            void each()
            {
                if ( ( starting_index_ != details::invalid_attribute_index && starting_index_ <= index_ )
                    && ( index_ <= ending_index_ || ending_index_ == details::invalid_attribute_index ) )
                {
                    const details::attribute& attr = Server::attribute_at( index_ );

                    using mapping = details::handle_index_mapping< Server >;

                    if ( filter_( index_, attr ) )
                    {
                        found_ = iterator_.template operator()< Service >(
                            mapping::handle_by_index( index_ ),
                            mapping::handle_by_index( index_ + Service::number_of_attributes - 1 ),
                            attr ) || found_;
                    }
                }

                index_ += Service::number_of_attributes;
            }

            std::size_t     starting_index_;
            std::size_t     ending_index_;
            std::size_t     index_;
            Iterator&       iterator_;
            const Filter&   filter_;
            bool&           found_;
        };
    }

    template < typename ... Options >
    template < class Iterator, class Filter >
    bool server< Options... >::all_services_by_group( std::uint16_t starting_handle, std::uint16_t ending_handle, Iterator& iterator, const Filter& filter )
    {
        bool result = false;
        details::services_by_group< Iterator, Filter, services, server< Options...  > > service_iterator( starting_handle, ending_handle, iterator, filter, result );
        details::for_< services >::each( service_iterator );

        return result;
    }

    template < typename ... Options >
    std::uint8_t* server< Options... >::collect_handle_uuid_tuples( std::uint16_t start, std::uint16_t end, bool only_16_bit, std::uint8_t* out, std::uint8_t* out_end )
    {
        const std::size_t size_per_tuple = only_16_bit
            ? 2 + 2
            : 2 + 16;

        for ( ; start <= end && start <= number_of_attributes
            && static_cast< std::size_t >( out_end - out ) >= size_per_tuple; ++start )
        {
            const details::attribute attr = attribute_at( start -1 );
            const bool is_16_bit_uuids    = attr.uuid != bits( details::gatt_uuids::internal_128bit_uuid );

            if ( only_16_bit == is_16_bit_uuids )
            {
                details::write_handle( out, start );

                if ( is_16_bit_uuids )
                {
                    details::write_16bit_uuid( out + 2, attr.uuid );
                }
                else
                {
                    write_128bit_uuid( out + 2, attribute_at( start -2 ) );
                }

                out += size_per_tuple;
            }
        }

        return out;
    }

    template < typename ... Options >
    void server< Options... >::write_128bit_uuid( std::uint8_t* out, const details::attribute& char_declaration )
    {
        // this is a little bit tricky: To save memory, details::attribute contains only 16 bit uuids at all,
        // but the "Characteristic Value Declaration" contain 16 bit uuids. However, as the "Characteristic Value Declaration"
        // "is the first Attribute after the characteristic declaration", the attribute just in front of the
        // "Characteristic Value Declaration" contains the the 128 bit uuid.
        assert( char_declaration.uuid == bits( details::gatt_uuids::characteristic ) );

        std::uint8_t buffer[ 3 + 16 ];
        auto read = details::attribute_access_arguments::read( buffer, 0 );
        char_declaration.access( read, 1 );

        assert( read.buffer_size == sizeof( buffer ) );

        std::copy( &read.buffer[ 3 ], &read.buffer[ 3 + 16 ], out );
    }

    template < typename ... Options >
    std::size_t server< Options... >::last_handle_index( std::uint16_t ending_handle )
    {
        const std::size_t mapped = handle_mapping::first_index_by_handle( ending_handle );

        return mapped == details::invalid_attribute_index
            ? number_of_attributes - 1
            : mapped;
    }

    template < typename ... Options >
    details::notification_data server< Options... >::find_notification_data( const void* value ) const
    {
        return details::find_notification_data_in_list< notification_priority, services >::find_notification_data( value );
    }

    template < typename ... Options >
    details::notification_data server< Options... >::find_notification_data_by_index( std::size_t client_characteristic_configuration_index ) const
    {
        return details::find_notification_data_in_list< notification_priority, services >::find_notification_data_by_index( client_characteristic_configuration_index );
    }

    /** @endcond */
}

#endif
