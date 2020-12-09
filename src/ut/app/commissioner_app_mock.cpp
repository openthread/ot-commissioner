// TODO copyright

#include "commissioner_app_mock.hpp"

static CommissionerAppStaticExpecter *gpCommissionerAppStaticExpecter = nullptr;

void SetCommissionerAppStaticExpecter(CommissionerAppStaticExpecter *ptr)
{
    gpCommissionerAppStaticExpecter = ptr;
}

void ClearCommissionerAppStaticExpecter()
{
    gpCommissionerAppStaticExpecter = nullptr;
}

namespace ot {
namespace commissioner {

Error CommissionerAppCreate(std::shared_ptr<CommissionerApp> &aCommApp, const Config &aConfig)
{
    return gpCommissionerAppStaticExpecter->Create(aCommApp, aConfig);
}

} // namespace commissioner
} // namespace ot
