/**
 * @file rpc.hpp
 *
 * Basic idea behind this Remote Procedure Call abstraction is,
 * to have a bidirectional, binary stream over which synchronous remote function calls
 * are serialized and performed. So, all functions on the remote side have to be
 * none blocking.
 *
 * The usuall aproach for a basic RPC implementation is to serialize a function call,
 * by sending an opcode (that identified the function to be called on the remote side),
 * followed by the serialized function arguments of the call and then wait for
 * the serialized function return value.
 *
 * To be able to call remote prodcedures in both directions, there is a need to distiquish between
 * the opcode of a function call (this is the one direction) and the function return value in the
 * oposite direction. This can be solved by using a dedicated opcode for function return values.
 * This implementation uses 0 for function return value. All other values are opcodes for
 * function calls.
 *
 * As the set of functions to be called needs to be known by both sides, the opcode can be
 * derived from an order of the functions (which then have to be equal for both).
 *
 * The set of function arguments and the function return type can be deduced by prototypes of
 * the function to be called. This also implies that there is no need to add a size of
 * parameters.
 *
 * References / pointers can not be serialized. For calls to global objects on the
 * remote side, it is necessary, to register the instance of that global instance.
 *
 *   static const bool registed = protocol.register_implementation< scheduled_radio2 >( impl );
 *
 * Then, all remote calls to a specific interface, would be resolved to the instance `impl`
 * on the remote side:
 *
 *   protocol.call< scheduled_radio2::time_now >();
 *
 * would be resolved to:
 *
 *   impl.time_now();
 *
 * on the remote side.
 */

#include <type_traits>
#include <cstdint>

namespace rpc {

    template < class T >
    concept stream = requires( T stream, std::uint8_t p1 )
    {
        stream.put( p1 );
        { stream.get() } ->  std::same_as< std::uint8_t >;
    };

    template < class T >
    concept remote_procedure = true;


    template < stream IO >
    void deserialize( IO& io, std::uint8_t& b )
    {
        b = io.get();
    }

    template < stream IO >
    void deserialize( IO& io, std::uint16_t& w )
    {
        const std::uint8_t low  = io.get();
        const std::uint8_t high = io.get();

        w = static_cast< std::uint16_t >( low << 0 )
          | static_cast< std::uint16_t >( high << 8 );
    }

    template < stream IO >
    void deserialize( IO& io, std::uint32_t& w )
    {
        std::uint16_t low;
        std::uint16_t high;
        deserialize( io, low );
        deserialize( io, high );

        w = static_cast< std::uint32_t >( low << 0 )
          | static_cast< std::uint32_t >( high << 16 );
    }

    template < stream IO >
    void deserialize( IO& io, bool& b )
    {
        b = io.get();
    }

    namespace details
    {
        template < class RemoteCalls, class LocalFunctions >
        class protocol_t : public RemoteCalls
        {
        public:
            // template < class T, class Res, class C
            // Res call( Res (C::&func)() );

            // template < class T >
            // bool register_implementation( T& instance );
        };

        template < typename Proc, std::uint8_t Index, typename List >
        struct find_function;

        template < typename Proc, std::uint8_t Index, typename P, typename... Ps >
        struct find_function< Proc, Index, std::tuple< P, Ps... > >
        {
            static constexpr bool found = std::is_same< Proc, P >::value;

            static constexpr std::uint8_t value = found
                ? Index
                : find_function< Proc, Index + 1, std::tuple< Ps... > >::value;
        };

        template < typename Proc, std::uint8_t Index >
        struct find_function< Proc, Index, std::tuple<> >
        {
            static constexpr std::uint8_t value = ~0;
        };

        template < typename F >
        struct return_type;

        template < typename R, typename... Args >
        struct return_type< R(*)(Args...) >
        {
            using type = R;
        };

        template < typename R, typename Obj, typename... Args >
        struct return_type< R (Obj::*)(Args...) >
        {
            using type = R;
        };

        template < typename F, F Func >
        struct wrapped_func
        {
        };

        template < typename T >
        struct deserialize_return_value;

        template <>
        struct deserialize_return_value< void >
        {
            template < stream IO >
            static void read( IO& ) {}
        };

        template < typename T >
        struct deserialize_return_value
        {
            template < stream IO >
            static T read( IO& io )
            {
                T result;
                deserialize( io, result );
                return result;
            }
        };

        template < typename... Procs >
        class remote_protocol_t
        {
        public:
            template < auto proc, stream IO >
            auto call( IO& io ) const
            {
                using func = wrapped_func< decltype(proc), proc >;

                constexpr std::uint8_t index = find_function<
                    func,
                    0,
                    std::tuple< Procs... > >::value;

                static_assert( index != ~0, "Function to be called is not member of the set declared for the set of remote functions. Add function to `function_set()`!" );

                io.put( index + 1 );

                return read_result< decltype(proc) >( io );
            }

        private:
            static constexpr std::uint8_t response_opcode = 0x00;

            template < typename F, stream IO >
            auto read_result( IO& io ) const
            {
                for ( std::uint8_t opcode = io.get(); opcode != response_opcode; opcode = io.get() )
                {
                    // TODO if opcode != response_opcode; a call from the other side happend
                }

                return deserialize_return_value< typename return_type< F >::type >::read( io );
            }
        };

    }

    template < auto... procs >
    consteval auto function_set()
    {
        return details::remote_protocol_t< details::wrapped_func<decltype(procs), procs >... >();
    }
    /*

    template < remote_procedure... RemoteProcs, remote_procedure... LocalProcs, stream Stream >
    auto protocol( const details::remote_protocol_t< RemoteProcs... > &, const details::remote_protocol_t< LocalProcs... > &, Stream& io)
    {
        return details::protocol_t<
            details::remote_protocol_t< RemoteProcs... >,
            details::remote_protocol_t< LocalProcs... > >();
    }
*/
}
