#include "Integration/RuntimeDependencies.h"
#include "Integration/ActionRegistration.h"
#include "Nodes/NodeActivity.h"
#include "Materials/MaterialActivity.h"

namespace Sbrpg::Runtime
{
    // Uses stock loot execution without its periodic broad `add all loot`
    // scan. Node farming queues only SBRPG-selected gameobject GUIDs.
    class SelfbotNodeLootStrategy : public Strategy
    {
    public:
        explicit SelfbotNodeLootStrategy(PlayerbotAI* ai) : Strategy(ai) { }
        std::string const getName() override { return "sbrpg node loot"; }
        uint32 GetType() const override { return STRATEGY_TYPE_NONCOMBAT; }
        std::vector<NextAction> getDefaultActions() override
        {
            return { NextAction("loot", 8.0f), NextAction("move to loot", 7.0f), NextAction("open loot", 9.0f) };
        }
    };

    class SelfbotMaterialLootStrategy : public Strategy
    {
    public:
        explicit SelfbotMaterialLootStrategy(PlayerbotAI* ai) : Strategy(ai) { }
        std::string const getName() override { return "sbrpg material loot"; }
        uint32 GetType() const override { return STRATEGY_TYPE_NONCOMBAT; }
        std::vector<NextAction> getDefaultActions() override
        {
            return {
                NextAction("loot", 8.0f),
                NextAction("move to loot", 7.0f),
                NextAction("open loot", 9.0f),
                NextAction("add all loot", 5.0f)
            };
        }
    };

    class SelfbotRpgFarmStrategy : public Strategy
    {
    public:
        SelfbotRpgFarmStrategy(PlayerbotAI* ai) : Strategy(ai) { }
        std::string const getName() override { return "sbrpg farm"; }
        uint32 GetType() const override { return STRATEGY_TYPE_NONCOMBAT; }
        std::vector<NextAction> getDefaultActions() override { return { NextAction("sbrpg farm", 4.0f) }; }
    };

    class SelfbotRpgActionContext : public NamedObjectContext<Action>
    {
    public:
        SelfbotRpgActionContext()
        {
            creators["sbrpg farm"] = &CreateFarm;
            creators["sbrpg material attack"] = &CreateMaterialAttack;
        }
    private:
        static Action* CreateFarm(PlayerbotAI* ai) { return new SelfbotRpgFarmAction(ai); }
        static Action* CreateMaterialAttack(PlayerbotAI* ai) { return new SelfbotMaterialAttackAction(ai); }
    };

    class SelfbotRpgMaterialStrategy : public Strategy
    {
    public:
        SelfbotRpgMaterialStrategy(PlayerbotAI* ai) : Strategy(ai) { }
        std::string const getName() override { return "sbrpg material"; }
        uint32 GetType() const override { return STRATEGY_TYPE_NONCOMBAT; }
        std::vector<NextAction> getDefaultActions() override
        { return { NextAction("sbrpg material attack", 4.5f) }; }
    };

    class SelfbotRpgStrategyContext : public NamedObjectContext<Strategy>
    {
    public:
        SelfbotRpgStrategyContext() : NamedObjectContext<Strategy>(false, false)
        {
            creators["sbrpg farm"] = &CreateFarm;
            creators["sbrpg material"] = &CreateMaterial;
            creators["sbrpg node loot"] = &CreateNodeLoot;
            creators["sbrpg material loot"] = &CreateMaterialLoot;
        }
    private:
        static Strategy* CreateFarm(PlayerbotAI* ai) { return new SelfbotRpgFarmStrategy(ai); }
        static Strategy* CreateMaterial(PlayerbotAI* ai) { return new SelfbotRpgMaterialStrategy(ai); }
        static Strategy* CreateNodeLoot(PlayerbotAI* ai) { return new SelfbotNodeLootStrategy(ai); }
        static Strategy* CreateMaterialLoot(PlayerbotAI* ai) { return new SelfbotMaterialLootStrategy(ai); }
    };

    template <class Ctx>
    void RegisterContexts()
    {
        Ctx::sharedActionContexts.Add(new SelfbotRpgActionContext());
        Ctx::sharedStrategyContexts.Add(new SelfbotRpgStrategyContext());
    }

void RegisterPlayerbotContexts()
{
            RegisterContexts<WarriorAiObjectContext>(); RegisterContexts<PaladinAiObjectContext>();
            RegisterContexts<DruidAiObjectContext>(); RegisterContexts<DKAiObjectContext>();
            RegisterContexts<HunterAiObjectContext>(); RegisterContexts<MageAiObjectContext>();
            RegisterContexts<PriestAiObjectContext>(); RegisterContexts<RogueAiObjectContext>();
            RegisterContexts<ShamanAiObjectContext>(); RegisterContexts<WarlockAiObjectContext>();
}

}
