#ifndef BLUETOE_L2CAP_CHANNELS_HPP
#define BLUETOE_L2CAP_CHANNELS_HPP

namespace bluetoe {

    namespace l2cap_channel_ids {
        /**
         * @brief currently supported LE L2CAP channels
         */
        enum l2cap_channel_ids : std::uint16_t {
            att             = 4,
            signaling       = 5,
            sm              = 6
        };
    }
}

#endif
