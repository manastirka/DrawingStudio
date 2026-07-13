#include "ToolSwitchOps.h"

namespace ToolSwitchOps {

void resetCreationStages(int &bezierStage, int &angleLineStage, int &arcStage)
{
    bezierStage = 0;
    angleLineStage = 0;
    arcStage = 0;
}

} // namespace ToolSwitchOps
