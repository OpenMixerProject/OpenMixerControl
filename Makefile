#
# Makefile
#

GITREF = $(shell git describe --abbrev=10 --dirty --always --tags)
DEPFLAGS = -MMD -MP

ifeq "$(BUILD_TARGET)" "TARGET_XM32"

	DEBUG           = 0
	COPTS           ?= -mcpu=arm926ej-s -Os -fno-caller-saves -pipe -funit-at-a-time -msoft-float -fno-plt -fno-unwind-tables -fno-asynchronous-unwind-tables
	CC              = arm-linux-gnueabi-gcc
	CXX             = arm-linux-gnueabi-g++
	AR              = arm-linux-gnueabi-ar
	LD  	        = arm-linux-gnueabi-ld
	BUILD_DIR       = $(ROOT_DIR)/build/xm32
	DEPFLAGS        += -D=TARGET_XM32 -D_GNU_SOURCE
	FLTO            = -flto=auto
	C_STD           = -std=c11
	LVGL_CONF       = files/lv_conf.h
	LDFLAGS_EXTRA	= 

endif

ifeq "$(BUILD_TARGET)" "TARGET_WING"

	DEBUG           = 0
	COPTS           = 
	CC              = arm-linux-gnueabihf-gcc
	CXX             = arm-linux-gnueabihf-g++
	AR              = arm-linux-gnueabihf-ar
	LD              = arm-linux-gnueabihf-ld
	BUILD_DIR       = $(ROOT_DIR)/build/wing
	DEPFLAGS        += -D=TARGET_WING -D_GNU_SOURCE
	FLTO            = 
	C_STD           = -std=c11
	LVGL_CONF       = files/lv_conf.h
	LDFLAGS_EXTRA	= 

endif

ifeq "$(BUILD_TARGET)" "TARGET_PC_SDL2"

	DEBUG           = 1
	COPTS           = 
	BUILD_DIR       = $(ROOT_DIR)/build/pc_sdl2
	DEPFLAGS        += -D=TARGET_PC_SDL2
	FLTO            =
	C_STD           = 
	LVGL_CONF       = files/lv_conf_SDL2.h
	LDFLAGS_EXTRA   = -lSDL2

endif

BIN             = omc
BUILD_OBJ_DIR   = $(BUILD_DIR)/obj
BUILD_BIN_DIR   = $(BUILD_DIR)

prefix          ?= /usr
bindir          ?= $(prefix)/bin

LIB_DIR			= lib
LIB_EXT_DIR		= lib_ext
LVGL_DIR        = $(LIB_EXT_DIR)/lvgl
GLAZE_DIR		= $(LIB_EXT_DIR)/glaze/include
LIBARTNET_DIR	= $(LIB_EXT_DIR)/libartnet
OSC_DIR			= $(LIB_EXT_DIR)/small-osc

WARNINGS        := -Wall -Wshadow -Wundef -Wextra -Wno-unused-function -Wno-error=strict-prototypes -Wpointer-arith \
                   -fno-strict-aliasing -Wno-error=cpp -Wuninitialized -Wmaybe-uninitialized -Wno-unused-parameter -Wno-missing-field-initializers -Wtype-limits \
                   -Wsizeof-pointer-memaccess -Wno-format-nonliteral -Wno-cast-qual -Wunreachable-code -Wno-switch-default -Wreturn-type -Wmultichar -Wformat-security \
                   -Wno-ignored-qualifiers -Wno-error=pedantic -Wno-sign-compare -Wno-error=missing-prototypes -Wdouble-promotion -Wclobbered -Wdeprecated -Wempty-body \
                   -Wshift-negative-value -Wstack-usage=2048 -Wno-unused-value

ifeq ($(DEBUG), 1)

	# debug build
	CFLAGS          ?= $(C_STD) -g -I$(LIB_DIR)/ -I$(LVGL_DIR)/ -I$(GLAZE_DIR)/ -I$(LIBARTNET_DIR)/ -I$(OSC_DIR)/ $(WARNINGS) $(DEPFLAGS)
	CXXFLAGS        ?= -std=c++23 -g -I$(LIB_DIR)/ -I$(LVGL_DIR)/ -I$(GLAZE_DIR)/ -I$(LIBARTNET_DIR)/ -I$(OSC_DIR)/ $(WARNINGS) $(DEPFLAGS)

else

	# normal build
	CFLAGS          ?= $(C_STD) $(FLTO) $(COPTS) -g0 -I$(LIB_DIR)/ -I$(LVGL_DIR)/ -I$(GLAZE_DIR)/ -I$(LIBARTNET_DIR)/ -I$(OSC_DIR)/ $(WARNINGS) $(DEPFLAGS)
	CXXFLAGS        ?= -std=c++23 $(FLTO) $(COPTS) -g0 -I$(LIB_DIR)/ -I$(LVGL_DIR)/ -I$(GLAZE_DIR)/ -I$(LIBARTNET_DIR)/ -I$(OSC_DIR)/ $(WARNINGS) $(DEPFLAGS)

endif


LDFLAGS         ?= $(FLTO) -lm -lrt -lpthread -L$(BUILD_OBJ_DIR) $(LDFLAGS_EXTRA)

# Collect source files recursively
CSRCS           := $(shell find src -type f -name '*.c' -print) \
		 	   	   $(shell find $(LIB_DIR) -type f -name '*.c' -print) \
				   $(shell find $(LVGL_DIR)/src -type f -name '*.c' -print) \
				   $(shell find $(LIBARTNET_DIR)/artnet -type f -name '*.c' -print) \
				   $(shell find $(OSC_DIR) -type f -name '*.c' ! -name "main.c" -print)
				   
CXXSRCS         := $(shell find src -type f -name '*.cpp' -print) \
				   $(shell find $(LIB_DIR) -type f -name '*.cpp' -print) \
				   $(shell find $(LVGL_DIR)/src -type f -name '*.cpp' -print) \
				   $(shell find $(LIBARTNET_DIR)/artnet -type f -name '*.cpp' -print) \
				   $(shell find $(OSC_DIR) -type f -name '*.cpp' -print)



all: versionfile copy default

OBJEXT          ?= .o

COBJS           = $(CSRCS:.c=$(OBJEXT))
CXXOBJS         = $(CXXSRCS:.cpp=$(OBJEXT))
AOBJS           = $(ASRCS:.S=$(OBJEXT))

SRCS            = $(ASRCS) $(CSRCS) $(CXXSRCS)
OBJS            = $(AOBJS) $(COBJS) $(CXXOBJS)
TARGET          = $(addprefix $(BUILD_OBJ_DIR)/, $(patsubst ./%, %, $(OBJS)))
DEPS            = $(TARGET:.o=.d)

$(BUILD_OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC)  $(CFLAGS) -c $< -o $@
	@echo "CC  $<"

$(BUILD_OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@$(CXX)  $(CXXFLAGS) -c $< -o $@
	@echo "CXX $<"

$(BUILD_OBJ_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@$(CC)  $(CFLAGS) -c $< -o $@
	@echo "AS  $<"

copy:
	cp files/Makefile.lvgl $(LVGL_DIR)/Makefile

	@if ! diff -q $(LVGL_CONF) $(LVGL_DIR)/lv_conf.h >/dev/null 2>&1; then \
		cp $(LVGL_CONF) $(LVGL_DIR)/lv_conf.h; \
		echo "Update $(LVGL_DIR)/lv_conf.h (changed content)"; \
	else \
		echo "$(LVGL_DIR)/lv_conf.h is up to date."; \
	fi

	@if ! diff -q files/libartnet_network.c $(LIBARTNET_DIR)/artnet/network.c >/dev/null 2>&1; then \
		cp files/libartnet_network.c $(LIBARTNET_DIR)/artnet/network.c; \
		echo "Update $(LIBARTNET_DIR)/artnet/network.c (changed content)"; \
	else \
		echo "$(LIBARTNET_DIR)/artnet/network.c is up to date."; \
	fi

default: $(TARGET)
	@mkdir -p $(dir $(BUILD_BIN_DIR)/)
	$(CXX) -o $(BUILD_BIN_DIR)/$(BIN) $(TARGET) $(LDFLAGS)

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)
	rm src/version.h

versionfile:
	@echo "#define GIT_VERSION \"$(GITREF)\"" > src/version.h

test:
	@echo "builddir: $(BUILD_DIR), build_target: $(BUILD_TARGET)"
