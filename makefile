# Created by the Intel FPGA Monitor Program
# DO NOT MODIFY

############################################
# Global Defines
DEFINE_COMMA	:= ,

############################################
# Compilation Targets

# Programs
AS		:= nios2-elf-as
CC		:= nios2-elf-gcc
LD		:= nios2-elf-ld
OC		:= nios2-elf-objcopy
RM		:= rm -f

# Flags
USERCCFLAGS	:= -g -O1 -ffunction-sections -fverbose-asm -fno-inline -mno-cache-volatile
ARCHASFLAGS	:= --gstabs -I "C:/intelFPGA_lite/18.1/nios2eds/components/altera_nios2/sdk/inc/"
ARCHCCFLAGS	:= -mno-hw-mul -mno-hw-div -I "C:/intelFPGA_lite/18.1/nios2eds/components/altera_nios2/HAL/inc/" -DSYSTEM_BUS_WIDTH=32 -DALT_SINGLE_THREADED -D_JTAG_UART_BASE=4280291328u
ARCHLDFLAGS	:= --defsym nasys_stack_top=0x4000000 --defsym nasys_program_mem=0x200 --defsym nasys_data_mem=0x200 --section-start .exceptions=0x20 --section-start .reset=0x0 --section-start .reset=0x0 --section-start .exceptions=0x20
ARCHLDSCRIPT	:= --script="C:/intelFPGA_lite/18.1/University_Program/Monitor_Program/build/nios_cpp_build.ld"
ASFLAGS		:= $(ARCHASFLAGS)
CCFLAGS		:= -Wall -c $(USERCCFLAGS) $(ARCHCCFLAGS)
LDFLAGS		:= $(patsubst %, -Wl$(DEFINE_COMMA)%, $(ARCHLDFLAGS)) $(patsubst %, -Wl$(DEFINE_COMMA)%, $(ARCHLDSCRIPT))
OCFLAGS		:= -O srec


# Files
<<<<<<< Updated upstream
HDRS		:=
SRCS		:= src/sensors/adc.c src/sensors/light.c src/sensors/sensor_manager.c src/sensors/sound.c src/sensors/vibration.c src/storage/event_record.c src/storage/ring_buffer.c src/system/exception_handler.c src/system/interrupt.c src/system/system_state.c src/system/timer.c tests/test_sensor_console.c
OBJS		:= $(patsubst %, %.o, $(SRCS))
=======
HDRS :=
SRCS := src/display/alarm.c src/display/alarm_audio_driver.c src/display/led_hex.c src/display/vga.c src/event/event_classifier.c src/event/event_detector.c src/event/risk_level.c src/input/key_switch.c src/sensors/adc.c src/sensors/light.c src/sensors/sensor_manager.c src/sensors/sound.c src/sensors/vibration.c src/storage/event_history.c src/storage/event_record.c src/storage/ring_buffer.c src/system/exception_handler.c src/system/interrupt.c src/system/system_state.c src/system/timer.c src/main.c amp/lib/src/niosII_jtag_uart.c
OBJS := $(patsubst %, %.o, $(SRCS))

>>>>>>> Stashed changes

# Targets
COMPILE: adc.srec

adc.srec: adc.elf
	$(RM) $@
	$(OC) $(OCFLAGS) $< $@

adc.elf: $(OBJS)
	$(RM) $@
	$(CC) $(LDFLAGS) $(OBJS) -o $@

%.c.o: %.c $(HDRS)
	$(RM) $@
	$(CC) $(CCFLAGS) $< -o $@

%.s.o: %.s $(HDRS)
	$(RM) $@
	$(AS) $(ASFLAGS) $< -o $@

CLEAN: 
	$(RM) adc.srec adc.elf $(OBJS)

