ifneq (, $(shell which tput))
_tput_red = `tput setaf 1`
_tput_green = `tput setaf 2`
_tput_yellow = `tput setaf 3`
_tput_reset = `tput sgr0`
_mk_tput_red = $(shell tput setaf 1)
_mk_tput_green = $(shell tput setaf 2)
_mk_tput_yellow = $(shell tput setaf 3)
_mk_tput_reset = $(shell tput sgr0)
else
_tput_red =
_tput_green =
_tput_yellow =
_tput_reset =
_mk_tput_red =
_mk_tput_green =
_mk_tput_yellow =
_mk_tput_reset =
endif

echo_log = `date "+[%Y-%m-%d %H:%M:%S] [$1] [$2]: $3"`
echo_log_info = echo -e "$(_tput_green)$(call echo_log,info,$1,$2)$(_tput_reset)"
echo_log_warning = echo -e "$(_tput_yellow)$(call echo_log,warning,$1,$2)$(_tput_reset)"
echo_log_error = echo -e "$(_tput_red)$(call echo_log,error,$1,$2)$(_tput_reset)"

mk_log = $(shell date "+[%Y-%m-%d %H:%M:%S] [$1] [$2]: $3")
mk_log_info = $(info $(_mk_tput_green)$(call mk_log,info,$1,$2)$(_mk_tput_reset))
mk_log_warning = $(info $(_mk_tput_yellow)$(call mk_log,warning,$1,$2)$(_mk_tput_reset))
mk_log_error = $(info $(_mk_tput_red)$(call mk_log,error,$1,$2)$(_mk_tput_reset))