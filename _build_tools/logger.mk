red = $(shell tput setaf 1)
green = $(shell tput setaf 2)
yellow = $(shell tput setaf 3)
reset = $(shell tput sgr0)

log = $(shell date "+[%Y-%m-%d %H:%M:%S] [$1] [$2]: $3")

log_info = $(info $(green)$(call log,info,$1,$2)$(reset))
log_warning = $(info $(yellow)$(call log,warning,$1,$2)$(reset))
log_error = $(info $(red)$(call log,error,$1,$2)$(reset))