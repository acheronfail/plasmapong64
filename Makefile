ADVECTION_CHAIN ?= 1
ADVECTION_PIPELINE ?= 1
GRADIENT_PIPELINE ?= 1
CPU_LTO ?= 1
TRACE_ROWS ?= 3
CONFINEMENT_CHAIN ?= 1
PROJECTION_CHAIN ?= 1
PIXELS_CHAIN ?= 1
HIRES_FONT_FORMAT ?= RGBA16
SPLAT_PLAN ?= 1
SOUND_STEADY ?= 1
PRESSURE_VECTOR_SUM ?= 1
AUDIO_STREAM ?= 1
EXPANSION_BANKS ?= 0
SPEED_RSP ?= 1
GRADIENT_RSP ?= 1
PREPARE_RSP ?= 1
FLUID_RSP ?= 1
DYE_RSP ?= 1
DYE_FIXED ?= $(DYE_RSP)
VELOCITY_RSP ?= 1
VELOCITY_FIXED ?= $(VELOCITY_RSP)
CONFINEMENT_RSP ?= 1
CONFINEMENT_FIXED ?= $(CONFINEMENT_RSP)
# Keep default objects separate from the old CPU build and comparison builds.
ifeq ($(CONFINEMENT_RSP),1)
BUILD_DIR ?= build/rsp_confinement
ROM ?= plasmapong
else ifeq ($(CONFINEMENT_FIXED),1)
BUILD_DIR ?= build/confinement_cpu
ROM ?= plasmapong-confinement-cpu
endif
BUILD_DIR ?= build/$(if $(filter 1,$(VELOCITY_RSP)),rsp_velocity,$(if $(filter 1,$(VELOCITY_FIXED)),velocity_cpu,$(if $(filter 1,$(DYE_RSP)),rsp_dye,$(if $(filter 1,$(DYE_FIXED)),dye_cpu,$(if $(filter 1,$(FLUID_RSP)),rsp,cpu)))))
ROM ?= $(if $(filter 1,$(VELOCITY_RSP)),plasmapong-float-confinement,$(if $(filter 1,$(VELOCITY_FIXED)),plasmapong-velocity-cpu,$(if $(filter 1,$(DYE_RSP)),plasmapong-float-velocity,$(if $(filter 1,$(DYE_FIXED)),plasmapong-dye-cpu,plasmapong-float))))
.DEFAULT_GOAL := all
ifeq ($(N64_INST),)
$(error N64_INST is unset. Use ./tools/build-rom.sh or install libdragon)
endif
include $(N64_INST)/include/n64.mk
ifeq ($(PIXELS_CHAIN),1)
N64_CFLAGS += -DPLASMAPONG_PIXELS_CHAIN
endif
ifeq ($(CPU_LTO),1)
N64_CFLAGS += -flto
# n64.mk links through g++; keep these as driver flags, not -Wl options.
N64_CXXFLAGS += -flto
endif
src := src/main.c src/game.c src/arcade.c src/fluid.c src/fluid_advection.c src/ui.c src/sound.c src/save.c src/save_n64.c
ifeq ($(EXPANSION_BANKS),1)
src += src/expansion_n64.c
N64_LDFLAGS += --wrap malloc_uncached_aligned
endif
ifeq ($(FLUID_RSP),1)
# This pinned n64.mk does not sanitize hyphens in embedded ucode symbols.
ifneq ($(findstring -,$(BUILD_DIR)),)
$(error FLUID_RSP requires BUILD_DIR without hyphens; use build/rsp or build/rsp_benchmark)
endif
src += src/fluid_rsp.c
rsp_obj := $(BUILD_DIR)/src/rsp_fluid.o
N64_CFLAGS += -DPLASMAPONG_FLUID_RSP
endif
ifeq ($(DYE_FIXED),1)
src += src/fluid_dye_fixed.c
N64_CFLAGS += -DPLASMAPONG_DYE_FIXED
endif
ifeq ($(DYE_RSP),1)
ifneq ($(DYE_FIXED),1)
$(error DYE_RSP=1 requires DYE_FIXED=1)
endif
ifneq ($(FLUID_RSP),1)
$(error DYE_RSP=1 requires FLUID_RSP=1)
endif
src += src/fluid_dye_rsp.c
rsp_obj += $(BUILD_DIR)/src/rsp_dye.o
N64_CFLAGS += -DPLASMAPONG_DYE_RSP
endif
ifeq ($(VELOCITY_FIXED),1)
ifneq ($(DYE_FIXED),1)
$(error VELOCITY_FIXED=1 requires DYE_FIXED=1)
endif
src += src/fluid_velocity_fixed.c
N64_CFLAGS += -DPLASMAPONG_VELOCITY_FIXED
endif
ifeq ($(VELOCITY_RSP),1)
ifneq ($(DYE_RSP),1)
$(error VELOCITY_RSP=1 requires DYE_RSP=1)
endif
ifneq ($(VELOCITY_FIXED),1)
$(error VELOCITY_RSP=1 requires VELOCITY_FIXED=1)
endif
N64_CFLAGS += -DPLASMAPONG_VELOCITY_RSP
endif
ifeq ($(CONFINEMENT_FIXED),1)
ifneq ($(VELOCITY_FIXED),1)
$(error CONFINEMENT_FIXED=1 requires VELOCITY_FIXED=1)
endif
src += src/fluid_confinement.c
N64_CFLAGS += -DPLASMAPONG_CONFINEMENT_FIXED
endif
ifeq ($(CONFINEMENT_RSP),1)
ifneq ($(findstring -,$(BUILD_DIR)),)
$(error CONFINEMENT_RSP requires BUILD_DIR without hyphens)
endif
ifneq ($(CONFINEMENT_FIXED),1)
$(error CONFINEMENT_RSP=1 requires CONFINEMENT_FIXED=1)
endif
src += src/fluid_confinement_rsp.c
rsp_obj += $(BUILD_DIR)/src/rsp_confinement.o
N64_CFLAGS += -DPLASMAPONG_CONFINEMENT_RSP
ifeq ($(CONFINEMENT_CHAIN),1)
N64_CFLAGS += -DPLASMAPONG_CONFINEMENT_CHAIN
endif
endif
ifeq ($(PREPARE_RSP)$(VELOCITY_RSP),11)
src += src/fluid_prepare_rsp.c
rsp_obj += $(BUILD_DIR)/src/rsp_prepare.o
N64_CFLAGS += -DPLASMAPONG_PREPARE_RSP
ifeq ($(PROJECTION_CHAIN)$(FLUID_RSP)$(GRADIENT_RSP),111)
N64_CFLAGS += -DPLASMAPONG_PROJECTION_CHAIN
endif
ifeq ($(ADVECTION_CHAIN),1)
N64_CFLAGS += -DPLASMAPONG_ADVECTION_CHAIN
endif
ifeq ($(GRADIENT_RSP),1)
N64_CFLAGS += -DPLASMAPONG_GRADIENT_RSP
endif
ifeq ($(SPEED_RSP),1)
N64_CFLAGS += -DPLASMAPONG_SPEED_RSP
endif
endif
ifeq ($(CONFINEMENT_TEST),1)
ifneq ($(CONFINEMENT_FIXED),1)
$(error CONFINEMENT_TEST=1 requires CONFINEMENT_FIXED=1)
endif
N64_CFLAGS += -DPLASMAPONG_CONFINEMENT_TEST
endif
ifeq ($(VELOCITY_TEST),1)
ifneq ($(VELOCITY_FIXED),1)
$(error VELOCITY_TEST=1 requires VELOCITY_FIXED=1)
endif
N64_CFLAGS += -DPLASMAPONG_VELOCITY_TEST
endif
ifeq ($(RSP_TEST),1)
ifneq ($(FLUID_RSP),1)
$(error RSP_TEST=1 requires FLUID_RSP=1)
endif
N64_CFLAGS += -DPLASMAPONG_RSP_TEST
endif
N64_CFLAGS += -Wall -Wextra -Werror
ifeq ($(AUDIO_STREAM),1)
N64_CFLAGS += -DPLASMAPONG_AUDIO_STREAM
endif
ifneq ($(filter $(TRACE_ROWS),1 3),$(TRACE_ROWS))
$(error TRACE_ROWS must be 1 or 3)
endif
N64_RSPASFLAGS += -DPLASMAPONG_TRACE_ROWS=$(TRACE_ROWS)
ifeq ($(ADVECTION_PIPELINE),1)
N64_RSPASFLAGS += -DPLASMAPONG_ADVECTION_PIPELINE
endif
ifeq ($(GRADIENT_PIPELINE),1)
N64_RSPASFLAGS += -DPLASMAPONG_GRADIENT_PIPELINE
endif
ifeq ($(PRESSURE_VECTOR_SUM),1)
N64_RSPASFLAGS += -DPLASMAPONG_PRESSURE_VECTOR_SUM
endif
ifeq ($(SPLAT_PLAN),1)
N64_CFLAGS += -DPLASMAPONG_SPLAT_PLAN
endif
ifeq ($(SOUND_STEADY),1)
N64_CFLAGS += -DPLASMAPONG_SOUND_STEADY
$(BUILD_DIR)/src/sound.o: CFLAGS += -O3
endif
ifeq ($(RDP_VALIDATE),1)
N64_CFLAGS += -DPLASMAPONG_RDP_VALIDATE
endif
ifeq ($(ADVECTION_TEST),1)
N64_CFLAGS += -DPLASMAPONG_ADVECTION_TEST
endif
ifeq ($(DYE_TEST),1)
ifneq ($(DYE_RSP),1)
$(error DYE_TEST=1 requires DYE_RSP=1)
endif
N64_CFLAGS += -DPLASMAPONG_DYE_TEST
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
# Inline the fluid sampling loops without expanding the rest of the ROM.
$(BUILD_DIR)/src/fluid.o $(BUILD_DIR)/src/fluid_advection.o $(BUILD_DIR)/src/fluid_dye_fixed.o $(BUILD_DIR)/src/fluid_velocity_fixed.o $(BUILD_DIR)/src/fluid_confinement.o: CFLAGS += -O3
$(BUILD_DIR)/src/game.o $(BUILD_DIR)/src/ui.o: CFLAGS += -O3
$(BUILD_DIR)/src/main.o: CFLAGS += -O3
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
$(BUILD_DIR)/$(ROM).dfs: $(BUILD_DIR)/filesystem/at01-2x.font64
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
