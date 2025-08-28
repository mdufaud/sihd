#ifndef __SIHD_UTIL_STATEMACHINE_HPP__
#define __SIHD_UTIL_STATEMACHINE_HPP__

#include <map>
#include <string>

namespace sihd::util
{

class IStateMachine
{
};

template <typename State, typename Event>
class StateMachine: public IStateMachine
{
    public:
        StateMachine(State initial)
        {
            _last_event = Event();
            _state = initial;
        }
        virtual ~StateMachine() = default;
        ;

        void add_transition(State from, Event event, State into) { _transitions[from][event] = into; }

        bool transition(Event event)
        {
            bool ret = _transitions.find(_state) != _transitions.end();
            if (ret)
            {
                std::map<Event, State> & evt_map = _transitions[_state];
                ret = evt_map.find(event) != evt_map.end();
                if (ret)
                {
                    _state = evt_map[event];
                    _last_event = event;
                }
            }
            return ret;
        }

        State state() const { return _state; }
        Event last_event() const { return _last_event; }
        std::string state_name(State st) const
        {
            auto it = _states_name.find(st);
            return it == _states_name.end() ? "" : it->second;
        }
        std::string event_name(Event evt) const
        {
            auto it = _events_name.find(evt);
            return it == _events_name.end() ? "" : it->second;
        }
        void set_state_name(State st, const std::string & name) { _states_name[st] = name; }
        void set_event_name(Event evt, const std::string & name) { _events_name[evt] = name; }

        void set_transitions_map(const std::map<State, std::map<Event, State>> & transitions)
        {
            _transitions = transitions;
        }
        void set_states_names_map(const std::map<State, std::string> & states) { _states_name = states; }
        void set_events_names_map(const std::map<Event, std::string> & events) { _events_name = events; }

        const std::map<State, std::map<Event, State>> & transitions_map() const { return _transitions; }
        const std::map<State, std::string> & states_names_map() const { return _states_name; }
        const std::map<Event, std::string> & events_names_map() const { return _events_name; }

    protected:

    private:
        State _state;
        std::map<State, std::string> _states_name;

        Event _last_event;
        std::map<Event, std::string> _events_name;

        std::map<State, std::map<Event, State>> _transitions;
};

} // namespace sihd::util

#endif