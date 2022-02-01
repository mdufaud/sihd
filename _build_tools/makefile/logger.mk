ifneq (, $(shell which tput))
_tput_red = `tput setaf 1`
_tput_green = `tput setaf 2`
_tput_yellow = `tput setaf 3`
_tput_reset = `tput sgr0`
_makefile_tput_red = $(shell tput setaf 1)
_makefile_tput_green = $(shell tput setaf 2)
_makefile_tput_yellow = $(shell tput setaf 3)
_makefile_tput_reset = $(shell tput sgr0)
else
_tput_red =
_tput_green =
_tput_yellow =
_tput_reset =
_makefile_tput_red =
_makefile_tput_green =
_makefile_tput_yellow =
_makefile_tput_reset =
endif

log_echo = date "+[%Y-%m-%d %H:%M:%S] [$1] [$2]: $3"
log_echo_info = echo -e "$(_tput_green)`$(call log_echo,info,$1,$2)`$(_tput_reset)"
log_echo_warning = echo -e "$(_tput_yellow)`$(call log_echo,warning,$1,$2)`$(_tput_reset)"
log_echo_error = echo -e "$(_tput_red)`$(call log_echo,error,$1,$2)`$(_tput_reset)"

log = $(shell date "+[%Y-%m-%d %H:%M:%S] [$1] [$2]: $3")
log_info = $(info $(_makefile_tput_green)$(call log,info,$1,$2)$(_makefile_tput_reset))
log_warning = $(info $(_makefile_tput_yellow)$(call log,warning,$1,$2)$(_makefile_tput_reset))
log_error = $(info $(_makefile_tput_red)$(call log,error,$1,$2)$(_makefile_tput_reset))