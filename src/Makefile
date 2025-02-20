
DTS_FILE=adlink-gpio
DTS_PPS_GEN_FILE=adlink-pps-gen-gpio
DTS_I210=adlink-pps-ieee1588

# parse overlay-name with escape chars into sed -n "s/^.*=\s*\(.*\);$/\1/p"
OVERLAY_NAME := $(shell grep 'overlay-name' $(PWD)/$(DTS_FILE).dts | sed -n "s/^.*=\\s*\\(.*\\);$\/\\1/p")
# $(warning OVERLAY_NAME=$(OVERLAY_NAME))
OVERLAY_PPS_GEN_NAME := $(shell grep 'overlay-name' $(PWD)/$(DTS_PPS_GEN_FILE).dts | sed -n "s/^.*=\\s*\\(.*\\);$\/\\1/p")
# $(warning OVERLAY_NAME=$(OVERLAY_PPS_GEN_NAME))
OVERLAY_I210_NAME := 'PPS_IEEE1588 By I210 Device Tree Overlay'

overlay_hdr2_cnt := $(shell sudo /opt/nvidia/jetson-io/config-by-hardware.py -l | grep 'Header 2' | wc -l)
ifeq ($(overlay_hdr2_cnt), 1)
	TARGET_OVERLAY_HEADER := 2
else
	TARGET_OVERLAY_HEADER := 1
endif
# $(warning TARGET_OVERLAY_HEADER=$(TARGET_OVERLAY_HEADER))


obj-m := adlink-base-gpio.o adlink-fsync-gpio.o adlink-pps-gpio.o adlink-pps-mcu.o adlink-pps-gen-gpio.o adlink-pps-i210.o
#rqx-fpga.o
#tegra194_gte_test.o

.PHONY: all
all: modules dtbo

pps-gen: modules pps-gen-dtbo

pps-gen-dtbo:
	dtc -O dtb -o $(DTS_PPS_GEN_FILE).dtbo -@ $(DTS_PPS_GEN_FILE).dts
	sudo cp -rf $(DTS_PPS_GEN_FILE).dtbo /boot
	sudo /opt/nvidia/jetson-io/config-by-hardware.py -n $(TARGET_OVERLAY_HEADER)=$(OVERLAY_PPS_GEN_NAME)

modules:
	make -C /lib/modules/`uname -r`/build M=`pwd` modules

dtbo:
	dtc -O dtb -o $(DTS_FILE).dtbo -@ $(DTS_FILE).dts
	sudo cp -rf $(DTS_FILE).dtbo /boot
	sudo /opt/nvidia/jetson-io/config-by-hardware.py -n $(TARGET_OVERLAY_HEADER)=$(OVERLAY_NAME)
	
i210-pps: modules
	dtc -O dtb -o $(DTS_I210).dtbo -@ $(DTS_I210).dts
	sudo cp -rf $(DTS_I210).dtbo /boot
	sudo /opt/nvidia/jetson-io/config-by-hardware.py -n $(TARGET_OVERLAY_HEADER)=$(OVERLAY_I210_NAME)

clean:
	rm -rf *.o *.ko *.mod.* *.symvers *.order *.mod.cmd *.mod .*.cmd
