local log = sihd.util.log

-- emitting at every level
log.emergency("emergency")
log.alert("alert")
log.critical("critical")
log.error("error")
log.warning("warning")
log.notice("notice")
log.info("info")
log.debug("debug")

-- sink configuration: swap SIHD's logger output from a config script
log.clear()
log.console()
log.clear()
log.stream()
