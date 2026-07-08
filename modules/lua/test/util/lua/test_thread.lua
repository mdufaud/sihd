local util = sihd.util
local time = util.time
local log = util.log

local function one_second()
    return util.Duration(time.sec(1))
end

log.info("Waitable: notify wakes a pending wait_for")
do
    local w = util.Waitable()
    local notified = false
    local sched = util.Scheduler("wait_notifier", nil)
    -- reschedule the notifier so a notify delivered before wait_for is entered
    -- (missed wakeup) is retried until the waiter is actually parked
    sched:add_task({
        task = function()
            notified = true
            w:notify_all()
            return true
        end,
        run_in = 0,
        reschedule_time = time.ms(1),
    })
    sched:start()
    -- bounded wait: releases the GIL so the scheduler thread runs and notifies
    w:wait_for(one_second())
    sched:stop()
    assert(notified, "scheduler task should have signalled the waitable")
end

log.info("Waitable: raw nanosecond and userdata time arguments are equivalent")
do
    local w = util.Waitable()
    -- un-notified waitable times out (returns true) in both argument forms
    assert(w:wait_for(time.ms(1)) == true, "raw-ns wait_for should time out")
    assert(w:wait_for(util.Duration(time.ms(1))) == true, "userdata wait_for should time out")
    -- a past absolute timestamp times out immediately in both argument forms
    assert(w:wait_until(1) == true, "raw-ns wait_until(past) should time out")
    assert(w:wait_until(util.Timestamp(1)) == true, "userdata wait_until(past) should time out")
end

log.info("Scheduler: rescheduling task runs while main thread allocates Lua objects")
do
    local w = util.Waitable()
    local called = 0
    local sched = util.Scheduler("scheduler", nil)
    sched:add_task({
        task = function()
            called = called + 1
            if called >= 10 then
                w:notify_all()
            end
            return true
        end,
        run_in = 0,
        reschedule_time = time.ms(2),
    })

    sched:start()
    -- hammer the shared universe from the main thread at the same time
    local root = util.Node("root", nil)
    for i = 1, 100 do
        util.Named("named" .. i, root)
    end
    w:wait_for(one_second())
    sched:stop()

    assert(called >= 10, "rescheduling task should have run at least 10 times")
end

log.info("Scheduler: rescheduling task keeps running across a stop/restart cycle")
do
    local w = util.Waitable()
    local runs = 0
    local sched = util.Scheduler("restart_sched", nil)
    sched:add_task({
        task = function()
            runs = runs + 1
            if runs % 5 == 0 then
                w:notify_all()
            end
            return true
        end,
        run_in = 0,
        reschedule_time = time.ms(1),
    })

    sched:start()
    w:wait_for(one_second())
    sched:stop()
    local after_first = runs
    assert(after_first >= 5, "task should run before the first stop")

    -- restart the same scheduler: the surviving rescheduling task must be
    -- re-pointed at the freshly created worker coroutine and run again
    sched:start()
    w:wait_for(one_second())
    sched:stop()
    assert(runs > after_first, "task must keep running after a restart")
end

log.info("Scheduler: task added while running gets executed")
do
    local w = util.Waitable()
    local sched = util.Scheduler("add_while_running", nil)
    -- keep the scheduler busy so the added task is inserted mid-run
    sched:add_task({
        task = function() return true end,
        run_in = 0,
        reschedule_time = time.ms(1),
    })
    sched:start()
    time.msleep(2)

    local late_called = false
    sched:add_task({
        task = function()
            late_called = true
            w:notify_all()
            return false
        end,
        run_in = 0,
    })
    w:wait_for(one_second())
    sched:stop()

    assert(late_called, "task added while running must execute")
end

log.info("Scheduler: two schedulers run concurrently on the shared universe")
do
    local w = util.Waitable()
    local a, b = 0, 0
    local sched_a = util.Scheduler("sched_a", nil)
    local sched_b = util.Scheduler("sched_b", nil)
    sched_a:add_task({
        task = function()
            a = a + 1
            if a >= 10 and b >= 10 then w:notify_all() end
            return true
        end,
        run_in = 0,
        reschedule_time = time.ms(1),
    })
    sched_b:add_task({
        task = function()
            b = b + 1
            if a >= 10 and b >= 10 then w:notify_all() end
            return true
        end,
        run_in = 0,
        reschedule_time = time.ms(1),
    })

    sched_a:start()
    sched_b:start()
    w:wait_for(one_second())
    sched_a:stop()
    sched_b:stop()

    assert(a >= 10 and b >= 10, "both schedulers should have run concurrently")
end

log.info("Worker: runs the function once")
do
    local worker_called = false
    local worker = util.Worker(function()
        worker_called = true
    end)

    worker:start_sync_worker("worker")
    time.msleep(1)
    worker:stop_worker()

    assert(worker_called, "worker function should have run")
end

log.info("StepWorker: loops at frequency with pause/resume")
do
    local w = util.Waitable()
    local steps = 0
    local stepworker = util.StepWorker(function()
        steps = steps + 1
        if steps % 10 == 0 then
            w:notify(1)
        end
        return true
    end)
    assert(stepworker:set_frequency(1000.0), "1000 Hz frequency should be accepted")

    stepworker:start_sync_worker("stepworker")
    w:wait_for(one_second())

    stepworker:pause_worker()
    local paused_at = steps
    time.msleep(10)
    -- while paused the step function must not advance
    assert(steps == paused_at, "paused StepWorker must not step")

    stepworker:resume_worker()
    w:wait_for(one_second())
    stepworker:stop_worker()

    assert(steps > paused_at, "StepWorker must resume stepping after resume")
    assert(steps >= 20, "StepWorker should have stepped many times")
end

log.info("all threading mechanisms passed")
