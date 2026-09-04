#include "MaterialCatalog.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace Sbrpg::Materials
{
    namespace
    {
        std::vector<MaterialDefinition> const definitions = {
            {2589, "linen", "Linen Cloth", MaterialFamily::Cloth,
                {"linen cloth", "linen"}, {AcquisitionMethod::CreatureLoot}},
            {2592, "wool", "Wool Cloth", MaterialFamily::Cloth,
                {"wool cloth", "wool"}, {AcquisitionMethod::CreatureLoot}},
            {4306, "silk", "Silk Cloth", MaterialFamily::Cloth,
                {"silk cloth", "silk"}, {AcquisitionMethod::CreatureLoot}},
            {4338, "mageweave", "Mageweave Cloth", MaterialFamily::Cloth,
                {"mageweave cloth", "mageweave"}, {AcquisitionMethod::CreatureLoot}},
            {14047, "runecloth", "Runecloth", MaterialFamily::Cloth,
                {"rune cloth", "runecloth"}, {AcquisitionMethod::CreatureLoot}},
            {14256, "felcloth", "Felcloth", MaterialFamily::Cloth,
                {"fel cloth", "felcloth"}, {AcquisitionMethod::CreatureLoot}},
            {21877, "netherweave", "Netherweave Cloth", MaterialFamily::Cloth,
                {"netherweave cloth", "netherweave"}, {AcquisitionMethod::CreatureLoot}},
            {33470, "frostweave", "Frostweave Cloth", MaterialFamily::Cloth,
                {"frostweave cloth", "frostweave"}, {AcquisitionMethod::CreatureLoot}},
            {783, "light hide", "Light Hide", MaterialFamily::Leather,
                {"light hide"}, {AcquisitionMethod::Skinning, AcquisitionMethod::CreatureLoot}},
            {2318, "light leather", "Light Leather", MaterialFamily::Leather,
                {"light leather"}, {AcquisitionMethod::Skinning, AcquisitionMethod::CreatureLoot}},
            {4232, "medium hide", "Medium Hide", MaterialFamily::Leather,
                {"medium hide"}, {AcquisitionMethod::Skinning, AcquisitionMethod::CreatureLoot}},
            {2319, "medium leather", "Medium Leather", MaterialFamily::Leather,
                {"medium leather"}, {AcquisitionMethod::Skinning, AcquisitionMethod::CreatureLoot}},
            {4235, "heavy hide", "Heavy Hide", MaterialFamily::Leather,
                {"heavy hide"}, {AcquisitionMethod::Skinning, AcquisitionMethod::CreatureLoot}},
            {4234, "heavy leather", "Heavy Leather", MaterialFamily::Leather,
                {"heavy leather"}, {AcquisitionMethod::Skinning, AcquisitionMethod::CreatureLoot}},
            {4305, "thick hide", "Thick Hide", MaterialFamily::Leather,
                {"thick hide"}, {AcquisitionMethod::Skinning, AcquisitionMethod::CreatureLoot}},
            {4304, "thick leather", "Thick Leather", MaterialFamily::Leather,
                {"thick leather"}, {AcquisitionMethod::Skinning, AcquisitionMethod::CreatureLoot}},
            {8171, "rugged hide", "Rugged Hide", MaterialFamily::Leather,
                {"rugged hide"}, {AcquisitionMethod::Skinning, AcquisitionMethod::CreatureLoot}},
            {8170, "rugged leather", "Rugged Leather", MaterialFamily::Leather,
                {"rugged leather"}, {AcquisitionMethod::Skinning, AcquisitionMethod::CreatureLoot}},
            {25649, "knothide scraps", "Knothide Leather Scraps", MaterialFamily::Leather,
                {"knothide leather scraps", "knothide scraps"}, {AcquisitionMethod::Skinning, AcquisitionMethod::CreatureLoot}},
            {21887, "knothide", "Knothide Leather", MaterialFamily::Leather,
                {"knothide leather"}, {AcquisitionMethod::Skinning, AcquisitionMethod::CreatureLoot}},
            {33567, "borean scraps", "Borean Leather Scraps", MaterialFamily::Leather,
                {"borean leather scraps", "borean scraps"}, {AcquisitionMethod::Skinning, AcquisitionMethod::CreatureLoot}},
            {33568, "borean", "Borean Leather", MaterialFamily::Leather,
                {"borean leather"}, {AcquisitionMethod::Skinning, AcquisitionMethod::CreatureLoot}},
            {769, "boar meat", "Chunk of Boar Meat", MaterialFamily::Cooking,
                {"chunk of boar meat", "boar meat"}, {AcquisitionMethod::CreatureLoot}},
            {3770, "mutton", "Mutton Chop", MaterialFamily::Cooking,
                {"mutton chop", "mutton"}, {AcquisitionMethod::CreatureLoot}},
            {2672, "wolf meat", "Stringy Wolf Meat", MaterialFamily::Cooking,
                {"stringy wolf meat", "wolf meat"}, {AcquisitionMethod::CreatureLoot}},
            {2673, "coyote meat", "Coyote Meat", MaterialFamily::Cooking,
                {"coyote meat"}, {AcquisitionMethod::CreatureLoot}},
            {27668, "lynx meat", "Lynx Meat", MaterialFamily::Cooking,
                {"lynx meat"}, {AcquisitionMethod::CreatureLoot}},
            {27669, "bat flesh", "Bat Flesh", MaterialFamily::Cooking,
                {"bat flesh"}, {AcquisitionMethod::CreatureLoot}},
            {8150, "deeprock salt", "Deeprock Salt", MaterialFamily::Cooking,
                {"deeprock salt"}, {AcquisitionMethod::CreatureLoot}},
            {22572, "mote of air", "Mote of Air", MaterialFamily::Elemental,
                {"mote air"}, {AcquisitionMethod::CreatureLoot}},
            {22573, "mote of earth", "Mote of Earth", MaterialFamily::Elemental,
                {"mote earth"}, {AcquisitionMethod::CreatureLoot}},
            {22574, "mote of fire", "Mote of Fire", MaterialFamily::Elemental,
                {"mote fire"}, {AcquisitionMethod::CreatureLoot}},
            {22575, "mote of life", "Mote of Life", MaterialFamily::Elemental,
                {"mote life"}, {AcquisitionMethod::CreatureLoot}},
            {22576, "mote of mana", "Mote of Mana", MaterialFamily::Elemental,
                {"mote mana"}, {AcquisitionMethod::CreatureLoot}},
            {22577, "mote of shadow", "Mote of Shadow", MaterialFamily::Elemental,
                {"mote shadow"}, {AcquisitionMethod::CreatureLoot}},
            {22578, "mote of water", "Mote of Water", MaterialFamily::Elemental,
                {"mote water"}, {AcquisitionMethod::CreatureLoot}},
            {21884, "primal fire", "Primal Fire", MaterialFamily::Elemental,
                {"primal fire"}, {AcquisitionMethod::Transformation}},
            {21885, "primal water", "Primal Water", MaterialFamily::Elemental,
                {"primal water"}, {AcquisitionMethod::Transformation}},
            {22451, "primal air", "Primal Air", MaterialFamily::Elemental,
                {"primal air"}, {AcquisitionMethod::Transformation}},
            {22452, "primal earth", "Primal Earth", MaterialFamily::Elemental,
                {"primal earth"}, {AcquisitionMethod::Transformation}},
            {22457, "primal mana", "Primal Mana", MaterialFamily::Elemental,
                {"primal mana"}, {AcquisitionMethod::Transformation}},
            {21886, "primal life", "Primal Life", MaterialFamily::Elemental,
                {"primal life"}, {AcquisitionMethod::Transformation}},
            {22456, "primal shadow", "Primal Shadow", MaterialFamily::Elemental,
                {"primal shadow"}, {AcquisitionMethod::Transformation}}
        };
    }

    std::string Normalize(std::string value)
    {
        std::string normalized;
        bool previousSpace = true;
        for (unsigned char character : value)
        {
            if (std::isspace(character))
            {
                if (!previousSpace)
                    normalized.push_back(' ');
                previousSpace = true;
            }
            else if (std::isalnum(character))
            {
                normalized.push_back(static_cast<char>(std::tolower(character)));
                previousSpace = false;
            }
        }
        if (!normalized.empty() && normalized.back() == ' ')
            normalized.pop_back();
        return normalized;
    }

    std::vector<MaterialDefinition> const& Catalog()
    {
        return definitions;
    }

    MaterialDefinition const* Find(uint32_t itemId)
    {
        auto const it = std::find_if(definitions.begin(), definitions.end(),
            [itemId](MaterialDefinition const& definition) { return definition.itemId == itemId; });
        return it == definitions.end() ? nullptr : &*it;
    }

    MaterialDefinition const* Find(std::string name)
    {
        std::string const normalized = Normalize(std::move(name));
        for (MaterialDefinition const& definition : definitions)
        {
            if (normalized == definition.key || normalized == Normalize(definition.displayName))
                return &definition;
            for (char const* alias : definition.aliases)
                if (normalized == Normalize(alias))
                    return &definition;
        }
        return nullptr;
    }

    char const* FamilyName(MaterialFamily family)
    {
        switch (family)
        {
            case MaterialFamily::Cloth: return "cloth";
            case MaterialFamily::Leather: return "leather";
            case MaterialFamily::Cooking: return "cooking";
            case MaterialFamily::Elemental: return "elemental";
            default: return "other";
        }
    }

    char const* MethodName(AcquisitionMethod method)
    {
        switch (method)
        {
            case AcquisitionMethod::CreatureLoot: return "creature loot";
            case AcquisitionMethod::Skinning: return "skinning";
            case AcquisitionMethod::HerbalismCorpse: return "herbalism corpse";
            case AcquisitionMethod::MiningCorpse: return "mining corpse";
            case AcquisitionMethod::EngineeringCorpse: return "engineering corpse";
            case AcquisitionMethod::GameObjectNode: return "gameobject node";
            case AcquisitionMethod::Fishing: return "fishing";
            case AcquisitionMethod::Transformation: return "transformation";
            case AcquisitionMethod::Reputation: return "reputation";
            default: return "unknown";
        }
    }
}
