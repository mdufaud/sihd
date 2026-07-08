local util = sihd.util
local time = util.time
local log = util.log

local ROUNDS = 6          -- stop/restart cycles
local N_SCHED = 3         -- schedulers per round
local N_WORKER = 4        -- run-once workers per round
local N_STEP = 2          -- looping step-workers per round
local ROUND_WAIT = util.Duration(time.ms(60))

-- shared, mutated from every thread through the GIL
local total = 0
local per = {}

local function bump(name)
    total = total + 1
    per[name] = (per[name] or 0) + 1
end

local function sum_per()
    local s = 0
    for _, v in pairs(per) do
        s = s + v
    end
    return s
end

-- persistent schedulers/step-workers: created once, restarted every round so the
-- rescheduling-task-survives-restart path is exercised repeatedly under load
local schedulers = {}
for i = 1, N_SCHED do
    local name = "sched_" .. i
    local sched = util.Scheduler(name, nil)
    sched:add_task({
        task = function()
            bump(name)
            return true
        end,
        run_in = 0,
        reschedule_time = time.ms(1),
    })
    schedulers[i] = sched
end

local stepworkers = {}
for i = 1, N_STEP do
    local name = "step_" .. i
    local sw = util.StepWorker(function()
        bump(name)
        return true
    end)
    sw:set_frequency(2000.0) -- 0.5 ms
    stepworkers[i] = sw
end

log.info(string.format("stress: %d rounds x (%d schedulers + %d workers + %d step-workers)",
         ROUNDS, N_SCHED, N_WORKER, N_STEP))

for round = 1, ROUNDS do
    -- start the persistent long-lived mechanisms (restart path)
    for _, sched in ipairs(schedulers) do
        sched:start()
    end
    for _, sw in ipairs(stepworkers) do
        sw:start_sync_worker("stepworker")
    end

    -- fresh run-once workers each round
    local workers = {}
    for i = 1, N_WORKER do
        local name = "worker_" .. i
        local worker = util.Worker(function()
            bump(name)
        end)
        worker:start_sync_worker("worker")
        workers[i] = worker
    end

    -- main thread: allocate Lua while everything else mutates the universe,
    -- including a C++ Node tree, then force a full GC to stress lifetime
    local root = util.Node("stress_root_" .. round, nil)
    for i = 1, 200 do
        util.Named("n" .. i, root)
    end
    local garbage = {}
    for i = 1, 500 do
        garbage[i] = ("x"):rep(8) .. tostring(i)
    end
    collectgarbage("collect")

    -- add a task to a running scheduler mid-round
    schedulers[1]:add_task({
        task = function()
            bump("late_" .. round)
            return false
        end,
        run_in = 0,
    })

    -- let the mayhem run, then pause/resume the step-workers under load
    local half = util.Duration(time.ms(30))
    util.Waitable():wait_for(half)
    for _, sw in ipairs(stepworkers) do
        sw:pause_worker()
    end
    util.Waitable():wait_for(half)
    for _, sw in ipairs(stepworkers) do
        sw:resume_worker()
    end
    util.Waitable():wait_for(ROUND_WAIT)

    -- tear everything down for this round
    for _, worker in ipairs(workers) do
        worker:stop_worker()
    end
    for _, sw in ipairs(stepworkers) do
        sw:stop_worker()
    end
    for _, sched in ipairs(schedulers) do
        sched:stop()
    end
end

print("stress total increments: " .. total)
print("stress distinct sources : " .. tostring(sum_per()))

-- the load must actually have happened
assert(total > 0, "no work was executed")
-- GIL correctness: no lost/torn read-modify-write across all threads
assert(total == sum_per(),
       "shared-state corruption: total(" .. total .. ") ~= sum_per(" .. sum_per() .. ")")
-- every mechanism contributed
for i = 1, N_SCHED do
    assert((per["sched_" .. i] or 0) > 0, "scheduler " .. i .. " never ran")
end
for i = 1, N_STEP do
    assert((per["step_" .. i] or 0) > 0, "step-worker " .. i .. " never ran")
end
local worker_hits = 0
for i = 1, N_WORKER do
    worker_hits = worker_hits + (per["worker_" .. i] or 0)
end
assert(worker_hits >= ROUNDS, "run-once workers did not run every round")
-- the mid-run added tasks ran at least once
local late_hits = 0
for r = 1, ROUNDS do
    late_hits = late_hits + (per["late_" .. r] or 0)
end
assert(late_hits > 0, "no mid-run added task ran")

log.info(string.format("stress: survived %d rounds, %d increments, invariant holds", ROUNDS, total))
