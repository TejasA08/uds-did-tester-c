CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2 -Iinclude
LDFLAGS ?=

SRC = \
	src/main.c \
	src/hexutil.c \
	src/config_csv.c \
	src/bus.c \
	src/isotp.c \
	src/pcan_bus.c \
	src/runner.c \
	src/report.c

OBJ = $(patsubst src/%.c,build/%.o,$(SRC))
BIN = build/uds_tester
SELFCHECK_SRC = \
	src/selfcheck.c \
	src/hexutil.c \
	src/config_csv.c \
	src/bus.c \
	src/isotp.c \
	src/pcan_bus.c \
	src/runner.c \
	src/report.c
SELFCHECK_OBJ = $(patsubst src/%.c,build/%.o,$(SELFCHECK_SRC))
SELFCHECK_BIN = build/selfcheck

# Optional Peak build (Windows with PCAN-Basic SDK):
#   make pcan PCAN_SDK="C:/Program Files/PEAK-System/PCAN-Basic API"
# Expects PCANBasic.h and PCANBasic.lib under that tree.
ifdef PCAN_SDK
CFLAGS += -DUDS_HAS_PCAN -I"$(PCAN_SDK)/Include"
LDFLAGS += -L"$(PCAN_SDK)/x64/VC_LIB" -lPCANBasic
endif

.PHONY: all clean test run-mock config dirs

all: dirs $(BIN)

dirs:
	@mkdir -p build config reports

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(SELFCHECK_BIN): $(SELFCHECK_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/%.o: src/%.c include/uds_tester.h src/bus_internal.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c -o $@ $<

config: $(BIN)
	./$(BIN) --create-config

test: dirs $(SELFCHECK_BIN)
	./$(SELFCHECK_BIN)

run-mock: $(BIN) config/setup.csv config/test_cases.csv
	./$(BIN) --mock

config/setup.csv config/test_cases.csv:
	@mkdir -p config
	./$(BIN) --create-config

clean:
	rm -rf build build_selfcheck
