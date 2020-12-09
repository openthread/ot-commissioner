#include "border_agent_functions_mock.hpp"

namespace ot {
namespace commissioner {

BorderAgentFunctionsMock *gBorderAgentFunctionsMock = nullptr;

void SetBorderAgentFunctionsMock(ot::commissioner::BorderAgentFunctionsMock *ptr)
{
    gBorderAgentFunctionsMock = ptr;
}

void ClearBorderAgentFunctionsMock()
{
    gBorderAgentFunctionsMock = nullptr;
}

Error DiscoverBorderAgent(BorderAgentHandler aBorderAgentHandler, size_t aTimeout)
{
    return gBorderAgentFunctionsMock->DiscoverBorderAgent(aBorderAgentHandler, aTimeout);
}

} // namespace commissioner
} // namespace ot
