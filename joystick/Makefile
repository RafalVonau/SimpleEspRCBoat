all : image.elf
FW_FILE_1:=0x00000.bin
FW_FILE_2:=0x40000.bin


BOOT ?= none
# APP = 0 - eagle.flash.bin + eagle.irom0text.bin
# APP = 1 - user1.bin
# APP = 2 - user2.bin
APP ?= 0
# SPI_SPEED = 40, 26, 20, 80
SPI_SPEED ?= 40
# SPI_MODE: QIO, QOUT, DIO, DOUT
SPI_MODE ?= QIO
# SPI_SIZE_MAP
# 0 : 512 KB (256 KB + 256 KB)
# 1 : 256 KB
# 2 : 1024 KB (512 KB + 512 KB)
# 3 : 2048 KB (512 KB + 512 KB)
# 4 : 4096 KB (512 KB + 512 KB)
# 5 : 2048 KB (1024 KB + 1024 KB)
# 6 : 4096 KB (1024 KB + 1024 KB)
SPI_SIZE_MAP ?= 4

TARGET_OUT:=image.elf
OBJS:=driver/uart.o \
	driver/gpio16.o \
	driver/pwm.o \
	user/mystuff.o \
	user/user_main.o

SRCS:=driver/uart.c \
	driver/gpio16.c \
	driver/pwm.c \
	user/mystuff.c \
	user/user_main.c

GCC_FOLDER:=/opt/esp-open-sdk/xtensa-lx106-elf
ESPTOOL_PY:=/opt/esp-open-sdk/esptool/esptool.py

ESPTOOL?=/opt/esp-open-sdk/esptool/esptool.py
ESPPORT?=/dev/ttyUSB0
FW_TOOL?=/opt/esp-open-sdk/esptool/esptool.py
SDK:=/opt/esp-open-sdk/sdk
#SDK:=~/GIT/GIT/mcu/nodemcu/nodemcu-firmware/sdk/esp_iot_sdk_v1.4.0
PORT:=/dev/ttyUSB0
#PORT:=/dev/ttyACM0

XTLIB:=$(SDK)/lib
XTGCCLIB:=$(GCC_FOLDER)/lib/gcc/xtensa-lx106-elf/4.8.2/libgcc.a
FOLDERPREFIX:=$(GCC_FOLDER)/bin
PREFIX:=$(FOLDERPREFIX)/xtensa-lx106-elf-
CC:=$(PREFIX)gcc

CFLAGS:=-mlongcalls -I$(SDK)/include -Imyclib -Iinclude -Iuser -Os -I$(SDK)/include/ -DICACHE_FLASH

#CFLAGS = -I. -Iinclude -I$(SDK)/include -D__ets__ -DICACHE_FLASH -gdwarf-2 -O2
#CFLAGS += -Wpointer-arith -Wundef -Werror -fno-strict-aliasing -fno-inline-functions -fno-jump-tables
#CFLAGS += -falign-functions=4 -nostdlib -mlongcalls -mtext-section-literals -Wno-unused-but-set-variable

#	   \
#

LDFLAGS_CORE:=\
	-nostdlib \
	-Wl,--relax -Wl,--gc-sections \
	-L$(XTLIB) \
	-L$(XTGCCLIB) \
	$(SDK)/lib/liblwip.a \
	$(SDK)/lib/libssl.a \
	$(SDK)/lib/libupgrade.a \
	$(SDK)/lib/libnet80211.a \
	$(SDK)/lib/liblwip.a \
	$(SDK)/lib/libwpa.a \
	$(SDK)/lib/libnet80211.a \
	$(SDK)/lib/libphy.a \
	$(SDK)/lib/libmain.a \
	$(SDK)/lib/libpp.a \
	$(SDK)/lib/libcrypto.a \
	$(XTGCCLIB) \
	-T $(SDK)/ld/eagle.app.v6.ld

LINKFLAGS:= \
	$(LDFLAGS_CORE) \
	-B$(XTLIB)

#image.elf : $(OBJS)
#	$(PREFIX)ld $^ $(LDFLAGS) -o $@

$(TARGET_OUT) : $(SRCS)
	$(PREFIX)gcc $(CFLAGS) $^  -flto $(LINKFLAGS) -o $@



$(FW_FILE_1): $(TARGET_OUT)
	@echo "FW $@"
	$(FW_TOOL) elf2image $<

$(FW_FILE_2): $(TARGET_OUT)
	@echo "FW $@"
#	$(FW_TOOL) elf2image $<

burn : $(FW_FILE_1) $(FW_FILE_2)
	$(ESPTOOL_PY) --port $(PORT) write_flash 0x00000 image.elf-0x00000.bin 0x40000 image.elf-0x40000.bin
#	($(ESPTOOL_PY) --port $(PORT) write_flash -fm dio -fs 32m -ff 40m 0x00000 image.elf-0x00000.bin 0x40000 image.elf-0x40000.bin)||(true)

burn : $(FW_FILE_1) $(FW_FILE_2)
	$(ESPTOOL_PY) --port $(PORT) write_flash 0x00000 image.elf-0x00000.bin 0x40000 image.elf-0x40000.bin

burn4m : $(FW_FILE_1) $(FW_FILE_2)
	$(ESPTOOL_PY) --port $(PORT) write_flash --flash_size 32m 0x00000 image.elf-0x00000.bin 0x40000 image.elf-0x40000.bin

burn4m_all : $(FW_FILE_1) $(FW_FILE_2)
	$(ESPTOOL_PY) --port $(PORT) write_flash --flash_size 32m \
	0x00000 image.elf-0x00000.bin \
	0x40000 image.elf-0x40000.bin \
	0x07c000 ../16kblank.bin \
	0x0fc000 ../16kblank.bin \
	0x3fc000 ../16kblank.bin \
	0x3fc000 $(SDK)/bin/esp_init_data_default.bin

burn_all : $(FW_FILE_1) $(FW_FILE_2)
	$(ESPTOOL_PY) --port $(PORT) write_flash \
	0x00000 image.elf-0x00000.bin \
	0x40000 image.elf-0x40000.bin \
	0x07c000 ../16kblank.bin \
	0x07c000 $(SDK)/bin/esp_init_data_default.bin

burn4m_zero : $(FW_FILE_1) $(FW_FILE_2)
	$(ESPTOOL_PY) --port $(PORT) write_flash --flash_size 32m \
	0x07c000 ../16kblank.bin \
	0x0fc000 ../16kblank.bin \
	0x3fc000 ../16kblank.bin \
	0x3fc000 $(SDK)/bin/esp_init_data_default.bin


clean :
	rm -rf user/*.o driver/*.o $(TARGET_OUT) $(FW_FILE_1) $(FW_FILE_2)


