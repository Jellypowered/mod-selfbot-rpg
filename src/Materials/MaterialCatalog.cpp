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
            {2934, "ruined leather scraps", "Ruined Leather Scraps", MaterialFamily::Leather,
                {"ruined scraps"}, {AcquisitionMethod::Skinning, AcquisitionMethod::CreatureLoot}},
            {15417, "devilsaur leather", "Devilsaur Leather", MaterialFamily::Leather,
                {"devilsaur"}, {AcquisitionMethod::Skinning}},
            {17012, "core leather", "Core Leather", MaterialFamily::Leather,
                {"core leather"}, {AcquisitionMethod::Skinning}},
            {25699, "crystal infused leather", "Crystal Infused Leather", MaterialFamily::Leather,
                {"crystal leather"}, {AcquisitionMethod::Skinning}},
            {29548, "nether dragonscales", "Nether Dragonscales", MaterialFamily::Leather,
                {"nether dragonscale"}, {AcquisitionMethod::Skinning}},
            {29547, "wind scales", "Wind Scales", MaterialFamily::Leather,
                {"wind scale"}, {AcquisitionMethod::Skinning}},
            {44128, "arctic fur", "Arctic Fur", MaterialFamily::Leather,
                {"arctic fur"}, {AcquisitionMethod::Skinning}},
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
            {2674, "crawler meat", "Crawler Meat", MaterialFamily::Cooking,
                {"crawler meat"}, {AcquisitionMethod::CreatureLoot}},
            {2675, "crawler claw", "Crawler Claw", MaterialFamily::Cooking,
                {"crawler claw"}, {AcquisitionMethod::CreatureLoot}},
            {2677, "boar ribs", "Boar Ribs", MaterialFamily::Cooking,
                {"boar ribs"}, {AcquisitionMethod::CreatureLoot}},
            {2886, "crag boar rib", "Crag Boar Rib", MaterialFamily::Cooking,
                {"crag boar rib"}, {AcquisitionMethod::CreatureLoot}},
            {3173, "bear meat", "Bear Meat", MaterialFamily::Cooking,
                {"bear meat"}, {AcquisitionMethod::CreatureLoot}},
            {3404, "buzzard wing", "Buzzard Wing", MaterialFamily::Cooking,
                {"buzzard wing"}, {AcquisitionMethod::CreatureLoot}},
            {3730, "big bear meat", "Big Bear Meat", MaterialFamily::Cooking,
                {"big bear meat"}, {AcquisitionMethod::CreatureLoot}},
            {43010, "worm meat", "Worm Meat", MaterialFamily::Cooking,
                {"worm meat"}, {AcquisitionMethod::CreatureLoot}},
            {43011, "worg haunch", "Worg Haunch", MaterialFamily::Cooking,
                {"worg haunch"}, {AcquisitionMethod::CreatureLoot}},
            {43012, "rhino meat", "Rhino Meat", MaterialFamily::Cooking,
                {"rhino meat"}, {AcquisitionMethod::CreatureLoot}},
            {43013, "chilled meat", "Chilled Meat", MaterialFamily::Cooking,
                {"chilled meat"}, {AcquisitionMethod::CreatureLoot}},
            {43009, "shoveltusk flank", "Shoveltusk Flank", MaterialFamily::Cooking,
                {"shoveltusk flank"}, {AcquisitionMethod::CreatureLoot}},
            {6291, "brilliant smallfish", "Raw Brilliant Smallfish", MaterialFamily::Fishing,
                {"raw brilliant smallfish"}, {AcquisitionMethod::Fishing}},
            {6317, "loch frenzy", "Raw Loch Frenzy", MaterialFamily::Fishing,
                {"raw loch frenzy"}, {AcquisitionMethod::Fishing}},
            {6358, "oily blackmouth", "Oily Blackmouth", MaterialFamily::Fishing,
                {"oily blackmouth"}, {AcquisitionMethod::Fishing}},
            {6359, "firefin snapper", "Firefin Snapper", MaterialFamily::Fishing,
                {"firefin snapper"}, {AcquisitionMethod::Fishing}},
            {6361, "rainbow fin albacore", "Raw Rainbow Fin Albacore", MaterialFamily::Fishing,
                {"rainbow fin albacore"}, {AcquisitionMethod::Fishing}},
            {6522, "deviate fish", "Deviate Fish", MaterialFamily::Fishing,
                {"deviate"}, {AcquisitionMethod::Fishing}},
            {13754, "glossy mightfish", "Raw Glossy Mightfish", MaterialFamily::Fishing,
                {"glossy mightfish"}, {AcquisitionMethod::Fishing}},
            {13755, "winter squid", "Winter Squid", MaterialFamily::Fishing,
                {"winter squid"}, {AcquisitionMethod::Fishing}},
            {13756, "summer bass", "Raw Summer Bass", MaterialFamily::Fishing,
                {"summer bass"}, {AcquisitionMethod::Fishing}},
            {13757, "lightning eel", "Lightning Eel", MaterialFamily::Fishing,
                {"lightning eel"}, {AcquisitionMethod::Fishing}},
            {13758, "redgill", "Raw Redgill", MaterialFamily::Fishing,
                {"redgill"}, {AcquisitionMethod::Fishing}},
            {13759, "nightfin snapper", "Raw Nightfin Snapper", MaterialFamily::Fishing,
                {"nightfin snapper"}, {AcquisitionMethod::Fishing}},
            {13760, "sunscale salmon", "Raw Sunscale Salmon", MaterialFamily::Fishing,
                {"sunscale salmon"}, {AcquisitionMethod::Fishing}},
            {27422, "barbed gill trout", "Barbed Gill Trout", MaterialFamily::Fishing,
                {"barbed gill trout"}, {AcquisitionMethod::Fishing}},
            {27425, "spotted feltail", "Spotted Feltail", MaterialFamily::Fishing,
                {"spotted feltail"}, {AcquisitionMethod::Fishing}},
            {27429, "sporefish", "Zangarian Sporefish", MaterialFamily::Fishing,
                {"zangarian sporefish", "sporefish"}, {AcquisitionMethod::Fishing}},
            {27435, "mudfish", "Figluster's Mudfish", MaterialFamily::Fishing,
                {"mudfish"}, {AcquisitionMethod::Fishing}},
            {27438, "golden darter", "Golden Darter", MaterialFamily::Fishing,
                {"golden darter"}, {AcquisitionMethod::Fishing}},
            {27439, "furious crawdad", "Furious Crawdad", MaterialFamily::Fishing,
                {"crawdad"}, {AcquisitionMethod::Fishing}},
            {6308, "bristle whisker catfish", "Raw Bristle Whisker Catfish", MaterialFamily::Fishing,
                {"raw bristle whisker catfish"}, {AcquisitionMethod::Fishing}},
            {6289, "longjaw mud snapper", "Raw Longjaw Mud Snapper", MaterialFamily::Fishing,
                {"raw longjaw mud snapper"}, {AcquisitionMethod::Fishing}},
            {787, "slitherskin mackerel", "Slitherskin Mackerel", MaterialFamily::Fishing,
                {"raw slitherskin mackerel"}, {AcquisitionMethod::Fishing}},
            {8364, "mithril head trout", "Mithril Head Trout", MaterialFamily::Fishing,
                {"raw mithril head trout"}, {AcquisitionMethod::Fishing}},
            {21071, "sagefish", "Raw Sagefish", MaterialFamily::Fishing,
                {"raw sagefish"}, {AcquisitionMethod::Fishing}},
            {41800, "deep sea monsterbelly", "Deep Sea Monsterbelly", MaterialFamily::Fishing,
                {"monsterbelly"}, {AcquisitionMethod::Fishing}},
            {41801, "moonglow cuttlefish", "Moonglow Cuttlefish", MaterialFamily::Fishing,
                {"cuttlefish"}, {AcquisitionMethod::Fishing}},
            {41802, "imperial manta ray", "Imperial Manta Ray", MaterialFamily::Fishing,
                {"manta ray"}, {AcquisitionMethod::Fishing}},
            {41803, "rockfin grouper", "Rockfin Grouper", MaterialFamily::Fishing,
                {"rockfin"}, {AcquisitionMethod::Fishing}},
            {41805, "borean man o war", "Borean Man O' War", MaterialFamily::Fishing,
                {"borean man o war"}, {AcquisitionMethod::Fishing}},
            {41806, "musselback sculpin", "Musselback Sculpin", MaterialFamily::Fishing,
                {"sculpin"}, {AcquisitionMethod::Fishing}},
            {41807, "dragonfin angelfish", "Dragonfin Angelfish", MaterialFamily::Fishing,
                {"dragonfin"}, {AcquisitionMethod::Fishing}},
            {41808, "bonescale snapper", "Bonescale Snapper", MaterialFamily::Fishing,
                {"bonescale"}, {AcquisitionMethod::Fishing}},
            {41809, "glacial salmon", "Glacial Salmon", MaterialFamily::Fishing,
                {"glacial salmon"}, {AcquisitionMethod::Fishing}},
            {41810, "fangtooth herring", "Fangtooth Herring", MaterialFamily::Fishing,
                {"fangtooth"}, {AcquisitionMethod::Fishing}},
            {41812, "barrelhead goby", "Barrelhead Goby", MaterialFamily::Fishing,
                {"goby"}, {AcquisitionMethod::Fishing}},
            {2835, "copper ore", "Copper Ore", MaterialFamily::Blacksmithing,
                {"copper"}, {AcquisitionMethod::GameObjectNode}},
            {2770, "tin ore", "Tin Ore", MaterialFamily::Blacksmithing,
                {"tin"}, {AcquisitionMethod::GameObjectNode}},
            {2771, "silver ore", "Silver Ore", MaterialFamily::Blacksmithing,
                {"silver"}, {AcquisitionMethod::GameObjectNode}},
            {2772, "iron ore", "Iron Ore", MaterialFamily::Blacksmithing,
                {"iron"}, {AcquisitionMethod::GameObjectNode}},
            {3858, "mithril ore", "Mithril Ore", MaterialFamily::Blacksmithing,
                {"mithril"}, {AcquisitionMethod::GameObjectNode}},
            {7911, "truesilver ore", "Truesilver Ore", MaterialFamily::Blacksmithing,
                {"truesilver"}, {AcquisitionMethod::GameObjectNode}},
            {10620, "thorium ore", "Thorium Ore", MaterialFamily::Blacksmithing,
                {"thorium"}, {AcquisitionMethod::GameObjectNode}},
            {23424, "fel iron ore", "Fel Iron Ore", MaterialFamily::Blacksmithing,
                {"fel iron"}, {AcquisitionMethod::GameObjectNode}},
            {23425, "adamantite ore", "Adamantite Ore", MaterialFamily::Blacksmithing,
                {"adamantite"}, {AcquisitionMethod::GameObjectNode}},
            {36909, "cobalt ore", "Cobalt Ore", MaterialFamily::Blacksmithing,
                {"cobalt ore"}, {AcquisitionMethod::GameObjectNode}},
            {36912, "saronite ore", "Saronite Ore", MaterialFamily::Blacksmithing,
                {"saronite ore"}, {AcquisitionMethod::GameObjectNode}},
            {36910, "titanium ore", "Titanium Ore", MaterialFamily::Blacksmithing,
                {"titanium"}, {AcquisitionMethod::GameObjectNode}},
            {2447, "peacebloom", "Peacebloom", MaterialFamily::Alchemy,
                {"peace bloom"}, {AcquisitionMethod::GameObjectNode}},
            {765, "silverleaf", "Silverleaf", MaterialFamily::Alchemy,
                {"silver leaf"}, {AcquisitionMethod::GameObjectNode}},
            {2449, "earthroot", "Earthroot", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {785, "mageroyal", "Mageroyal", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {2450, "briarthorn", "Briarthorn", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {2453, "bruiseweed", "Bruiseweed", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {3355, "wild steelbloom", "Wild Steelbloom", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {3356, "kingsblood", "Kingsblood", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {3357, "liferoot", "Liferoot", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {3358, "khadgars whisker", "Khadgar's Whisker", MaterialFamily::Alchemy,
                {"khadgar whisker"}, {AcquisitionMethod::GameObjectNode}},
            {3818, "fadeleaf", "Fadeleaf", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {3819, "wintersbite", "Wintersbite", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {3820, "stranglekelp", "Stranglekelp", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {4625, "firebloom", "Firebloom", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {8831, "purple lotus", "Purple Lotus", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {8836, "arthas tears", "Arthas' Tears", MaterialFamily::Alchemy,
                {"arthas tears"}, {AcquisitionMethod::GameObjectNode}},
            {8838, "sungrass", "Sungrass", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {8839, "blindweed", "Blindweed", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {8845, "ghost mushroom", "Ghost Mushroom", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {13463, "dreamfoil", "Dreamfoil", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {13464, "golden sansam", "Golden Sansam", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {13465, "mountain silversage", "Mountain Silversage", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {13466, "plaguebloom", "Plaguebloom", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {22785, "felweed", "Felweed", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {22786, "dreaming glory", "Dreaming Glory", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {22787, "ragveil", "Ragveil", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {22789, "terocone", "Terocone", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {22790, "ancient lichen", "Ancient Lichen", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {22791, "netherbloom", "Netherbloom", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {22792, "nightmare vine", "Nightmare Vine", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {36901, "goldclover", "Goldclover", MaterialFamily::Alchemy,
                {"gold clover"}, {AcquisitionMethod::GameObjectNode}},
            {36903, "adder tongue", "Adder's Tongue", MaterialFamily::Alchemy,
                {"adder tongue"}, {AcquisitionMethod::GameObjectNode}},
            {36904, "tiger lily", "Tiger Lily", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {36905, "lichbloom", "Lichbloom", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {36906, "icethorn", "Icethorn", MaterialFamily::Alchemy,
                {}, {AcquisitionMethod::GameObjectNode}},
            {36907, "talandras rose", "Talandra's Rose", MaterialFamily::Alchemy,
                {"talandra rose"}, {AcquisitionMethod::GameObjectNode}},
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
            {7067, "elemental earth", "Elemental Earth", MaterialFamily::Alchemy,
                {"elemental earth"}, {AcquisitionMethod::CreatureLoot}},
            {7068, "elemental fire", "Elemental Fire", MaterialFamily::Alchemy,
                {"elemental fire"}, {AcquisitionMethod::CreatureLoot}},
            {7069, "elemental air", "Elemental Air", MaterialFamily::Alchemy,
                {"elemental air"}, {AcquisitionMethod::CreatureLoot}},
            {7070, "elemental water", "Elemental Water", MaterialFamily::Alchemy,
                {"elemental water"}, {AcquisitionMethod::CreatureLoot}},
            {7076, "essence of earth", "Essence of Earth", MaterialFamily::Alchemy,
                {"essence earth"}, {AcquisitionMethod::CreatureLoot}},
            {7077, "heart of fire", "Heart of Fire", MaterialFamily::Alchemy,
                {"heart fire"}, {AcquisitionMethod::CreatureLoot}},
            {7078, "essence of fire", "Essence of Fire", MaterialFamily::Alchemy,
                {"essence fire"}, {AcquisitionMethod::CreatureLoot}},
            {7079, "globe of water", "Globe of Water", MaterialFamily::Alchemy,
                {"globe water"}, {AcquisitionMethod::CreatureLoot}},
            {7080, "essence of water", "Essence of Water", MaterialFamily::Alchemy,
                {"essence water"}, {AcquisitionMethod::CreatureLoot}},
            {7081, "breath of wind", "Breath of Wind", MaterialFamily::Alchemy,
                {"breath wind"}, {AcquisitionMethod::CreatureLoot}},
            {7082, "essence of air", "Essence of Air", MaterialFamily::Alchemy,
                {"essence air"}, {AcquisitionMethod::CreatureLoot}},
            {12808, "essence of undeath", "Essence of Undeath", MaterialFamily::Alchemy,
                {"essence undeath"}, {AcquisitionMethod::CreatureLoot}},
            {12803, "living essence", "Living Essence", MaterialFamily::Alchemy,
                {"living essence"}, {AcquisitionMethod::CreatureLoot}},
            {7191, "fused wiring", "Fused Wiring", MaterialFamily::Engineering,
                {"fused wiring"}, {AcquisitionMethod::CreatureLoot, AcquisitionMethod::EngineeringCorpse}},
            {10558, "gold power core", "Gold Power Core", MaterialFamily::Engineering,
                {"gold power core"}, {AcquisitionMethod::CreatureLoot}},
            {10561, "mithril casing", "Mithril Casing", MaterialFamily::Engineering,
                {"mithril casing"}, {AcquisitionMethod::Transformation}},
            {15994, "thorium widget", "Thorium Widget", MaterialFamily::Engineering,
                {"thorium widget"}, {AcquisitionMethod::Transformation}},
            {8167, "turtle scale", "Turtle Scale", MaterialFamily::Leather,
                {"turtle scale"}, {AcquisitionMethod::Skinning}},
            {15412, "green dragonscale", "Green Dragonscale", MaterialFamily::Leather,
                {"green dragonscale"}, {AcquisitionMethod::Skinning}},
            {15414, "red dragonscale", "Red Dragonscale", MaterialFamily::Leather,
                {"red dragonscale"}, {AcquisitionMethod::Skinning}},
            {15415, "blue dragonscale", "Blue Dragonscale", MaterialFamily::Leather,
                {"blue dragonscale"}, {AcquisitionMethod::Skinning}},
            {15416, "black dragonscale", "Black Dragonscale", MaterialFamily::Leather,
                {"black dragonscale"}, {AcquisitionMethod::Skinning}},
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
            case MaterialFamily::Blacksmithing: return "blacksmithing";
            case MaterialFamily::Alchemy: return "alchemy";
            case MaterialFamily::Engineering: return "engineering";
            case MaterialFamily::Fishing: return "fishing";
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
