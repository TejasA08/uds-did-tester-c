CC ?= gcc
CFLAGS ?= -O2 -Wall -Iinclude -Ithird_party/miniz
LDFLAGS ?=

APP_SRC = \
	src/main.c \
	src/hexutil.c \
	src/config_csv.c \
	src/xlsx_config.c \
	src/bus.c \
	src/isotp.c \
	src/pcan_bus.c \
	src/runner.c \
	src/report.c

MINIZ_SRC = \
	third_party/miniz/miniz.c \
	third_party/miniz/miniz_tdef.c \
	third_party/miniz/miniz_tinfl.c \
	third_party/miniz/miniz_zip.c

SRC = $(APP_SRC) $(MINIZ_SRC)
OBJ = $(patsubst src/%.c,build/%.o,$(filter src/%.c,$(APP_SRC))) \
	$(patsubst third_party/miniz/%.c,build/miniz_%.o,$(MINIZ_SRC))

BIN = build/uds_tester

SELFCHECK_APP = \
	src/selfcheck.c \
	src/hexutil.c \
	src/config_csv.c \
	src/xlsx_config.c \
	src/bus.c \
	src/isotp.c \
	src/pcan_bus.c \
	src/runner.c \
	src/report.c

SELFCHECK_OBJ = $(patsubst src/%.c,build/%.o,$(SELFCHECK_APP)) \
	$(patsubst third_party/miniz/%.c,build/miniz_%.o,$(MINIZ_SRC))
SELFCHECK_BIN = build/selfcheck

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

build/%.o: src/%.c include/uds_tester.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c -o $@ $<

build/miniz_%.o: third_party/miniz/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c -o $@ $<

config: $(BIN)
	./$(BIN) --create-config

test: dirs $(SELFCHECK_BIN)
	./$(SELFCHECK_BIN)

run-mock: $(BIN) config/test_cases.xlsx
	./$(BIN) --mock

config/test_cases.xlsx:
	@mkdir -p config
	./$(BIN) --create-config

clean:
	rm -rf build build_selfcheck
