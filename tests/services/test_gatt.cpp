#include <boost/test/unit_test.hpp>

#include "test_gatt.hpp"
#include <hexdump.hpp>
#include <iomanip>

namespace test {
    discovered_service::discovered_service( std::uint16_t start, std::uint16_t end )
        : starting_handle( start )
        , ending_handle( end )
    {
    }

    discovered_service::discovered_service()
        : starting_handle( invalid_handle )
        , ending_handle( invalid_handle )
    {
    }

    bool discovered_service::operator==( const discovered_service& rhs ) const
    {
        return starting_handle == rhs.starting_handle
            && ending_handle == rhs.ending_handle;
    }

    std::ostream& operator<<( std::ostream& out, const discovered_service& service )
    {
        return out << std::hex << "{ 0x" << service.starting_handle << ", 0x" << service.ending_handle << " }";
    }

    discovered_characteristic::discovered_characteristic( std::uint16_t decl, std::uint16_t value, std::uint8_t p, const dynamic_uuid& u )
        : declaration_handle( decl )
        , end_handle( invalid_handle )
        , value_handle( value )
        , properties( p )
        , uuid( u )
    {

    }

    discovered_characteristic::discovered_characteristic()
        : declaration_handle( invalid_handle )
        , end_handle( invalid_handle )
        , value_handle( invalid_handle )
        , uuid()
    {

    }

    bool discovered_characteristic::operator==( const discovered_characteristic& rhs ) const
    {
        return declaration_handle == rhs.declaration_handle
            && end_handle == rhs.end_handle
            && value_handle == rhs.value_handle
            && properties == rhs.properties
            && uuid == rhs.uuid;
    }

    std::ostream& operator<<( std::ostream& out, const discovered_characteristic& characteristic )
    {
        return out
            << std::hex << "{ 0x" << characteristic.declaration_handle << ":0x" << characteristic.end_handle
            << ", 0x" << characteristic.properties
            << ", 0x" << characteristic.value_handle
            << ", " << characteristic.uuid << " }";
    }

    discovered_characteristic_descriptor::discovered_characteristic_descriptor( std::uint16_t h, std::uint16_t u )
        : handle( h )
        , uuid( u )
    {
    }

    discovered_characteristic_descriptor::discovered_characteristic_descriptor()
        : handle( invalid_handle )
        , uuid( invalid_handle )
    {
    }

    bool discovered_characteristic_descriptor::operator==( const discovered_characteristic_descriptor& rhs ) const
    {
        return handle == rhs.handle
            && uuid == rhs.uuid;
    }

    std::ostream& operator<<( std::ostream& out, const discovered_characteristic_descriptor& desc )
    {
        return out << std::hex << "{ 0x" << desc.handle << ", 0x" << desc.uuid << " }";
    }

}
