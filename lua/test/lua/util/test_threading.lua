local sched = sihd.util.Scheduler("scheduler", nil)
local waitable = sihd.util.Waitable()

local called = 0

sched:add_task({
    task = function()
        called = called + 1
        if (called >= 10) then
            -- stop playing tasks
            sched:clear_tasks()
            -- unlock wait
            waitable:notify_all()
        end
        return true
    end,
    -- when possible
    when = 0,
    -- every 2 ms
    reschedule = sihd.util.time.ms(2)
})

local before = sched:get_clock():now()

sched:start()
-- lock wait for 1 second or until all tasks are played
waitable:wait_for(sihd.util.time.sec(1))
sched:stop()

local after = sched:get_clock():now()

print("process time: " .. sihd.util.time.to_double(after - before))
print("total called: " .. called)

assert(called >= 9)