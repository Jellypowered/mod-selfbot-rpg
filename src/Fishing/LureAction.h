#pragma once
#include "UseItemAction.h"
namespace Sbrpg::Runtime {
    class SbrpgLureUseAction : public UseItemAction
    {
    public:
        explicit SbrpgLureUseAction(PlayerbotAI* ai) : UseItemAction(ai, "sbrpg fishing lure") { }
        bool Apply(Item* lure, Item* pole) { return lure && pole && UseItemOnItem(lure, pole); }
    };
}
