CC ?= cc
CFLAGS ?= -O2 -g -Wall -Wextra
BUILD := .build
COMMON := malloc-lab/memlib.c
.PHONY: setup test demo benchmark
setup: $(BUILD)/trace-avl $(BUILD)/trace-list $(BUILD)/contracts-avl $(BUILD)/contracts-list
$(BUILD):
	mkdir -p $@
$(BUILD)/trace-avl: scripts/trace_runner.c malloc-lab/mm.c $(COMMON) | $(BUILD)
	$(CC) $(CFLAGS) -I malloc-lab $^ -o $@
$(BUILD)/trace-list: scripts/trace_runner.c malloc-lab/baseline.c $(COMMON) | $(BUILD)
	$(CC) $(CFLAGS) -I malloc-lab $^ -o $@
$(BUILD)/contracts-avl: tests/contracts.c malloc-lab/mm.c $(COMMON) | $(BUILD)
	$(CC) $(CFLAGS) -I malloc-lab $^ -o $@
$(BUILD)/contracts-list: tests/contracts.c malloc-lab/baseline.c $(COMMON) | $(BUILD)
	$(CC) $(CFLAGS) -I malloc-lab $^ -o $@
test: setup
	$(BUILD)/contracts-avl
	$(BUILD)/contracts-list
	python3 scripts/run_traces.py
benchmark: test
demo: setup
	$(BUILD)/trace-avl malloc-lab/traces/short1-bal.rep --verbose
	$(BUILD)/trace-list malloc-lab/traces/short1-bal.rep --verbose
