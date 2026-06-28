#include <sihd/util/AService.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Named.hpp>

namespace sihd::util
{

SIHD_LOGGER;

AService::AService(): _service_state(this) {}

#define CREATE_SERVICE_OPERATION(OP, ID)                                                                               \
    bool AService::OP()                                                                                                \
    {                                                                                                                  \
        AService::IServiceController *ctrl = this->service_ctrl();                                                     \
        bool ret = false;                                                                                              \
        if (ctrl == nullptr)                                                                                           \
            ret = this->do_##OP();                                                                                     \
        else                                                                                                           \
        {                                                                                                              \
            if (ctrl->op_start(ID))                                                                                    \
            {                                                                                                          \
                ret = this->do_##OP();                                                                                 \
                ctrl->op_end(ID, ret);                                                                                 \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                Named *obj = dynamic_cast<Named *>(this);                                                              \
                if (obj != nullptr)                                                                                    \
                {                                                                                                      \
                    SIHD_LOG(warning, "AService: cannot change the state of {} to " #OP, obj->name());                 \
                }                                                                                                      \
            }                                                                                                          \
        }                                                                                                              \
        if (ret)                                                                                                       \
        {                                                                                                              \
            _service_state.notify_observers_delegate();                                                                \
        }                                                                                                              \
        return ret;                                                                                                    \
    }                                                                                                                  \
                                                                                                                       \
    bool AService::do_##OP()                                                                                           \
    {                                                                                                                  \
        return true;                                                                                                   \
    }

CREATE_SERVICE_OPERATION(setup, AService::Setup);
CREATE_SERVICE_OPERATION(init, AService::Init);
CREATE_SERVICE_OPERATION(start, AService::Start);
CREATE_SERVICE_OPERATION(stop, AService::Stop);
CREATE_SERVICE_OPERATION(reset, AService::Reset);

} // namespace sihd::util
