#include "SbrpgProtocol.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace Sbrpg::Protocol
{
    bool Parse(std::string const& wire, Frame& frame)
    {
        frame = Frame();
        if (wire.size() > 240 || wire.rfind(std::string(Prefix) + '\t', 0) != 0)
            return false;
        for (unsigned char c : wire)
            if ((c < 0x20 && c != '\t') || c == 0x7f)
                return false;

        std::istringstream input(wire.substr(sizeof(Prefix)));
        std::string version;
        if (!std::getline(input, version, '\t') || version != "1")
            return false;
        if (!std::getline(input, frame.opcode, '\t') || frame.opcode.empty())
            return false;

        for (std::string field; std::getline(input, field, '\t');)
        {
            if (field.size() > 96 || frame.fields.size() >= 16)
                return false;
            frame.fields.push_back(std::move(field));
        }
        if (frame.fields.empty() || frame.fields.front().empty() || frame.fields.front().size() > 10 ||
            !std::all_of(frame.fields.front().begin(), frame.fields.front().end(), [](unsigned char c) { return c >= '0' && c <= '9'; }))
            return false;
        frame.requestId = std::move(frame.fields.front());
        frame.fields.erase(frame.fields.begin());
        frame.version = Version;
        return true;
    }

    std::string Build(std::string const& opcode, std::vector<std::string> const& fields)
    {
        std::string wire = std::string(Prefix) + '\t' + std::to_string(Version) + '\t' + opcode;
        for (std::string const& field : fields)
            wire += '\t' + field;
        return wire;
    }
}
