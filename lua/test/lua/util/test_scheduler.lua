local sched = sihd.util.Scheduler("scheduler", nil)

local called = 0

sched:add_task({
    task = function()
        called = called + 1
    end,
    -- when possible
    when = 0,
    -- every 2 ms
    reschedule = sihd.util.time.ms(2)
})

local before = sched:get_clock():now()

sched:start()
sihd.util.time.msleep(20)
sched:stop()

local after = sched:get_clock():now()

print("process time: " .. sihd.util.time.to_double(after - before))
print("total called: " .. called)

assert(called >= 9)