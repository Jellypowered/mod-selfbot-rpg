#ifndef SELFBOTRPG_PROTOCOL_H
#define SELFBOTRPG_PROTOCOL_H

#include <cstdint>
#include <string>
#include <vector>

namespace Sbrpg::Protocol
{
    constexpr char Prefix[] = "JLYRPG2";
    constexpr uint32_t Version = 1;

    struct Frame
    {
        uint32_t version = 0;
        std::string requestId;
        std::string opcode;
        std::vector<std::string> fields;
    };

    // Parses the complete server-side wire value: Prefix<TAB>version<TAB>opcode<TAB>requestId...
    // Returns false for every malformed, wrong-prefix, wrong-version, or missing request-id frame.
    bool Parse(std::string const& wire, Frame& frame);

    std::string Build(std::string const& opcode, std::vector<std::string> const& fields = {});
}

#endif
