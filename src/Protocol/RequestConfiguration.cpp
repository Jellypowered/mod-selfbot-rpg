#include "Integration/RuntimeDependencies.h"
#include "Nodes/NodeResources.h"
#include "Protocol/RequestConfiguration.h"

namespace Sbrpg::Runtime
{
    bool ConfigureFarm(Player* player, std::string const& professionText, std::string const& entries,
                       uint32 durationMinutes, uint32 quantityGoal, std::string* error)
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
        uint32 targetItemId = 0;
        if (quantityGoal != 0)
        {
            // Node sessions count actual inventory gains, so a quantity goal
            // is meaningful only for one explicit resource, not zone/both.
            if (type == Sbrpg::Profession::Both || NormalizeResource(professionText) == "zone")
            {
                if (error) *error = "A quantity goal requires one exact mining or herbalism resource.";
                return false;
            }
            static std::unordered_map<std::string, uint32> const resourceItems = {
                { "copper", 2770 }, { "tin", 2771 }, { "silver", 2775 }, { "iron", 2772 },
                { "gold", 2776 }, { "mithril", 3858 }, { "truesilver", 7911 }, { "thorium", 10620 },
                { "fel iron", 23424 }, { "adamantite", 23425 }, { "khorium", 23426 },
                { "cobalt", 36909 }, { "saronite", 36912 }, { "titanium", 36910 },
                { "peacebloom", 2447 }, { "silverleaf", 765 }, { "earthroot", 2449 },
                { "mageroyal", 785 }, { "briarthorn", 2450 }, { "bruiseweed", 2453 },
                { "wild steelbloom", 3355 }, { "kingsblood", 3356 }, { "liferoot", 3357 },
                { "fadeleaf", 3818 }, { "goldthorn", 3821 }, { "felweed", 22785 },
                { "goldclover", 36901 }, { "lichbloom", 36905 }, { "icethorn", 36906 },
                { "frost lotus", 36907 }
            };
            auto const item = resourceItems.find(NormalizeResource(entries));
            if (item == resourceItems.end())
            {
                if (error) *error = "The selected resource does not support an exact item quantity goal.";
                return false;
            }
            targetItemId = item->second;
        }
        return Sbrpg::Start(player, type, std::move(nodeEntries), durationMinutes, error, targetItemId, quantityGoal);
    }

}
