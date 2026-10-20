local util = sihd.util
local time = util.time
local log = util.log

local function one_second()
    return util.Duration(time.sec(1))
end

log.info("Worker dropped without stop is joined at GC")
do
    local ran = false
    local wk = util.Worker(function() ran = true end)
    wk:start_sync_worker("forgotten_worker")
    -- drop the only reference without calling stop_worker
    wk = nil
    collectgarbage("collect")
    collectgarbage("collect")
    assert(ran, "the forgotten worker function should still have run")
end

log.info("Worker blocked on the GIL while its finalizer joins")
do
    local ran = false
    local wk = util.Worker(function()
        time.msleep(100)
        ran = true
    end)
    wk:start_sync_worker("slow_worker")
    util.Waitable():wait_for(util.Duration(time.ms(20)))
    wk = nil
    collectgarbage("collect")
    collectgarbage("collect")
    assert(ran, "the slow worker function should have finished before the join returned")
end

log.info("StepWorker blocked on the GIL while its finalizer joins")
do
    local ran = false
    local sw = util.StepWorker(function()
        time.msleep(100)
        ran = true
        return true
    end)
    sw:set_frequency(1000.0)
    sw:start_sync_worker("slow_step_worker")
    util.Waitable():wait_for(util.Duration(time.ms(20)))
    sw = nil
    collectgarbage("collect")
    collectgarbage("collect")
    assert(ran, "the slow step function should have finished before the join returned")
end

log.info("Running Scheduler blocked on the GIL while its finalizer joins")
do
    local ran = false
    local sched = util.Scheduler("slow_sched", nil)
    sched:add_task({
        task = function()
            time.msleep(100)
            ran = true
            return false
        end,
        run_in = 0,
    })
    sched:start()
    util.Waitable():wait_for(util.Duration(time.ms(20)))
    sched = nil
    collectgarbage("collect")
    collectgarbage("collect")
    assert(ran, "the slow scheduler task should have finished before the join returned")
end

log.info("Repeated create/destroy rounds with GC keep the universe consistent")
do
    for round = 1, 5 do
        local w = util.Waitable()
        local ticks = 0
        local sched = util.Scheduler("churn_sched_" .. round, nil)
        sched:add_task({
            task = function()
                ticks = ticks + 1
                if ticks >= 5 then w:notify_all() end
                return true
            end,
            run_in = 0,
            reschedule_time = time.ms(1),
        })
        local sw = util.StepWorker(function() return true end)
        sw:set_frequency(1000.0)

        sched:start()
        sw:start_sync_worker("churn_step_" .. round)
        w:wait_for(one_second())
        sched:stop()
        sw:stop_worker()

        assert(ticks >= 5, "round " .. round .. " task must have run before teardown")

        -- drop every reference and force the coroutine anchors to be released
        sched = nil
        sw = nil
        w = nil
        collectgarbage("collect")
    end
end

log.info("Task added while the scheduler is stopped runs after restart")
do
    local sched = util.Scheduler("stopped_add", nil)
    sched:start()
    sched:stop()

    local w = util.Waitable()
    local ran = false
    sched:add_task({
        task = function()
            ran = true
            w:notify_all()
            return false
        end,
        run_in = 0,
    })

    sched:start()
    w:wait_for(one_second())
    sched:stop()

    assert(ran, "task added while stopped must run after the scheduler restarts")
end

log.info("all ownership assertions passed")
return true
