#include <sihd/util/AService.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Named.hpp>

namespace sihd::util
{

LOGGER;

# define CREATE_SERVICE_OPERATION(OP, ID)\
bool    AService::OP()\
{\
    AService::IServiceController *ctrl = this->get_service_ctrl();\
    bool ret = false;\
    if (ctrl == nullptr)\
        ret = this->on_##OP();\
    else\
    {\
        if (ctrl->op_start(ID))\
        {\
            ret = this->on_##OP();\
            ctrl->op_end(ID, ret);\
        }\
        else\
        {\
            Named *obj = dynamic_cast<Named *>(this);\
            if (obj != nullptr)\
            {\
                LOG(warning, "AService: cannot change the state of " << obj->get_name() << " to " #OP);\
            }\
        }\
    }\
    if (ret)\
        this->notify_observers(this);\
    return ret;\
}\
bool    AService::on_##OP()\
{\
    return true;\
}

CREATE_SERVICE_OPERATION(setup, AService::SETUP);
CREATE_SERVICE_OPERATION(init, AService::INIT);
CREATE_SERVICE_OPERATION(start, AService::START);
CREATE_SERVICE_OPERATION(stop, AService::STOP);
CREATE_SERVICE_OPERATION(reset, AService::RESET);

}