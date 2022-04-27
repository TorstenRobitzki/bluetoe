#ifndef BLUETOE_LINK_LAYER_PHY_ENCODINGS_HPP
#define BLUETOE_LINK_LAYER_PHY_ENCODINGS_HPP

namespace bluetoe {
namespace link_layer {
namespace details {

    namespace phy_ll_encoding {
        enum phy_ll_encoding_t : std::uint8_t {
            le_unchanged_coding = 0x00,
            le_1m_phy           = 0x01,
            le_2m_phy           = 0x02,
            le_coded_phy        = 0x04,
        };
    }
}
}
}
#endif
