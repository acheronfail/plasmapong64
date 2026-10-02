BUILD_DIR ?= build
ROM ?= plasmapong
.DEFAULT_GOAL := all
ifeq ($(N64_INST),)
$(error N64_INST is unset. Use ./tools/build-rom.sh or install libdragon)
endif
include $(N64_INST)/include/n64.mk
src := src/main.c src/game.c src/fluid.c src/ui.c src/sound.c
N64_CFLAGS += -Wall -Wextra -Werror
# Inline the fluid sampling loops without expanding the rest of the ROM.
$(BUILD_DIR)/src/fluid.o: CFLAGS += -O3
ifeq ($(SMOKE),1)
N64_CFLAGS += -DPLASMAPONG_SMOKE
endif
all: $(ROM).z64
$(BUILD_DIR)/$(ROM).elf: $(src:%.c=$(BUILD_DIR)/%.o)
$(ROM).z64: N64_ROM_TITLE="Plasma Pong 64"
clean:
	rm -rf $(BUILD_DIR) $(ROM).z64 plasmapong-smoke.z64
-include $(wildcard $(BUILD_DIR)/src/*.d)
.PHONY: all clean
