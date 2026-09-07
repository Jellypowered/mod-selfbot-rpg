#include "Integration/RuntimeDependencies.h"
#include "Nodes/NodeResources.h"

namespace Sbrpg::Runtime
{
    std::vector<uint32> ResolveNodeEntriesForItem(uint32 itemId)
    {
        std::vector<uint32> entries;
        QueryResult rows = WorldDatabase.Query(
            "SELECT DISTINCT gt.entry FROM gameobject_template gt "
            "JOIN gameobject_loot_template gl ON gl.Entry = gt.Data1 "
            "WHERE gt.type IN (3, 25) AND gl.Item = {}", itemId);
        if (!rows)
            return entries;
        do
        {
            uint32 entry = rows->Fetch()[0].Get<uint32>();
            if (std::find(entries.begin(), entries.end(), entry) == entries.end())
                entries.push_back(entry);
        } while (rows->NextRow());
        return entries;
    }

    bool IsMiningMaterial(uint32 itemId)
    {
        switch (itemId)
        {
            case 2835: case 2770: case 2771: case 2772: case 3858: case 7911:
            case 10620: case 23424: case 23425: case 36909: case 36912: case 36910:
                return true;
            default: return false;
        }
    }

    bool HasEntry(Sbrpg::FarmState const& state, uint32 entry)
    {
        return std::find(state.entries.begin(), state.entries.end(), entry) != state.entries.end();
    }

    bool MatchesSkill(Sbrpg::Profession profession, uint32 skill)
    {
        return (profession == Sbrpg::Profession::Mining && skill == SKILL_MINING) ||
               (profession == Sbrpg::Profession::Herbalism && skill == SKILL_HERBALISM) ||
               (profession == Sbrpg::Profession::Both && (skill == SKILL_MINING || skill == SKILL_HERBALISM));
    }

    bool MatchesProfession(Sbrpg::FarmState const& state, LootObject const& loot)
    {
        return MatchesSkill(state.profession, loot.skillId);
    }

    bool IsGatheringEntry(Sbrpg::Profession profession, uint32 entry)
    {
        GameObjectTemplate const* info = sObjectMgr->GetGameObjectTemplate(entry);
        LockEntry const* lock = info ? sLockStore.LookupEntry(info->GetLockId()) : nullptr;
        if (!lock) return false;
        for (uint8 i = 0; i < 8; ++i)
            if (lock->Type[i] == LOCK_KEY_SKILL && MatchesSkill(profession, SkillByLockType(LockType(lock->Index[i]))))
                return true;
        return false;
    }

    std::string NormalizeResource(std::string value)
    {
        std::string normalized;
        bool previousSpace = true;
        for (unsigned char c : value)
        {
            if (std::isspace(c))
            {
                if (!previousSpace) normalized += ' ';
                previousSpace = true;
            }
            else if (std::isalpha(c))
            {
                normalized += static_cast<char>(std::tolower(c));
                previousSpace = false;
            }
        }
        if (!normalized.empty() && normalized.back() == ' ')
            normalized.pop_back();
        return normalized;
    }

    // Resource names deliberately resolve through the server's gameobject
    // templates rather than baking entry IDs into the UI. That keeps the module
    // compatible with custom database packs that retain the WotLK node names.
    std::vector<uint32> ResolveResource(Sbrpg::Profession profession, std::string const& name)
    {
        static std::unordered_map<std::string, std::string> const mining = {
            {"copper", "Copper Vein"}, {"tin", "Tin Vein"}, {"silver", "Silver Vein"},
            {"iron", "Iron Deposit"}, {"gold", "Gold Vein"}, {"mithril", "Mithril Deposit"},
            {"truesilver", "Truesilver Deposit"}, {"thorium", "Thorium"},
            {"fel iron", "Fel Iron Deposit"}, {"adamantite", "Adamantite"},
            {"khorium", "Khorium"}, {"cobalt", "Cobalt"}, {"saronite", "Saronite"},
            {"titanium", "Titanium"}
        };
        static std::unordered_map<std::string, std::string> const herbs = {
            {"peacebloom", "Peacebloom"}, {"silverleaf", "Silverleaf"}, {"earthroot", "Earthroot"},
            {"mageroyal", "Mageroyal"}, {"briarthorn", "Briarthorn"}, {"bruiseweed", "Bruiseweed"},
            {"wild steelbloom", "Wild Steelbloom"}, {"kingsblood", "Kingsblood"},
            {"liferoot", "Liferoot"}, {"fadeleaf", "Fadeleaf"}, {"goldthorn", "Goldthorn"},
            {"khadgar's whisker", "Khadgar's Whisker"}, {"firebloom", "Firebloom"},
            {"purple lotus", "Purple Lotus"}, {"arthas' tears", "Arthas' Tears"},
            {"sungrass", "Sungrass"}, {"blindweed", "Blindweed"}, {"ghost mushroom", "Ghost Mushroom"},
            {"gromsblood", "Gromsblood"}, {"dreamfoil", "Dreamfoil"}, {"mountain silversage", "Mountain Silversage"},
            {"plaguebloom", "Plaguebloom"}, {"icecap", "Icecap"}, {"felweed", "Felweed"},
            {"dreaming glory", "Dreaming Glory"}, {"ragveil", "Ragveil"}, {"flame cap", "Flame Cap"},
            {"terocone", "Terocone"}, {"ancient lichen", "Ancient Lichen"}, {"netherbloom", "Netherbloom"},
            {"nightmare vine", "Nightmare Vine"}, {"mana thistle", "Mana Thistle"},
            {"goldclover", "Goldclover"}, {"tiger lily", "Tiger Lily"}, {"talandra's rose", "Talandra's Rose"},
            {"deadnettle", "Deadnettle"}, {"fire leaf", "Fire Leaf"}, {"adder's tongue", "Adder's Tongue"},
            {"lichbloom", "Lichbloom"}, {"icethorn", "Icethorn"}, {"frost lotus", "Frost Lotus"}
        };

        auto const& resources = profession == Sbrpg::Profession::Mining ? mining : herbs;
        auto it = resources.find(NormalizeResource(name));
        if (it == resources.end())
            return {};

        std::vector<uint32> entries;
        QueryResult result = WorldDatabase.Query(
            "SELECT entry FROM gameobject_template WHERE name LIKE '%{}%'", it->second);
        if (!result)
            return entries;
        do
        {
            uint32 const entry = result->Fetch()[0].Get<uint32>();
            if (entry && std::find(entries.begin(), entries.end(), entry) == entries.end())
                entries.push_back(entry);
        } while (result->NextRow());
        return entries;
    }

    char const* ProfessionName(Sbrpg::Profession profession)
    {
        return profession == Sbrpg::Profession::Mining ? "mining" : profession == Sbrpg::Profession::Herbalism ? "herbalism" : "mining and herbalism";
    }

    char const* PhaseName(Sbrpg::FarmPhase phase)
    {
        return Sbrpg::PhaseLabel(phase);
    }

}
