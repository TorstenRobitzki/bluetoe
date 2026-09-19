#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_SELF_TESTS_OBSERVED_PORT_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_SELF_TESTS_OBSERVED_PORT_HPP

/**
 * @file observed_port.hpp
 *
 * The port the self tests of the rigs instantiate an instrument with: the dummy of
 * host/, instrumented. It keeps its instance and hands out its buffers, so that a test can
 * play the host end of the link, and it keeps the instance pointer a real port keeps for
 * its interrupt handler, since the instrument owns the object.
 */

#include <cstddef>
#include <cstdint>

namespace bluetoe {
namespace test_rig {
namespace self_test {

    /**
     * @brief a byte ring buffer behind virtual functions
     *
     * An instrument's buffer type exists only inside the instrument; this is how a test
     * reaches the port's buffers without naming it.
     */
    class erased_buffer
    {
    public:
        virtual std::size_t free() const = 0;
        virtual void push( const std::uint8_t* data, std::size_t size ) = 0;
        virtual std::size_t available() const = 0;
        virtual void pop( std::uint8_t* data, std::size_t size ) = 0;

    protected:
        ~erased_buffer() = default;
    };

    template < typename Buffer >
    class erased_buffer_view : public erased_buffer
    {
    public:
        explicit erased_buffer_view( Buffer& buffer )
            : buffer_( buffer )
        {
        }

        std::size_t free() const override
        {
            return buffer_.free();
        }

        void push( const std::uint8_t* data, std::size_t size ) override
        {
            buffer_.push( data, size );
        }

        std::size_t available() const override
        {
            return buffer_.available();
        }

        void pop( std::uint8_t* data, std::size_t size ) override
        {
            buffer_.pop( data, size );
        }

    private:
        Buffer& buffer_;
    };

    /**
     * @brief the part of the observed port that a test can name
     */
    class observed_port_base
    {
    public:
        virtual erased_buffer& receive() = 0;
        virtual erased_buffer& transmit() = 0;

        // what the port does after it pushed received bytes
        virtual void wake() = 0;

        bool started       = false;
        int  transmissions = 0;

        static inline observed_port_base* instance = nullptr;

    protected:
        ~observed_port_base() = default;
    };

    template < typename Buffer, typename Wake >
    class observed_port : public observed_port_base
    {
    public:
        observed_port( Buffer& receive, Buffer& transmit, Wake& wake )
            : receive_( receive )
            , transmit_( transmit )
            , wake_( wake )
        {
            instance = this;
        }

        void start()
        {
            started = true;
        }

        void transmit_pending()
        {
            ++transmissions;
        }

        erased_buffer& receive() override
        {
            return receive_;
        }

        erased_buffer& transmit() override
        {
            return transmit_;
        }

        void wake() override
        {
            wake_.wake_up();
        }

    private:
        erased_buffer_view< Buffer >    receive_;
        erased_buffer_view< Buffer >    transmit_;
        Wake&                           wake_;
    };
}
}
}

#endif
