#ifndef BLUETOE_ATTRIBUTE_GENERATOR_HPP
#define BLUETOE_ATTRIBUTE_GENERATOR_HPP

#include <tuple>
#include <bluetoe/options.hpp>

namespace bluetoe {
namespace details {

    template < typename, typename, std::size_t, typename ... Options >
    struct generate_attribute;

    /**
     * generate a const static array of attributes out of a list of tuples, containing the parameter to generate a attribute
     *
     *  Attributes: A type_list, containing a tuple for every attribute to generate.
     *
     *  ClientCharacteristicIndex: The index of the characteristic to be generate in the containing service
     *
     *  Options: All options that where given to the characteristic
     */
    template < typename Attributes, typename CCCDIndices, std::size_t ClientCharacteristicIndex, typename ... Options >
    struct generate_attribute_list;

    template < typename CCCDIndices,std::size_t ClientCharacteristicIndex, typename ... Options >
    struct generate_attribute_list< type_list<>, CCCDIndices, ClientCharacteristicIndex, type_list< Options... > >
    {
        static const attribute attribute_at( std::size_t )
        {
            assert( !"should not happen" );
            return attribute();
        }
    };

    template < typename ... Attributes, typename ... CCCDIndices, std::size_t ClientCharacteristicIndex, typename ... Options >
    struct generate_attribute_list< type_list< Attributes... >, type_list< CCCDIndices... >, ClientCharacteristicIndex, type_list< Options... > >
    {
        static const attribute attribute_at( std::size_t index )
        {
            return attributes[ index ];
        }

        static const attribute attributes[ sizeof ...(Attributes) ];
    };

    template < typename ... Attributes, typename ... CCCDIndices, std::size_t ClientCharacteristicIndex, typename ... Options >
    const attribute generate_attribute_list< type_list< Attributes... >, type_list< CCCDIndices... >, ClientCharacteristicIndex, type_list< Options... > >::attributes[ sizeof ...(Attributes) ] =
    {
        generate_attribute< Attributes, type_list< CCCDIndices... >, ClientCharacteristicIndex, Options... >::attr...
    };

    template < typename OptionsList, typename MetaTypeList, typename OptionsDefault = type_list<> >
    struct count_attributes;

    template < typename OptionsList, typename ... MetaTypes, typename OptionsDefault >
    struct count_attributes< OptionsList, type_list< MetaTypes... >, OptionsDefault >
    {
        using attribute_generation_parameters = typename group_by_meta_types_without_empty_groups<
            typename add_type< OptionsList, OptionsDefault >::type,
            MetaTypes...
        >::type;

        enum { number_of_attributes = type_list_size< attribute_generation_parameters >::value };
    };

    template < typename OptionsList, typename MetaTypeList, typename CCCDIndices, typename OptionsDefault = type_list<> >
    struct generate_attributes;

    template < typename OptionsList, typename ... MetaTypes, typename CCCDIndices, typename OptionsDefault >
    struct generate_attributes< OptionsList, type_list< MetaTypes... >, CCCDIndices, OptionsDefault >
        : count_attributes< OptionsList, type_list< MetaTypes... >, OptionsDefault >
    {
        using attribute_generation_parameters = typename group_by_meta_types_without_empty_groups<
            typename add_type< OptionsList, OptionsDefault >::type,
            MetaTypes...
        >::type;

        attribute_generation_parameters get_attribute_generation_parameters() { return attribute_generation_parameters(); }

        template < std::size_t ClientCharacteristicIndex, typename ServiceUUID, typename Server >
        static const attribute attribute_at( std::size_t index )
        {
            return generate_attribute_list<
                attribute_generation_parameters,
                CCCDIndices,
                ClientCharacteristicIndex,
                typename add_type< OptionsList, details::type_list< ServiceUUID, Server > >::type
            >::attribute_at( index );
        }
    };

}
}

#endif // include guard
