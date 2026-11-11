.SUFFIXES:
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment.")
endif
TOPDIR ?= $(CURDIR)

APP_NAME	:= Canopy Dino
APP_SHORTNAME	:= Canopy Dino
APP_AUTHOR	:= Oliver
APP_ICON	:= $(TOPDIR)/icon.png
APP_TV_SPLASH	:= $(TOPDIR)/splash_tv.png
APP_DRC_SPLASH	:= $(TOPDIR)/splash_drc.png

include $(DEVKITPRO)/wut/share/wut_rules

TARGET		:=	CanopyDino
BUILD		:=	build
SOURCES		:=	src
INCLUDES	:=	src

CFLAGS	:=	-g -O2 -ffunction-sections -Wall -Wno-missing-field-initializers $(MACHDEP)
CFLAGS	+=	$(INCLUDE) -D__WIIU__ -D__WUT__ -DSAVE_PATH=\"fs:/vol/external01/wiiu/apps/CanopyDino/save.txt\" $(shell $(PORTLIBS_PATH)/wiiu/bin/powerpc-eabi-pkg-config --cflags sdl2)
ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map)
LIBS	:= $(shell $(PORTLIBS_PATH)/wiiu/bin/powerpc-eabi-pkg-config --libs sdl2) -lwut -lm
LIBDIRS	:= $(PORTLIBS) $(WUT_ROOT)

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
export LD	:=	$(CC)
export OFILES 	:=	$(CFILES:.c=.o)
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)
.PHONY: $(BUILD) clean all
all: $(BUILD)
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
clean:
	@rm -fr $(BUILD) $(TARGET).wuhb $(TARGET).rpx $(TARGET).elf
else
.PHONY:	all
DEPENDS	:=	$(OFILES:.o=.d)
all	:	$(OUTPUT).wuhb
$(OUTPUT).wuhb	:	$(OUTPUT).rpx
$(OUTPUT).rpx	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)
-include $(DEPENDS)
endif
