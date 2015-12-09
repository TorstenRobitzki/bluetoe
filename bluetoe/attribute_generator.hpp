#ifndef BLUETOE_ATTRIBUTE_GENERATOR_HPP
#define BLUETOE_ATTRIBUTE_GENERATOR_HPP

#include <tuple>
#include <bluetoe/options.hpp>

namespace bluetoe {
namespace details {

    template < typename, std::size_t, typename ... Options >
    struct generate_attribute;

    /**
     * generate a const static array of attributes out of a list of tuples, containing the parameter to generate a attribute
     *
     *  Attributes: A std::tuple, containing a tuple for every attribute to generate.
     *
     *  ClientCharacteristicIndex: The index of the characteristic to be generate in the containing service
     *
     *  Options: All options that where given to the characteristic
     */
    template < typename Attributes, std::size_t ClientCharacteristicIndex, typename ... Options >
    struct generate_attribute_list;

    template < std::size_t ClientCharacteristicIndex, typename ... Options >
    struct generate_attribute_list< std::tuple<>, ClientCharacteristicIndex, std::tuple< Options... > >
    {
        static const attribute attribute_at( std::size_t index )
        {
            assert( !"should not happen" );
            return attribute();
        }
    };

    template < typename ... Attributes, std::size_t ClientCharacteristicIndex, typename ... Options >
    struct generate_attribute_list< std::tuple< Attributes... >, ClientCharacteristicIndex, std::tuple< Options... > >
    {
        static const attribute attribute_at( std::size_t index )
        {
            return attributes[ index ];
        }

        static attribute attributes[ sizeof ...(Attributes) ];
    };

    template < typename ... Attributes, std::size_t ClientCharacteristicIndex, typename ... Options >
    attribute generate_attribute_list< std::tuple< Attributes... >, ClientCharacteristicIndex, std::tuple< Options... > >::attributes[ sizeof ...(Attributes) ] =
    {
        generate_attribute< Attributes, ClientCharacteristicIndex, Options... >::attr...
    };

    template < typename OptionsList, typename MetaTypeList, typename OptionsDefault = std::tuple<> >
    struct generate_attributes;

    template < typename OptionsList, typename ... MetaTypes, typename OptionsDefault >
    struct generate_attributes< OptionsList, std::tuple< MetaTypes... >, OptionsDefault >
    {
        typedef typename group_by_meta_types_without_empty_groups<
            typename add_type< OptionsList, OptionsDefault >::type,
            MetaTypes...
        >::type attribute_generation_parameters;

        attribute_generation_parameters get_attribute_generation_parameters() { return attribute_generation_parameters(); }

        enum { number_of_attributes     = std::tuple_size< attribute_generation_parameters >::value };

        template < std::size_t ClientCharacteristicIndex, typename ServiceUUID >
        static const attribute attribute_at( std::size_t index )
        {
            return generate_attribute_list< attribute_generation_parameters, ClientCharacteristicIndex, typename add_type< OptionsList, ServiceUUID >::type >::attribute_at( index );
        }
    };

}
}

#endif // include guard
