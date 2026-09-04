#ifndef SELFBOTRPG_MATERIAL_CATALOG_H
#define SELFBOTRPG_MATERIAL_CATALOG_H

#include <cstdint>
#include <string>
#include <vector>

namespace Sbrpg::Materials
{
    enum class MaterialFamily : uint8_t
    {
        Cloth,
        Leather,
        Cooking,
        Elemental,
        Other
    };

    enum class AcquisitionMethod : uint8_t
    {
        CreatureLoot,
        Skinning,
        HerbalismCorpse,
        MiningCorpse,
        EngineeringCorpse,
        GameObjectNode,
        Fishing,
        Transformation,
        Reputation
    };

    struct MaterialDefinition
    {
        uint32_t itemId = 0;
        char const* key = "";
        char const* displayName = "";
        MaterialFamily family = MaterialFamily::Other;
        std::vector<char const*> aliases;
        std::vector<AcquisitionMethod> methods;
    };

    std::vector<MaterialDefinition> const& Catalog();
    MaterialDefinition const* Find(uint32_t itemId);
    MaterialDefinition const* Find(std::string name);
    std::string Normalize(std::string value);
    char const* FamilyName(MaterialFamily family);
    char const* MethodName(AcquisitionMethod method);
}

#endif
