FLUID_RSP ?= 1
# Keep default objects separate from the old CPU build and comparison builds.
BUILD_DIR ?= build/$(if $(filter 1,$(FLUID_RSP)),rsp,cpu)
ROM ?= plasmapong
.DEFAULT_GOAL := all
ifeq ($(N64_INST),)
$(error N64_INST is unset. Use ./tools/build-rom.sh or install libdragon)
endif
include $(N64_INST)/include/n64.mk
src := src/main.c src/game.c src/arcade.c src/fluid.c src/fluid_advection.c src/ui.c src/sound.c src/save.c src/save_n64.c
ifeq ($(FLUID_RSP),1)
# This pinned n64.mk does not sanitize hyphens in embedded ucode symbols.
ifneq ($(findstring -,$(BUILD_DIR)),)
$(error FLUID_RSP requires BUILD_DIR without hyphens; use build/rsp or build/rsp_benchmark)
endif
src += src/fluid_rsp.c
rsp_obj := $(BUILD_DIR)/src/rsp_fluid.o
N64_CFLAGS += -DPLASMAPONG_FLUID_RSP
endif
ifeq ($(RSP_TEST),1)
ifneq ($(FLUID_RSP),1)
$(error RSP_TEST=1 requires FLUID_RSP=1)
endif
N64_CFLAGS += -DPLASMAPONG_RSP_TEST
endif
N64_CFLAGS += -Wall -Wextra -Werror
ifeq ($(ADVECTION_TEST),1)
N64_CFLAGS += -DPLASMAPONG_ADVECTION_TEST
endif
ifeq ($(FLUID_PROFILE),1)
N64_CFLAGS += -DPLASMAPONG_FLUID_PROFILE
endif
# Inline the fluid sampling loops without expanding the rest of the ROM.
$(BUILD_DIR)/src/fluid.o $(BUILD_DIR)/src/fluid_advection.o: CFLAGS += -O3
ifeq ($(SMOKE),1)
N64_CFLAGS += -DPLASMAPONG_SMOKE
endif
ifeq ($(SMOKE_ARCADE),1)
SMOKE_LEVEL ?= 1
N64_CFLAGS += -DPLASMAPONG_SMOKE_ARCADE -DPLASMAPONG_SMOKE_LEVEL=$(SMOKE_LEVEL)
endif
ifeq ($(SMOKE_SAVE),1)
N64_CFLAGS += -DPLASMAPONG_SAVE_SMOKE
endif
all: $(ROM).z64
$(BUILD_DIR)/$(ROM).elf: $(src:%.c=$(BUILD_DIR)/%.o) $(rsp_obj)
$(ROM).z64: N64_ROM_SAVETYPE=eeprom4k
$(ROM).z64: N64_ROM_TITLE="Plasma Pong 64"
clean:
	rm -rf $(BUILD_DIR) $(ROM).z64 plasmapong-smoke.z64 plasmapong-arcade-smoke.z64 plasmapong-save-smoke.z64 plasmapong-rsp.z64 plasmapong-rsp-benchmark.z64
-include $(wildcard $(BUILD_DIR)/src/*.d)
.PHONY: all clean
