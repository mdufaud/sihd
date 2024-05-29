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
    run_in = 0,
    -- every 2 ms
    reschedule_time = sihd.util.time.ms(2)
})

sihd.util.log.info("testing single Scheduler with processing to mess with stack")

local before = sched:now()

sched:start()

local root = sihd.util.Node("root", nil)
for i = 1,100 do
    sihd.util.Named("named" .. i, root)
end

-- waiting for 10 call or 1 sec
local timestamp = sihd.util.Timestamp(sihd.util.time.sec(1))
waitable:wait_for(timestamp)
sched:stop()

local after = sched:now()

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
    run_in = 0,
    -- every 1 ms
    reschedule_time = sihd.util.time.ms(1)
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
    run_in = 0
})

local timestamp = sihd.util.Timestamp(sihd.util.time.sec(1))
waitable:wait_for(timestamp)

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
    if steps > 1 and steps % 10 == 0 then
        waitable:notify(1)
    end
    return true
end)
-- 1ms
assert(stepworker:set_conf({ frequency = 1000.0 }))

stepworker:start_sync_worker()
-- 10 ms
local timestamp = sihd.util.Timestamp(sihd.util.time.sec(1))
waitable:wait_for(timestamp)
stepworker:pause_worker()

sihd.util.time.msleep(10)

stepworker:resume_worker()
-- 10 ms
local timestamp = sihd.util.Timestamp(sihd.util.time.sec(1))
waitable:wait_for(timestamp)
stepworker:stop_worker()

print("total steps: " .. steps)
assert(steps >= 20)