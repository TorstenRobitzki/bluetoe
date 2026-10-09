#ifndef BLUETOE_LINK_LAYER_LL_IDS_HPP
#define BLUETOE_LINK_LAYER_LL_IDS_HPP

#include <cstdint>

namespace bluetoe {
namespace link_layer {
namespace details {

    /**
     * @brief different values of the LLID field of the Data Physical Channel PDU header
     */
    namespace llid {
        enum : std::uint8_t
        {
            ll_data_pdu_l2cap_continuation  = 1,
            ll_data_pdu_l2cap_start         = 2,
            ll_control_pdu_code             = 3,
        };
    }

}
}
}
#endif
