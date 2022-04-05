local sched = sihd.util.Scheduler("scheduler", nil)
local waitable = sihd.util.Waitable()

local called = 0
sched:add_task({
    task = function()
        called = called + 1
        if (called >= 10) then
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

sihd.util.log.info("testing single Scheduler with processing to mess with stack")

local before = sched:get_clock():now()

sched:start()

local root = sihd.util.Node("root", nil)
for i = 1,100 do
    sihd.util.Named("named" .. i, root)
end

-- waiting for 10 call or 1 sec
waitable:wait_for(sihd.util.time.sec(1))
sched:stop()

local after = sched:get_clock():now()

print("process time: " .. sihd.util.time.to_double(after - before))
print("total called: " .. called)

-- can be 11 sometimes
assert(called >= 10)

sihd.util.log.info("testing two Schedulers with added task while started")

local second_sched = sihd.util.Scheduler("second_sched", nil)
second_sched:add_task({
    task = function()
        local task_op = 1
        task_op = task_op + 1
    end,
    -- when possible
    when = 0,
    -- every 1 ms
    reschedule = sihd.util.time.ms(1)
})
-- reset first scheduler
called = 0

second_sched:start()
sched:start()

sihd.util.time.msleep(2)

local new_task_called = false
second_sched:add_task({
    task = function()
        new_task_called = true
    end,
    when = 0
})

waitable:wait_for(sihd.util.time.sec(1))

second_sched:stop()
sched:stop()

print("total called: " .. called)
print("new task called: " .. tostring(new_task_called))
assert(called >= 10)
assert(new_task_called)
assert(task_op == nil)

sihd.util.log.info("testing Worker")

local worker_called = false
local worker = sihd.util.Worker(function()
    worker_called = true
end)

worker:start_sync_worker("thread")
sihd.util.time.msleep(1)
worker:stop_worker()

assert(worker_called)

sihd.util.log.info("testing StepWorker")

local steps = 0
local stepworker = sihd.util.StepWorker(function()
    steps = steps + 1
    waitable:notify(1)
    return true
end)
-- 1ms
assert(stepworker:set_conf({ frequency = 1000.0 }))

stepworker:start_sync_worker()
-- 10 ms
waitable:wait_loop(sihd.util.time.sec(1), 10)
stepworker:pause_worker()

sihd.util.time.msleep(10)

stepworker:resume_worker()
-- 10 ms
waitable:wait_loop(sihd.util.time.sec(1), 10)
stepworker:stop_worker()

print("total steps: " .. steps)
assert(steps >= 20)