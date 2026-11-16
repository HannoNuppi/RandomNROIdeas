#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. See the README.")
endif

TOPDIR ?= $(CURDIR)
STB_IMAGE_HEADER := $(TOPDIR)/third_party/stb/stb_image.h
include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
# Application settings
#---------------------------------------------------------------------------------
TARGET      := RandomNROIdeas
BUILD       := build
SOURCES     := source
DATA        := data
INCLUDES    := include third_party/stb

APP_TITLE   := Random NRO Ideas
APP_AUTHOR  := HannoNuppi
APP_VERSION := 0.1.0

#---------------------------------------------------------------------------------
# Switch compiler options
#---------------------------------------------------------------------------------
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS   := -g -Wall -Wextra -O2 -ffunction-sections $(ARCH) $(DEFINES)
CFLAGS   += $(INCLUDE) -D__SWITCH__
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS  := -g $(ARCH)
LDFLAGS  = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS    := -lnx
LIBDIRS := $(PORTLIBS) $(LIBNX)

#---------------------------------------------------------------------------------
# Build rules (based on the switchbrew application template)
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(CURDIR)/$(TARGET)
export TOPDIR  := $(CURDIR)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                  $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
  export LD := $(CC)
else
  export LD := $(CXX)
endif

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES     := $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(APP_ICON)),)
  ifneq ($(wildcard $(TARGET).jpg),)
    export APP_ICON := $(TOPDIR)/$(TARGET).jpg
  else ifneq ($(wildcard icon.jpg),)
    export APP_ICON := $(TOPDIR)/icon.jpg
  endif
endif

ifeq ($(strip $(APP_ICON)),)
  export APP_ICON := $(LIBNX)/default_icon.jpg
endif

ifeq ($(strip $(NO_ICON)),)
  export NROFLAGS += --icon=$(APP_ICON)
endif

ifeq ($(strip $(NO_NACP)),)
  export NROFLAGS += --nacp=$(CURDIR)/$(TARGET).nacp
endif

ifneq ($(APP_TITLEID),)
  export NACPFLAGS += --titleid=$(APP_TITLEID)
endif

ifneq ($(strip $(ROMFS)),)
  export NROFLAGS += --romfsdir=$(CURDIR)/$(ROMFS)
endif


$(STB_IMAGE_HEADER):
	@bash $(TOPDIR)/scripts/fetch-stb-image.sh
	@test -s $@

.PHONY: all clean
all: $(BUILD)

$(BUILD): $(STB_IMAGE_HEADER)
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $@ -f $(CURDIR)/Makefile

clean:
	@echo "Cleaning build output..."
	@rm -rf $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf

else

.PHONY: all
DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).nro
$(OUTPUT).nro: $(STB_IMAGE_HEADER)

ifeq ($(strip $(NO_NACP)),)
$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp
else
$(OUTPUT).nro: $(OUTPUT).elf
endif

$(OUTPUT).elf: $(OFILES)
$(OFILES_SRC): $(HFILES_BIN)

%.bin.o %_bin.h: %.bin
	@echo "Embedding $<"
	@$(bin2o)

-include $(DEPENDS)

endif
#---------------------------------------------------------------------------------
