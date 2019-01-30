#ifndef BLUETOE_ATTRIBUTE_GENERATOR_HPP
#define BLUETOE_ATTRIBUTE_GENERATOR_HPP

#include <tuple>
#include <bluetoe/meta_tools.hpp>

namespace bluetoe {
namespace details {

    /**
     *
     */
    template < typename, typename, std::size_t, typename, typename, typename ... Options >
    struct generate_attribute;

    /**
     * generate a const static array of attributes out of a list of tuples, containing the parameter to generate an attribute
     *
     *  Attributes: A std::tuple, containing a tuple for every attribute to generate.
     *
     *  ClientCharacteristicIndex: The index of the characteristic to be generate in the containing service
     *
     *  Options: All options that where given to the characteristic
     */
    template < typename Attributes, typename CCCDIndices, std::size_t ClientCharacteristicIndex, typename Service, typename Server, typename ... Options >
    struct generate_attribute_list;

    template < typename CCCDIndices,std::size_t ClientCharacteristicIndex, typename Service, typename Server, typename ... Options >
    struct generate_attribute_list< std::tuple<>, CCCDIndices, ClientCharacteristicIndex, Service, Server, std::tuple< Options... > >
    {
        static const attribute attribute_at( std::size_t )
        {
            assert( !"should not happen" );
            return attribute();
        }
    };

    template < typename ... Attributes, typename ... CCCDIndices, std::size_t ClientCharacteristicIndex, typename Service, typename Server, typename ... Options >
    struct generate_attribute_list< std::tuple< Attributes... >, std::tuple< CCCDIndices... >, ClientCharacteristicIndex, Service, Server, std::tuple< Options... > >
    {
        static const attribute attribute_at( std::size_t index )
        {
            return attributes[ index ];
        }

        static const attribute attributes[ sizeof ...(Attributes) ];
    };

    template < typename ... Attributes, typename ... CCCDIndices, std::size_t ClientCharacteristicIndex, typename Service, typename Server, typename ... Options >
    const attribute generate_attribute_list< std::tuple< Attributes... >, std::tuple< CCCDIndices... >, ClientCharacteristicIndex, Service, Server, std::tuple< Options... > >::attributes[ sizeof ...(Attributes) ] =
    {
        generate_attribute< Attributes, std::tuple< CCCDIndices... >, ClientCharacteristicIndex, Service, Server, Options... >::attr...
    };

    template < typename OptionsList, typename MetaTypeList, typename OptionsDefault = std::tuple<> >
    struct count_attributes;

    template < typename OptionsList, typename ... MetaTypes, typename OptionsDefault >
    struct count_attributes< OptionsList, std::tuple< MetaTypes... >, OptionsDefault >
    {
        using attribute_generation_parameters = typename group_by_meta_types_without_empty_groups<
            typename add_type< OptionsList, OptionsDefault >::type,
            MetaTypes...
        >::type;

        enum { number_of_attributes = std::tuple_size< attribute_generation_parameters >::value };
    };

    template < typename OptionsList, typename MetaTypeList, typename CCCDIndices, typename OptionsDefault = std::tuple<> >
    struct generate_attributes;

    template < typename OptionsList, typename ... MetaTypes, typename CCCDIndices, typename OptionsDefault >
    struct generate_attributes< OptionsList, std::tuple< MetaTypes... >, CCCDIndices, OptionsDefault >
        : count_attributes< OptionsList, std::tuple< MetaTypes... >, OptionsDefault >
    {
        using attribute_generation_parameters = typename group_by_meta_types_without_empty_groups<
            typename add_type< OptionsList, OptionsDefault >::type,
            MetaTypes...
        >::type;

        attribute_generation_parameters get_attribute_generation_parameters() { return attribute_generation_parameters(); }

        template < std::size_t ClientCharacteristicIndex, typename Service, typename Server >
        static const attribute attribute_at( std::size_t index )
        {
            return generate_attribute_list<
                attribute_generation_parameters,
                CCCDIndices,
                ClientCharacteristicIndex,
                Service,
                Server,
                OptionsList
            >::attribute_at( index );
        }
    };

}
}

#endif // include guard
