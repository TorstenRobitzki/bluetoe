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

        bool operator()( std::uint16_t index, const attribute& attr ) const
        {
            if ( is_128bit_ && attr.uuid == bits( gatt_uuids::internal_128bit_uuid ) )
            {
                auto compare = attribute_access_arguments::compare_128bit_uuid( bytes_ );

                return attr.access( compare ) == attribute_access_result::uuid_equal;
            }
            else
            {
                return read_16bit_uuid( bytes_ ) == attr.uuid;
            }
        }

        static bool representable_as_16bit_uuid( const std::uint8_t* bytes )
        {
            return std::equal( std::begin( bluetooth_base_uuid::bytes ), std::end( bluetooth_base_uuid::bytes ) - 4, bytes )
                && bytes[ 14 ] == 0 && bytes[ 15 ] == 0;
        }
    private:
        const std::uint8_t* bytes_;
        bool                is_128bit_;
    };

}
}

#endif
