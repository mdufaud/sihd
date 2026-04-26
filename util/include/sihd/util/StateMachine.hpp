#ifndef __SIHD_UTIL_STATEMACHINE_HPP__
#define __SIHD_UTIL_STATEMACHINE_HPP__

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>

namespace sihd::util
{

class IStateMachine
{
};

template <typename State, typename Event>
class StateMachine: public IStateMachine
{
    public:
        StateMachine(State initial): _state(initial), _last_event(Event()) {}
        virtual ~StateMachine() = default;

        void add_transition(State from, Event event, State into) { _transitions[pack_key(from, event)] = into; }

        bool transition(Event event)
        {
            const auto it = _transitions.find(pack_key(_state, event));
            if (it == _transitions.end())
                return false;
            _state = it->second;
            _last_event = event;
            return true;
        }

        State state() const { return _state; }
        Event last_event() const { return _last_event; }

        std::string state_name(State st) const
        {
            const auto it = _states_name.find(st);
            return it == _states_name.end() ? "" : it->second;
        }
        std::string event_name(Event evt) const
        {
            const auto it = _events_name.find(evt);
            return it == _events_name.end() ? "" : it->second;
        }
        void set_state_name(State st, const std::string & name) { _states_name[st] = name; }
        void set_event_name(Event evt, const std::string & name) { _events_name[evt] = name; }

        void set_transitions_map(const std::unordered_map<uint64_t, State> & transitions)
        {
            _transitions = transitions;
        }
        void set_states_names_map(const std::map<State, std::string> & states) { _states_name = states; }
        void set_events_names_map(const std::map<Event, std::string> & events) { _events_name = events; }

        [[nodiscard]] const std::unordered_map<uint64_t, State> & transitions_map() const { return _transitions; }
        const std::map<State, std::string> & states_names_map() const { return _states_name; }
        const std::map<Event, std::string> & events_names_map() const { return _events_name; }

        // Encodes (State, Event) into a single 64-bit key used by the flat transition table.
        static constexpr uint64_t pack_key(State s, Event e) noexcept
        {
            return (static_cast<uint64_t>(static_cast<uint32_t>(s)) << 32)
                   | static_cast<uint64_t>(static_cast<uint32_t>(e));
        }

        // Decodes a 64-bit key back into its (State, Event) components.
        static constexpr std::pair<State, Event> unpack_key(uint64_t key) noexcept
        {
            return {static_cast<State>(static_cast<uint32_t>(key >> 32)),
                    static_cast<Event>(static_cast<uint32_t>(key & 0xFFFF'FFFFu))};
        }

    protected:

    private:
        State _state;
        std::map<State, std::string> _states_name;

        Event _last_event;
        std::map<Event, std::string> _events_name;

        // Flat hash map: key = pack(state, event) → next state.
        // O(1) average lookup vs O(log N) per level with nested std::map.
        std::unordered_map<uint64_t, State> _transitions;
};

} // namespace sihd::util

#endif