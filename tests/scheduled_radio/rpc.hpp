#ifndef BLUETOE_TESTS_LINK_LAYER_RPC_HPP
#define BLUETOE_TESTS_LINK_LAYER_RPC_HPP

/**
 * @file rpc.hpp
 *
 * Basic idea behind this Remote Procedure Call abstraction is,
 * to have a bidirectional, binary stream over which synchronous remote function calls
 * are serialized and performed. All functions on the remote side have to be
 * none blocking, otherwise, the remote side will also block.
 *
 * The usual approach for a basic RPC implementation is to serialize a function call,
 * by sending an opcode (that identified the function to be called on the remote side),
 * followed by the serialized function arguments of the call and then wait for
 * the serialized function return value.
 *
 * To be able to call remote procedures in both directions, there is a need to distinguish between
 * the opcode of a function call (this is the one direction) and the function return value in the
 * opposite direction. This can be solved by using a dedicated opcode for function return values.
 *
 * As the set of functions to be called needs to be known by both sides, the opcode can be
 * derived from an order of the functions (which then have to be equal for both sides).
 *
 * The set of function arguments and the function return type can be deduced by prototypes of
 * the function to be called. This also implies that there is no need to add a size of
 * parameters into the binary stream.
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

    template < stream IO, typename... Ts >
    void deserialize( IO& io, std::tuple< Ts... >& value )
    {
        std::apply([&io]( Ts&... args ) {
            (deserialize( io, args ),...);
        }, value );
    }

    template < stream IO >
    void serialize( IO& io, std::uint8_t b )
    {
        io.put( b );
    }

    template < stream IO >
    void serialize( IO& io, bool b )
    {
        io.put( b ? 1 : 0 );
    }

    template < stream IO >
    void serialize( IO& io, std::uint16_t b )
    {
        serialize( io, static_cast< std::uint8_t >( b & 0xff ) );
        serialize( io, static_cast< std::uint8_t >( b >> 8 ) );
    }

    template < stream IO >
    void serialize( IO& io, std::uint32_t b )
    {
        serialize( io, static_cast< std::uint16_t >( b & 0xffff ) );
        serialize( io, static_cast< std::uint16_t >( b >> 16 ) );
    }

    template < stream IO, typename... Ts >
    void serialize( IO& io, const std::tuple< Ts... >& t )
    {
        std::apply( [&io]( const Ts&... args ) {
            ( serialize( io, args ), ... );
        }, t );
    }

    namespace details
    {
        static constexpr std::uint8_t no_such_function    = ~0;
        static constexpr std::uint8_t return_value_opcode = 0;

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
            static constexpr std::uint8_t value = no_such_function;
        };

        template < typename T, typename Types >
        struct is_element;

        template < typename T, typename Type, typename... Types >
        struct is_element< T, std::tuple< Type, Types... > > :
            std::integral_constant< bool,
                std::is_same< T, Type >::value
             || is_element< T, std::tuple< Types... > >::value
            >
        {};

        template < typename T >
        struct is_element< T, std::tuple<> > : std::false_type {};

        struct no_object_type {};

        template < typename F >
        struct function_details;

        template < typename R, typename... Args >
        struct function_details< R(*)(Args...) >
        {
            using return_type     = R;
            using arguments_type  = std::tuple< Args... >;
            using object_type     = no_object_type;
            using member_function = std::false_type;
        };

        template < typename R, typename Obj, typename... Args >
        struct function_details< R (Obj::*)(Args...) >
        {
            using return_type     = R;
            using arguments_type  = std::tuple< Args... >;
            using object_type     = Obj;
            using member_function = std::true_type;
        };

        template < typename F, F Func >
        struct wrapped_func
        {
            using details = function_details< F >;
            using arguments_t = details::arguments_type;
            using result_t    = details::return_type;
            using object_type = details::object_type;
            using member_function = details::member_function;

            /*
             * Call to member function without return value
             */
            template < stream IO, typename Objs >
            static void call( IO& io, const void*, const Objs& objs, const std::true_type& )
            {
                arguments_t args;
                deserialize( io, args );

                object_type* const obj = std::get< object_type* >( objs );
                assert( obj );

                std::apply( [obj](const auto&... as ){
                    (obj->*Func)( as... );
                }, args );
            }

            /*
             * Call to static / free function without return value
             */
            template < stream IO, typename Objs >
            static void call( IO& io, const void*, const Objs&, const std::false_type& )
            {
                arguments_t args;
                deserialize( io, args );

                std::apply( [](const auto&... as ){
                    Func( as... );
                }, args );
            }

            /*
             * Call to member function with return value
             */
            template < stream IO, typename T, typename Objs >
            static void call( IO& io, const T*, const Objs& objs, const std::true_type& )
            {
                arguments_t args;
                deserialize( io, args );

                object_type* const obj = std::get< object_type* >( objs );
                assert( obj );

                result_t result;
                std::apply( [&result, obj](const auto&... as ){
                    result = (obj->*Func)( as... );
                }, args );

                serialize( io, return_value_opcode );
                serialize( io, result );
            }

            /*
             * Call to static / free function with return value
             */
            template < stream IO, typename T, typename Objs >
            static void call( IO& io, const T*, const Objs&, const std::false_type& )
            {
                arguments_t args;
                deserialize( io, args );

                result_t result;
                std::apply( [&result](const auto&... as ){
                    result = Func( as... );
                }, args );

                serialize( io, return_value_opcode );
                serialize( io, result );
            }

            template < stream IO, typename Objs >
            static void call( IO& io, const Objs& obj )
            {
                // dispatch to version with / without return value
                call( io, static_cast< result_t* >( nullptr ), obj, member_function() );
            }
        };

        template < stream IO, typename T >
        T deserialize_return_value( IO& io, const T* )
        {
            T result;
            deserialize( io, result );
            return result;
        }

        template < stream IO >
        void deserialize_return_value( IO&, const void* )
        {
        }

        template < typename TL >
        struct call_function_from_set;

        template <>
        struct call_function_from_set< std::tuple<> >
        {
            template < stream IO, typename Objs >
            static void call( std::uint8_t, IO&, const Objs& )
            {
            }
        };

        template < typename Proc, Proc Func, typename... Procs >
        struct call_function_from_set< std::tuple< details::wrapped_func< Proc, Func >, Procs... > >
        {
            template < stream IO, typename Objs >
            static void call( std::uint8_t index, IO& io, const Objs& objs )
            {
                if ( index == 0 )
                {
                    details::wrapped_func< Proc, Func >::call( io, objs );
                }
                else
                {
                    call_function_from_set< std::tuple< Procs... > >::call( index - 1, io, objs );
                }
            }
        };

        struct no_local {
        protected:
            template < stream IO >
            void deserialize_call( std::uint8_t, IO& ) const
            {
                assert( !"invalid return code from remote" );
            }
        };

        template < typename Local, typename... WrappedFunc >
        class remote_protocol_t;

        template < typename Local, typename... Procs, auto... Funcs >
        class remote_protocol_t< Local, details::wrapped_func< Procs, Funcs >... >
            : public Local
        {
        public:
            template < auto proc, stream IO, typename... Args >
            auto call( IO& io, Args... args ) const
            {
                using func = wrapped_func< decltype(proc), proc >;

                constexpr std::uint8_t index = find_function<
                    func,
                    0,
                    std::tuple< details::wrapped_func< Procs, Funcs >... > >::value;

                static_assert( index != no_such_function, "Function to be called is not member of the set declared for the set of remote functions. Add function to `function_set()`!" );

                io.put( index + 1 );
                serialize( io, typename func::arguments_t( args... ) );

                return read_result< typename func::result_t >( io );
            }

        private:
            template < typename R, stream IO >
            auto read_result( IO& io ) const
            {
                for ( std::uint8_t opcode = io.get(); opcode != return_value_opcode; opcode = io.get() )
                {
                    this->deserialize_call( opcode, io );
                }

                return deserialize_return_value( io, static_cast< R* >( nullptr ) );
            }
        };

        template < typename... WrappedFunc >
        class local_protocol_t;

        template < typename... Procs, auto... Funcs >
        class local_protocol_t< details::wrapped_func< Procs, Funcs >... >
        {
        public:
            template < stream IO >
            void deserialize_call( IO& io ) const
            {
                deserialize_call( io.get(), io );
            }

            template < typename T >
            bool register_implementation( T& local_object )
            {
                std::get< T* >( local_objects_ ) = &local_object;

                return true;
            }

        protected:
            template < stream IO >
            void deserialize_call( std::uint8_t opcode, IO& io ) const
            {
                assert( opcode > 0 );
                assert( opcode <= sizeof...( Procs ) );

                call_function_from_set<
                    std::tuple< details::wrapped_func< Procs, Funcs >... > >::call( opcode - 1, io, local_objects_ );
            }

        private:
            template < class ObjectTypes, class FunctionTypes >
            struct local_objects_impl;

            template < typename... ObjectTypes >
            struct local_objects_impl< std::tuple< ObjectTypes... >, std::tuple<> >
            {
                using type = std::tuple< ObjectTypes... >;
            };

            template < typename... ObjectTypes, typename Fkt, typename... FunctionTypes >
            struct local_objects_impl< std::tuple< ObjectTypes... >, std::tuple< Fkt, FunctionTypes... > >
            {
                using object_type = Fkt::object_type*;

                static constexpr bool take = Fkt::member_function::value && !is_element< object_type, std::tuple< ObjectTypes... > >::value;

                using objects = std::conditional<
                    take,
                    std::tuple< ObjectTypes..., object_type >,
                    std::tuple< ObjectTypes... > >::type;

                using type = local_objects_impl< objects, std::tuple< FunctionTypes... > >::type;
            };

            using local_objects_t = typename local_objects_impl< std::tuple<>, std::tuple< details::wrapped_func< Procs, Funcs >... > >::type;
            local_objects_t local_objects_;
        };

        template < typename... WrappedFunc >
        class functions_t;

        template < typename... Procs, auto... Funcs >
        class functions_t< details::wrapped_func< Procs, Funcs >... >
        {
        };
    }

    template < auto... procs >
    consteval auto remote_protocol()
    {
        return details::remote_protocol_t<
            details::no_local,
            details::wrapped_func< decltype(procs), procs >... >();
    }

    template < auto... procs >
    consteval auto local_protocol()
    {
        return details::local_protocol_t< details::wrapped_func< decltype(procs), procs >... >();
    }

    template < typename Loc, typename... RemoteProcs, typename... LocalProcs >
    consteval auto protocol( const details::remote_protocol_t< Loc, RemoteProcs... > &, const details::local_protocol_t< LocalProcs... > & )
    {
        return
            details::remote_protocol_t<
                details::local_protocol_t< LocalProcs... >,
                RemoteProcs... >();
    }

    template < auto... procs >
    consteval auto functions()
    {
        return details::functions_t< details::wrapped_func< decltype(procs), procs >... >();
    }

    template < typename... RemoteProcs, typename... LocalProcs >
    consteval auto protocol( const details::functions_t< RemoteProcs... > &, const details::functions_t< LocalProcs... > & )
    {
        return
            details::remote_protocol_t<
                details::local_protocol_t< LocalProcs... >,
                RemoteProcs... >();
    }

}

#endif
