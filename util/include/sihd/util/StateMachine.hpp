#ifndef __SIHD_UTIL_STATEMACHINE_HPP__
#define __SIHD_UTIL_STATEMACHINE_HPP__

#include <map>
#include <string>

namespace sihd::util
{

class IStateMachine
{
};

template <typename STATE, typename EVENT>
class StateMachine: public IStateMachine
{
    public:
        StateMachine(STATE initial)
        {
            _last_event = EVENT();
            _state = initial;
        }
        virtual ~StateMachine() {};

        void add_transition(STATE from, EVENT event, STATE into) { _transitions[from][event] = into; }

        bool transition(EVENT event)
        {
            bool ret = _transitions.find(_state) != _transitions.end();
            if (ret)
            {
                std::map<EVENT, STATE> & evt_map = _transitions[_state];
                ret = evt_map.find(event) != evt_map.end();
                if (ret)
                {
                    _state = evt_map[event];
                    _last_event = event;
                }
            }
            return ret;
        }

        STATE state() const { return _state; }
        EVENT last_event() const { return _last_event; }
        std::string state_name(STATE st) const
        {
            auto it = _states_name.find(st);
            return it == _states_name.end() ? "" : it->second;
        }
        std::string event_name(EVENT evt) const
        {
            auto it = _events_name.find(evt);
            return it == _events_name.end() ? "" : it->second;
        }
        void set_state_name(STATE st, const std::string & name) { _states_name[st] = name; }
        void set_event_name(EVENT evt, const std::string & name) { _events_name[evt] = name; }

        void set_transitions_map(const std::map<STATE, std::map<EVENT, STATE>> & transitions)
        {
            _transitions = transitions;
        }
        void set_states_names_map(const std::map<STATE, std::string> & states) { _states_name = states; }
        void set_events_names_map(const std::map<EVENT, std::string> & events) { _events_name = events; }

        const std::map<STATE, std::map<EVENT, STATE>> & transitions_map() const { return _transitions; }
        const std::map<STATE, std::string> & states_names_map() const { return _states_name; }
        const std::map<EVENT, std::string> & events_names_map() const { return _events_name; }

    protected:

    private:
        STATE _state;
        std::map<STATE, std::string> _states_name;

        EVENT _last_event;
        std::map<EVENT, std::string> _events_name;

        std::map<STATE, std::map<EVENT, STATE>> _transitions;
};

} // namespace sihd::util

#endif