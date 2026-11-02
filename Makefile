include site_scons/sbt/makefile/sbt.mk

tsan_test: TSAN_OPTIONS="halt_on_error=0:second_deadlock_stack=1:history_size=7:suppressions=$(CURDIR)/scripts/tsan.supp"