# Delegate to the three standalone benchmark suites.
.DEFAULT_GOAL := run
.NOTPARALLEL:
.PHONY: all setup build run parsers hashmaps production http report report-parsers report-hashmaps report-production report-http request response list check clean nosimd sse2 sse42 neon
all: build
setup build list clean:
	@$(MAKE) -C parsers $@
	@$(MAKE) -C hashmaps $@
	@$(MAKE) -C production $@
check:
	@PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s shared/tests -p "test_*.py" -v
	@$(MAKE) -C parsers check
	@$(MAKE) -C hashmaps check
	@$(MAKE) -C production check
run: parsers hashmaps production
parsers:
	@$(MAKE) -C parsers run
hashmaps:
	@$(MAKE) -C hashmaps run
production http:
	@$(MAKE) -C production run
report: report-parsers report-hashmaps report-production
report-parsers:
	@$(MAKE) -C parsers report
report-hashmaps:
	@$(MAKE) -C hashmaps report
report-production report-http:
	@$(MAKE) -C production report
request response nosimd sse2 sse42 neon:
	@$(MAKE) -C parsers $@
