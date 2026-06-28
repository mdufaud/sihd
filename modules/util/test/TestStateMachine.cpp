#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/StateMachine.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::util;

enum Event
{
    EVT1 = 1,
    EVT2,
    EVT_SUCCESS,
    EVT_ERROR,
};

enum State
{
    FIRST,
    BEFORE_EVT1,
    DONE_EVT1,
    DONE_EVT2,
    Error,
};

class TestStateMachine: public ::testing::Test
{
    protected:
        TestStateMachine() { sihd::util::LoggerManager::stream(); }

        virtual ~TestStateMachine() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        void add_names(StateMachine<State, Event> & machine)
        {
            machine.set_event_name(Event::EVT1, "EVT1");
            machine.set_event_name(Event::EVT2, "EVT2");
            machine.set_event_name(Event::EVT_SUCCESS, "EVT_SUCCESS");
            machine.set_event_name(Event::EVT_ERROR, "EVT_ERROR");

            machine.set_state_name(State::FIRST, "FIRST");
            machine.set_state_name(State::BEFORE_EVT1, "BEFORE_EVT1");
            machine.set_state_name(State::DONE_EVT1, "DONE_EVT1");
            machine.set_state_name(State::DONE_EVT2, "DONE_EVT2");
            machine.set_state_name(State::Error, "Error");
        }

        void add_transitions(StateMachine<State, Event> & machine)
        {
            // first -> evt1 -> done_evt1
            machine.add_transition(State::FIRST, Event::EVT1, State::BEFORE_EVT1);
            machine.add_transition(State::BEFORE_EVT1, Event::EVT_ERROR, State::Error);
            machine.add_transition(State::BEFORE_EVT1, Event::EVT_SUCCESS, State::DONE_EVT1);

            // done_evt1 -> evt2 -> done_evt2
            machine.add_transition(State::DONE_EVT1, Event::EVT2, State::DONE_EVT2);
        }
};

TEST_F(TestStateMachine, test_statemachine_transitions)
{
    StateMachine<State, Event> first_machine(FIRST);
    this->add_transitions(first_machine);

    EXPECT_FALSE(first_machine.transition(Event::EVT_SUCCESS));
    EXPECT_FALSE(first_machine.transition(Event::EVT_ERROR));
    EXPECT_FALSE(first_machine.transition(Event::EVT2));

    EXPECT_EQ(first_machine.state(), FIRST);

    // FIRST -> EVT1 -> BEFORE_EVT1
    EXPECT_TRUE(first_machine.transition(EVT1));
    EXPECT_EQ(first_machine.last_event(), EVT1);
    EXPECT_EQ(first_machine.state(), BEFORE_EVT1);

    // EVT1 -> EVT1 => no transition - no change
    EXPECT_FALSE(first_machine.transition(EVT1));
    EXPECT_EQ(first_machine.last_event(), EVT1);
    EXPECT_EQ(first_machine.state(), BEFORE_EVT1);

    // EVT1 -> EVT_ERROR -> error state
    EXPECT_TRUE(first_machine.transition(EVT_ERROR));
    EXPECT_EQ(first_machine.last_event(), EVT_ERROR);
    EXPECT_EQ(first_machine.state(), State::Error);

    // let's get back to before it failed
    StateMachine<State, Event> evt1_machine(BEFORE_EVT1);
    this->add_transitions(evt1_machine);

    EXPECT_EQ(evt1_machine.state(), BEFORE_EVT1);

    // let's say we did it
    EXPECT_TRUE(evt1_machine.transition(EVT_SUCCESS));
    EXPECT_EQ(evt1_machine.last_event(), EVT_SUCCESS);
    EXPECT_EQ(evt1_machine.state(), State::DONE_EVT1);

    // testing last transition -> DONE_EVT1 -> EVT2 -> DONE_EVT2
    EXPECT_TRUE(evt1_machine.transition(EVT2));
    EXPECT_EQ(evt1_machine.last_event(), EVT2);
    EXPECT_EQ(evt1_machine.state(), State::DONE_EVT2);
}

TEST_F(TestStateMachine, test_statemachine_naming)
{
    StateMachine<State, Event> machine(FIRST);
    this->add_names(machine);
    EXPECT_EQ(machine.event_name(machine.last_event()), "");
    EXPECT_EQ(machine.event_name(Event::EVT1), "EVT1");
    EXPECT_EQ(machine.event_name(Event::EVT2), "EVT2");
    EXPECT_EQ(machine.event_name(Event::EVT_SUCCESS), "EVT_SUCCESS");
    EXPECT_EQ(machine.event_name(Event::EVT_ERROR), "EVT_ERROR");

    EXPECT_EQ(machine.state(), State::FIRST);
    EXPECT_EQ(machine.state_name(State::BEFORE_EVT1), "BEFORE_EVT1");
    EXPECT_EQ(machine.state_name(State::DONE_EVT1), "DONE_EVT1");
    EXPECT_EQ(machine.state_name(State::DONE_EVT2), "DONE_EVT2");
    EXPECT_EQ(machine.state_name(State::Error), "Error");
}

TEST_F(TestStateMachine, test_statemachine_bulk_init)
{
    // A machine built via set_transitions_map must behave identically to one built add_transition by add_transition
    using SM = StateMachine<State, Event>;
    std::unordered_map<uint64_t, State> transitions = {
        {SM::pack_key(State::FIRST, Event::EVT1), State::BEFORE_EVT1},
        {SM::pack_key(State::BEFORE_EVT1, Event::EVT_SUCCESS), State::DONE_EVT1},
        {SM::pack_key(State::BEFORE_EVT1, Event::EVT_ERROR), State::Error},
        {SM::pack_key(State::DONE_EVT1, Event::EVT2), State::DONE_EVT2},
    };

    StateMachine<State, Event> machine(FIRST);
    machine.set_transitions_map(transitions);

    EXPECT_FALSE(machine.transition(Event::EVT2));
    EXPECT_EQ(machine.state(), State::FIRST);

    EXPECT_TRUE(machine.transition(Event::EVT1));
    EXPECT_EQ(machine.state(), State::BEFORE_EVT1);

    EXPECT_TRUE(machine.transition(Event::EVT_SUCCESS));
    EXPECT_EQ(machine.state(), State::DONE_EVT1);

    EXPECT_TRUE(machine.transition(Event::EVT2));
    EXPECT_EQ(machine.state(), State::DONE_EVT2);
}

TEST_F(TestStateMachine, test_statemachine_copy)
{
    // A copied machine must carry over all transitions and reach the same states
    StateMachine<State, Event> original(FIRST);
    this->add_transitions(original);
    this->add_names(original);

    original.transition(EVT1);
    EXPECT_EQ(original.state(), BEFORE_EVT1);

    StateMachine<State, Event> copy = original;

    // copy starts in the same state as original at the moment of copy
    EXPECT_EQ(copy.state(), BEFORE_EVT1);
    EXPECT_EQ(copy.last_event(), EVT1);

    // transitions from original are preserved in copy
    EXPECT_TRUE(copy.transition(EVT_SUCCESS));
    EXPECT_EQ(copy.state(), DONE_EVT1);

    // original is unaffected
    EXPECT_EQ(original.state(), BEFORE_EVT1);

    // name maps are preserved
    EXPECT_EQ(copy.state_name(DONE_EVT1), "DONE_EVT1");
    EXPECT_EQ(copy.event_name(EVT_SUCCESS), "EVT_SUCCESS");
}
} // namespace test
