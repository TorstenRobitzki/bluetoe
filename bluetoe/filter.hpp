#ifndef BLUETOE_FILTER_HPP
#define BLUETOE_FILTER_HPP

#include <bluetoe/attribute.hpp>
#include <bluetoe/bits.hpp>
#include <bluetoe/uuid.hpp>
#include <algorithm>
#include <iterator>

namespace bluetoe {
namespace details {

    /**
     * @brief filters attributes according to there type
     */
    class uuid_filter
    {
    public:
        /**
         * @brief constructs a filter with a 16 bit or 128 bit, little endian encoded uuid.
         *
         * @param bytes points to little endian encode 16 bit or 128 bit uuid
         * @param true, if bytes point to 128 bit uuid, else false
         */
        uuid_filter( const std::uint8_t* bytes, bool is_128bit )
            : bytes_( bytes )
            , is_128bit_( is_128bit )
        {
            if ( is_128bit && representable_as_16bit_uuid( bytes ) )
            {
                // lets treat this uuid as a 16 bit uuid
                is_128bit_ = false;
                bytes_    += 12;
            }
        }

        bool operator()( std::uint16_t, const attribute& attr ) const
        {
            if ( is_128bit_ )
            {
                if ( attr.uuid == bits( gatt_uuids::internal_128bit_uuid ) )
                {
                    auto compare = attribute_access_arguments::compare_128bit_uuid( bytes_ );
                    return attr.access( compare, 1 ) == attribute_access_result::uuid_equal;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return read_16bit_uuid( bytes_ ) == attr.uuid;
            }
        }

    private:
        static bool representable_as_16bit_uuid( const std::uint8_t* bytes )
        {
            return std::equal( std::begin( bluetooth_base_uuid::bytes ), std::end( bluetooth_base_uuid::bytes ) - 4, bytes )
                && bytes[ 14 ] == 0 && bytes[ 15 ] == 0;
        }

        const std::uint8_t* bytes_;
        bool                is_128bit_;
    };

    /**
     * @brief filters a static 16bit uuid
     *
     * UUID have to be an instance of details::uuid16
     * Example:
     * @code
    bluetoe::details::uuid16_filter< bluetoe::details::uuid16< 0x2800 > > filter;
     * @endcode
     */
    template < class UUID >
    struct uuid16_filter;

    template < std::uint64_t UUID, typename Check >
    struct uuid16_filter< uuid16< UUID, Check > >
    {
        constexpr bool operator()( std::uint16_t, const attribute& attr ) const
        {
            return attr.uuid == UUID;
        }
    };

    /**
     * @brief returns every attribute
     */
    struct all_uuid_filter
    {
        constexpr bool operator()( std::uint16_t, const details::attribute& ) const
        {
            return true;
        }
    };

}
}

#endif
