/**
 * TODO copyright
 */
/**
 * @file Mock functions related to BorderAgent
 */

#ifndef OT_COM_BORDER_AGENT_FUNCTIONS_MOCK_HPP_
#define OT_COM_BORDER_AGENT_FUNCTIONS_MOCK_HPP_

#include "gmock/gmock.h"

#include "app/border_agent.hpp"

namespace ot {
namespace commissioner {

class BorderAgentFunctionsMock
{
public:
    virtual ~BorderAgentFunctionsMock() = default;

    MOCK_METHOD(Error, DiscoverBorderAgent, (BorderAgentHandler, size_t));
};

void SetBorderAgentFunctionsMock(ot::commissioner::BorderAgentFunctionsMock *ptr);
void ClearBorderAgentFunctionsMock();

} // namespace commissioner
} // namespace ot

#endif // OT_COM_BORDER_AGENT_FUNCTIONS_MOCK_HPP_
