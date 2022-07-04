ifneq (${TERM},)
_tput_red = $(echo -e "\033[0;31m")
_tput_green = $(echo -e "\033[0;32m")
_tput_yellow = $(echo -e "\033[0;33m")
_tput_reset = $(echo -e "\033[0m")
_mk_tput_red = $(shell echo -e "\033[0;32m")
_mk_tput_green = $(shell echo -e "\033[0;32m")
_mk_tput_yellow = $(shell echo -e "\033[0;32m")
_mk_tput_reset = $(shell echo -e "\033[0m")
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