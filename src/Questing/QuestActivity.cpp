#include "Questing/QuestActivity.h"

namespace Sbrpg::Questing
{
FeatureAvailability QuestActivity::Availability()
{
    return {false, "Not implemented; no runtime behavior enabled"};
}
}
