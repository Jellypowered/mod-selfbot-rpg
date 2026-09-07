#include "Integration/RuntimeDependencies.h"
#include "Materials/MaterialSourceSelector.h"

namespace Sbrpg::Runtime
{
    Sbrpg::Materials::MaterialDefinition const* ResolveMaterial(std::string value, uint32& itemId)
    {
        Sbrpg::Materials::MaterialDefinition const* material = Sbrpg::Materials::Find(value);
        if (material)
        {
            itemId = material->itemId;
            return material;
        }

        std::string normalized = Sbrpg::Materials::Normalize(std::move(value));
        if (normalized.empty() || !std::all_of(normalized.begin(), normalized.end(),
            [](unsigned char character) { return std::isdigit(character); }))
            return nullptr;
        try
        {
            unsigned long const parsed = std::stoul(normalized);
            if (parsed <= std::numeric_limits<uint32>::max())
                itemId = static_cast<uint32>(parsed);
        }
        catch (std::exception const&)
        {
            itemId = 0;
        }
        return Sbrpg::Materials::Find(itemId);
    }

}
