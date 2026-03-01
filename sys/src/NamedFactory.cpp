#include <sihd/sys/NamedFactory.hpp>
#include <sihd/util/DynMessage.hpp>
#include <sihd/util/Message.hpp>
#include <sihd/util/MessageField.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Scheduler.hpp>

using sihd::util::DynMessage;
using sihd::util::Message;
using sihd::util::MessageField;
using sihd::util::Named;
using sihd::util::Node;
using sihd::util::Scheduler;

SIHD_REGISTER_FACTORY(Named);
SIHD_REGISTER_FACTORY(Node);
SIHD_REGISTER_FACTORY(Message);
SIHD_REGISTER_FACTORY(MessageField);
SIHD_REGISTER_FACTORY(DynMessage);
SIHD_REGISTER_FACTORY(Scheduler);
