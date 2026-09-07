#include "Integration/RuntimeDependencies.h"
#include "Nodes/NodeResources.h"
#include "Protocol/RequestConfiguration.h"

namespace Sbrpg::Runtime
{
    bool ConfigureFarm(Player* player, std::string const& professionText, std::string const& entries,
                       uint32 durationMinutes, std::string* error)
    {
        std::string profession = professionText;
        if (NormalizeResource(profession) == "zone") profession = entries;
        profession = NormalizeResource(profession);
        Sbrpg::Profession type;
        if (profession == "mining" || profession == "mine") type = Sbrpg::Profession::Mining;
        else if (profession == "herbalism" || profession == "herb") type = Sbrpg::Profession::Herbalism;
        else if (profession == "both") type = Sbrpg::Profession::Both;
        else { if (error) *error = "Profession must be mining or herbalism."; return false; }
        std::vector<uint32> nodeEntries;
        if (NormalizeResource(professionText) == "zone" || type == Sbrpg::Profession::Both)
        {
            // Exact resource-name families, never generic locked gameobjects.
            std::string where;
            if (type == Sbrpg::Profession::Mining || type == Sbrpg::Profession::Both)
                where = "name REGEXP 'Copper Vein|Tin Vein|Silver Vein|Iron Deposit|Gold Vein|Mithril Deposit|Truesilver Deposit|Thorium|Fel Iron Deposit|Adamantite|Khorium|Cobalt|Saronite|Titanium'";
            if (type == Sbrpg::Profession::Herbalism || type == Sbrpg::Profession::Both)
            {
                if (!where.empty()) where += " OR ";
                where += "name REGEXP 'Peacebloom|Silverleaf|Earthroot|Mageroyal|Briarthorn|Bruiseweed|Steelbloom|Kingsblood|Liferoot|Fadeleaf|Goldthorn|Khadgar|Firebloom|Lotus|Arthas|Sungrass|Blindweed|Ghost Mushroom|Gromsblood|Dreamfoil|Silversage|Plaguebloom|Icecap|Felweed|Dreaming Glory|Ragveil|Flame Cap|Terocone|Lichen|Netherbloom|Nightmare Vine|Mana Thistle|Goldclover|Tiger Lily|Talandra|Deadnettle|Fire Leaf|Adder|Lichbloom|Icethorn'";
            }
            QueryResult result = WorldDatabase.Query("SELECT entry FROM gameobject_template WHERE {}", where);
            if (result) do { nodeEntries.push_back(result->Fetch()[0].Get<uint32>()); } while (result->NextRow());
        }
        else
            nodeEntries = ResolveResource(type, entries);
        if (nodeEntries.empty())
        {
            if (error)
                *error = "Unknown resource or no matching gameobject template: " + entries;
            return false;
        }
        return Sbrpg::Start(player, type, std::move(nodeEntries), durationMinutes, error);
    }

}
