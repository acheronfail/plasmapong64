# One production fluid solver; tune its parameters without selecting old backends.
ifneq ($(FLUID_PRESET),)
$(error FLUID_PRESET was removed; use the official defaults)
endif
GRID_W ?= 64
GRID_H ?= 44
CELL_Q4 ?= $(if $(filter 64,$(GRID_W)),72,96)
PRESSURE_PASSES ?= 1
FLOW_DAMPING ?= .16f
BUILD_DIR ?= build/official64
ROM ?= plasmapong
CPU_LTO ?= 1
PIXELS_CHAIN ?= 1
HIRES_FONT_FORMAT ?= RGBA16
AUDIO_STREAM ?= 1
SOUND_STEADY ?= 1
EXPANSION_BANKS ?= 1
SPEED_RSP ?= 1
MENU_STAMPS ?= 1
MENU_LABEL_BLOCK ?= $(if $(filter 1,$(FRAME_BLOCK)),0,1)
FLUID_HIGHPRI ?= 1
FLUID_HIGHPRI_YIELD ?= 1

.DEFAULT_GOAL := all
ifeq ($(N64_INST),)
$(error N64_INST is unset. Use ./tools/build-rom.sh or install libdragon)
endif
include $(N64_INST)/include/n64.mk
N64_CFLAGS += -DPLASMAPONG_GRID_W=$(GRID_W) -DPLASMAPONG_GRID_H=$(GRID_H) -DPLASMAPONG_CELL_Q4=$(CELL_Q4)
N64_RSPASFLAGS += -DPLASMAPONG_GRID_W=$(GRID_W) -DPLASMAPONG_GRID_H=$(GRID_H) -DPLASMAPONG_CELL_Q4=$(CELL_Q4)
ifneq ($(filter $(GRID_W),48 64),$(GRID_W))
$(error GRID_W currently supports 48 or 64)
endif
ifeq ($(GRID_W),64)
ifneq ($(GRID_H)$(CELL_Q4),4472)
$(error GRID_W=64 requires GRID_H=44 CELL_Q4=72)
endif
else
ifneq ($(GRID_H)$(CELL_Q4),3396)
$(error GRID_W=48 requires GRID_H=33 CELL_Q4=96)
endif
endif
ifeq ($(filter $(PRESSURE_PASSES),1 2 3 4 5 6 7 8),)
$(error PRESSURE_PASSES must be between 1 and 8)
endif
N64_CFLAGS += -DPLASMAPONG_FLOW_DAMPING=$(FLOW_DAMPING) -DPLASMAPONG_PRESSURE_PASSES=$(PRESSURE_PASSES)
N64_RSPASFLAGS += -DPLASMAPONG_PRESSURE_PASSES=$(PRESSURE_PASSES)
N64_CFLAGS += -DPLASMAPONG_VELOCITY_CHAIN -DPLASMAPONG_UPWIND_RSP -DPLASMAPONG_FLUID_RSP -DPLASMAPONG_PREPARE_RSP -DPLASMAPONG_PROJECTION_CHAIN
N64_RSPASFLAGS += -DPLASMAPONG_VELOCITY_CHAIN
ifeq ($(PIXELS_CHAIN),1)
N64_CFLAGS += -DPLASMAPONG_PIXELS_CHAIN
endif
ifeq ($(CPU_LTO),1)
N64_CFLAGS += -flto
# n64.mk links through g++; keep these as driver flags, not -Wl options.
N64_CXXFLAGS += -flto
endif
src := src/main.c src/game.c src/arcade.c src/fluid.c src/ui.c src/sound.c src/music.c src/save.c src/save_n64.c \
       src/fluid_dye_fixed.c src/fluid_upwind.c src/fluid_upwind_rsp.c src/fluid_rsp.c \
       src/fluid_prepare_rsp.c src/fluid_gradient_short_rsp.c
rsp_obj := $(addprefix $(BUILD_DIR)/src/,rsp_fluid.o rsp_upwind.o rsp_prepare.o rsp_gradient_short.o)
# This pinned n64.mk does not sanitize hyphens in embedded ucode symbols.
ifneq ($(findstring -,$(BUILD_DIR)),)
$(error BUILD_DIR must not contain hyphens)
endif
ifeq ($(RDP_TAIL_SYNC),1)
src += src/rdpq_tail_sync.c
N64_CFLAGS += -I/libdragon/src/rdpq
endif
ifeq ($(EXPANSION_BANKS),1)
src += src/expansion_n64.c
N64_LDFLAGS += --wrap malloc_uncached_aligned
endif
ifeq ($(SPEED_RSP),1)
N64_CFLAGS += -DPLASMAPONG_SPEED_RSP
endif
ifeq ($(RSP_TEST),1)
N64_CFLAGS += -DPLASMAPONG_RSP_TEST
endif
N64_CFLAGS += -Wall -Wextra -Werror
ifeq ($(UPWIND_TEST),1)
N64_CFLAGS += -DPLASMAPONG_UPWIND_TEST
endif
ifeq ($(PREPARE_TEST),1)
N64_CFLAGS += -DPLASMAPONG_PREPARE_TEST
endif
ifeq ($(AUDIO_STREAM),1)
N64_CFLAGS += -DPLASMAPONG_AUDIO_STREAM
endif
ifeq ($(SOUND_STEADY),1)
N64_CFLAGS += -DPLASMAPONG_SOUND_STEADY
$(BUILD_DIR)/src/sound.o: CFLAGS += -O3
endif
ifeq ($(RDP_VALIDATE),1)
N64_CFLAGS += -DPLASMAPONG_RDP_VALIDATE
endif
ifeq ($(DRAW_SYNC_PROFILE),1)
N64_CFLAGS += -DPLASMAPONG_DRAW_SYNC_PROFILE
endif
ifeq ($(FRAME_WORK_PROFILE),1)
N64_CFLAGS += -DPLASMAPONG_FRAME_WORK_PROFILE
endif
ifeq ($(FLUID_PROFILE),1)
N64_CFLAGS += -DPLASMAPONG_FLUID_PROFILE
endif
ifeq ($(QUEUE_PROFILE),1)
ifneq ($(FLUID_PROFILE),1)
$(error QUEUE_PROFILE=1 requires FLUID_PROFILE=1)
endif
N64_CFLAGS += -DPLASMAPONG_QUEUE_PROFILE
endif
ifeq ($(QUEUE_PC_PROFILE),1)
ifneq ($(QUEUE_PROFILE),1)
$(error QUEUE_PC_PROFILE=1 requires QUEUE_PROFILE=1)
endif
N64_CFLAGS += -DPLASMAPONG_QUEUE_PC_PROFILE
endif
ifeq ($(RDP_WAIT_TRACE),1)
ifneq ($(QUEUE_PC_PROFILE),1)
$(error RDP_WAIT_TRACE=1 requires QUEUE_PC_PROFILE=1)
endif
N64_CFLAGS += -DPLASMAPONG_RDP_WAIT_TRACE
endif
ifeq ($(SC64_RELOAD),1)
src += src/sc64_reload.c
N64_CFLAGS += -DPLASMAPONG_SC64_RELOAD
ifneq ($(USB_LOG),1)
$(error SC64_RELOAD=1 requires USB_LOG=1 for cartridge detection)
endif
endif
ifeq ($(USB_LOG),1)
N64_CFLAGS += -DPLASMAPONG_USB_LOG
endif
ifeq ($(BENCH_MENU),1)
N64_CFLAGS += -DPLASMAPONG_BENCH_MENU
endif
ifeq ($(BENCH_FLAT_FLUID),1)
ifneq ($(BENCH_MENU),1)
$(error BENCH_FLAT_FLUID=1 is a diagnostic requiring BENCH_MENU=1)
endif
N64_CFLAGS += -DPLASMAPONG_BENCH_FLAT_FLUID
endif
ifeq ($(HIRES_VI_POINT),1)
N64_CFLAGS += -DPLASMAPONG_HIRES_VI_POINT
endif
ifeq ($(DRAW_STREAM),1)
N64_CFLAGS += -DPLASMAPONG_DRAW_STREAM
endif
ifneq ($(MENU_BUFFER_KIB),)
ifneq ($(MENU_LABEL_BLOCK),1)
$(error MENU_BUFFER_KIB requires MENU_LABEL_BLOCK=1)
endif
# Isolated experiment against our pinned libdragon's internal allocation state.
N64_CFLAGS += -DPLASMAPONG_MENU_BUFFER_KIB=$(MENU_BUFFER_KIB) -I/libdragon/src/rdpq
endif
ifeq ($(FRAME_BLOCK),1)
N64_CFLAGS += -DPLASMAPONG_FRAME_BLOCK
endif
ifeq ($(MENU_TEMPLATE),1)
ifneq ($(BENCH_MENU),1)
$(error MENU_TEMPLATE=1 is an experiment requiring BENCH_MENU=1)
endif
ifeq ($(FRAME_BLOCK),1)
$(error MENU_TEMPLATE=1 cannot use FRAME_BLOCK=1)
endif
ifneq ($(filter 1,$(DRAW_STREAM) $(INK16)),)
$(error MENU_TEMPLATE=1 requires the cached RGBA32 renderer)
endif
N64_CFLAGS += -DPLASMAPONG_MENU_TEMPLATE -I/libdragon/src/rdpq
endif
ifeq ($(INK16),1)
N64_CFLAGS += -DPLASMAPONG_INK16
endif
ifeq ($(INK16_RSP),1)
ifneq ($(SPEED_RSP)$(PIXELS_CHAIN),11)
$(error INK16_RSP=1 requires SPEED_RSP=1 PIXELS_CHAIN=1)
endif
N64_CFLAGS += -DPLASMAPONG_INK16 -DPLASMAPONG_INK16_RSP
endif
ifeq ($(MENU_STAMPS),1)
N64_CFLAGS += -DPLASMAPONG_MENU_STAMPS
endif
ifeq ($(MENU_LABEL_BLOCK),1)
ifeq ($(FRAME_BLOCK),1)
$(error MENU_LABEL_BLOCK=1 cannot record inside FRAME_BLOCK=1)
endif
N64_CFLAGS += -DPLASMAPONG_MENU_LABEL_BLOCK
endif
ifeq ($(FLUID_HIGHPRI),1)
ifeq ($(QUEUE_PROFILE),1)
$(error FLUID_HIGHPRI=1 cannot use the low-priority QUEUE_PROFILE syncpoint)
endif
N64_CFLAGS += -DPLASMAPONG_FLUID_HIGHPRI
endif
ifeq ($(FLUID_HIGHPRI_YIELD),1)
ifneq ($(FLUID_HIGHPRI),1)
$(error FLUID_HIGHPRI_YIELD=1 requires FLUID_HIGHPRI=1)
endif
N64_CFLAGS += -DPLASMAPONG_FLUID_HIGHPRI_YIELD
endif
# Inline the fluid sampling loops without expanding the rest of the ROM.
$(BUILD_DIR)/src/fluid.o $(BUILD_DIR)/src/fluid_dye_fixed.o: CFLAGS += -O3
$(BUILD_DIR)/src/game.o $(BUILD_DIR)/src/ui.o: CFLAGS += -O3
$(BUILD_DIR)/src/main.o: CFLAGS += -O3
ifneq ($(SMOKE_MENU_VIEW),)
N64_CFLAGS += -DPLASMAPONG_SMOKE -DPLASMAPONG_SMOKE_MENU_VIEW=$(SMOKE_MENU_VIEW)
endif
ifeq ($(SMOKE_OPTIONS),1)
N64_CFLAGS += -DPLASMAPONG_SMOKE -DPLASMAPONG_SMOKE_OPTIONS
endif
ifeq ($(SMOKE_FLOW),1)
N64_CFLAGS += -DPLASMAPONG_SMOKE -DPLASMAPONG_SMOKE_FLOW
endif
ifeq ($(SMOKE_VIDEO),1)
N64_CFLAGS += -DPLASMAPONG_SMOKE -DPLASMAPONG_SMOKE_VIDEO
endif
ifneq ($(SMOKE_EFFECT),)
N64_CFLAGS += -DPLASMAPONG_SMOKE_EFFECT=$(SMOKE_EFFECT)
endif
ifneq ($(SMOKE_FPS),)
# Legacy benchmark selector: 30 means high res, 60 means low res.
N64_CFLAGS += -DPLASMAPONG_SMOKE_FPS=$(SMOKE_FPS)
endif
ifeq ($(SMOKE_HIGH_RES),1)
N64_CFLAGS += -DPLASMAPONG_SMOKE_HIGH_RES
endif
ifeq ($(SMOKE),1)
SMOKE_PLAYERS ?= 2
N64_CFLAGS += -DPLASMAPONG_SMOKE -DPLASMAPONG_SMOKE_PLAYERS=$(SMOKE_PLAYERS)
endif
ifeq ($(SMOKE_STRESS),1)
N64_CFLAGS += -DPLASMAPONG_SMOKE_STRESS
endif
ifeq ($(SMOKE_ARCADE),1)
SMOKE_LEVEL ?= 1
N64_CFLAGS += -DPLASMAPONG_SMOKE_ARCADE -DPLASMAPONG_SMOKE_LEVEL=$(SMOKE_LEVEL)
endif
ifeq ($(SMOKE_SAVE),1)
N64_CFLAGS += -DPLASMAPONG_SAVE_SMOKE
endif
all: $(ROM).z64
$(BUILD_DIR)/filesystem/at01-2x.font64: assets/fonts/at01-2x.fnt assets/fonts/at01-2x.png
	mkdir -p $(BUILD_DIR)/filesystem
	$(N64_MKFONT) --format $(HIRES_FONT_FORMAT) -o $(BUILD_DIR)/filesystem $<
$(BUILD_DIR)/filesystem/%.pcm: assets/audio/music/%.pcm
	mkdir -p $(BUILD_DIR)/filesystem
	cp $< $@
$(BUILD_DIR)/$(ROM).dfs: $(BUILD_DIR)/filesystem/at01-2x.font64 $(BUILD_DIR)/filesystem/intro_loop.pcm $(BUILD_DIR)/filesystem/battle_loop.pcm
	$(N64_MKDFS) $@ $(BUILD_DIR)/filesystem
# Rebuild cached objects when default compiler/ucode switches change.
$(src:%.c=$(BUILD_DIR)/%.o) $(rsp_obj): Makefile
$(BUILD_DIR)/$(ROM).elf: $(src:%.c=$(BUILD_DIR)/%.o) $(rsp_obj)
$(ROM).z64: $(BUILD_DIR)/$(ROM).dfs
$(ROM).z64: N64_ROM_SAVETYPE=eeprom4k
$(ROM).z64: N64_ROM_TITLE="Plasma Pong 64"
clean:
	rm -rf $(BUILD_DIR) $(ROM).z64 plasmapong-smoke.z64 plasmapong-arcade-smoke.z64 plasmapong-save-smoke.z64 plasmapong-rsp.z64 plasmapong-rsp-benchmark.z64 plasmapong-dye.z64 plasmapong-dye-benchmark.z64
-include $(wildcard $(BUILD_DIR)/src/*.d)
.PHONY: all clean
