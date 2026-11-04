#ifndef __SIHD_CORE_TREE_HELPER_HPP__
#define __SIHD_CORE_TREE_HELPER_HPP__

#include <string>

#include <sihd/core/Channel.hpp>
#include <sihd/core/Device.hpp>
#include <sihd/util/IHandler.hpp>
#include <sihd/util/Named.hpp>
#include <sihd/util/Waitable.hpp>

namespace test
{

class NamedObserver: public sihd::util::Named,
                     public sihd::util::IHandler<sihd::core::Channel *>
{
    public:
        NamedObserver(const std::string & name): sihd::util::Named(name) {}

        void handle(sihd::core::Channel *c) override
        {
            ++calls;
            (void)c;
        }

        int calls = 0;
};

class ProfilingDevice: public sihd::core::Device
{
    public:
        ProfilingDevice(const std::string & name, sihd::util::Node *parent = nullptr): sihd::core::Device(name, parent)
        {
        }

        bool is_running() const override { return _running; }

        void handle(sihd::core::Channel *c) override
        {
            ++handled;
            sihd::core::Device::handle(c);
        }

        int handled = 0;

    protected:
        bool on_init() override
        {
            this->add_channel("c1", "int");
            this->add_channel("c2", "int");
            return true;
        }

        bool on_start() override
        {
            (void)this->observe_channel("c1");
            (void)this->observe_channel("c2");
            _running = true;
            return true;
        }

        bool on_stop() override
        {
            _running = false;
            return true;
        }

    private:
        bool _running = false;
};

class FailingStartDevice: public sihd::core::Device
{
    public:
        FailingStartDevice(const std::string & name, sihd::util::Node *parent = nullptr):
            sihd::core::Device(name, parent)
        {
        }

        bool is_running() const override { return false; }

    protected:
        bool on_init() override
        {
            this->add_channel("c1", "int");
            return true;
        }

        bool on_start() override { return false; }
};

// reclaims its channels when the service stops: their pointers die on the state change
class ReclaimingDevice: public sihd::core::Device
{
    public:
        ReclaimingDevice(const std::string & name, sihd::util::Node *parent = nullptr): sihd::core::Device(name, parent)
        {
        }

        bool is_running() const override { return this->device_state() == sihd::util::ServiceController::Running; }

    protected:
        bool on_init() override
        {
            this->add_channel("c1", "int");
            return true;
        }

        bool on_stop() override
        {
            this->remove_child("c1");
            return true;
        }
};

// holds the dispatch thread on every event while blocked
class EventGate
{
    public:
        void block()
        {
            auto lock = _waitable.guard();
            _blocked = true;
        }

        void release()
        {
            {
                auto lock = _waitable.guard();
                _blocked = false;
            }
            _waitable.notify_all();
        }

        bool wait_held(int count)
        {
            return _waitable.wait_for(sihd::util::Duration(sihd::util::time::sec(1)),
                                      [this, count] { return _held >= count; });
        }

        void pass()
        {
            {
                auto lock = _waitable.guard();
                if (_blocked)
                    ++_held;
            }
            _waitable.notify_all();
            _waitable.wait([this] { return _blocked == false; });
        }

    private:
        sihd::util::Waitable _waitable;
        bool _blocked = false;
        int _held = 0;
};

} // namespace test

#endif
