#include "Safety/Blacklist.h"

namespace Sbrpg::Safety
{
FeatureAvailability Blacklist::Availability()
{
    return {false, "Not implemented; no runtime behavior enabled"};
}
}
