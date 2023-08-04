
DTS_FILE=pps-gpio

# parse overlay-name with escape chars into sed -n "s/^.*=\s*\(.*\);$/\1/p"
OVERLAY_NAME := $(shell grep 'overlay-name' $(PWD)/$(DTS_FILE).dts | sed -n "s/^.*=\\s*\\(.*\\);$\/\\1/p")
$(warning OVERLAY_NAME=$(OVERLAY_NAME))
overlay_hdr2_cnt := $(shell sudo /opt/nvidia/jetson-io/config-by-hardware.py -l | grep 'Header 2' | wc -l)
ifeq ($(overlay_hdr2_cnt), 1)
	TARGET_OVERLAY_HEADER := 2
else
	TARGET_OVERLAY_HEADER := 1
endif
$(warning TARGET_OVERLAY_HEADER=$(TARGET_OVERLAY_HEADER))


obj-m := adlink-pps-gpio.o tegra194_gte_test.o

.PHONY: all
all: modules dtbo

modules:
	make -C /lib/modules/`uname -r`/build M=`pwd` modules

dtbo:
	dtc -O dtb -o $(DTS_FILE).dtbo -@ $(DTS_FILE).dts
	sudo cp -rf $(DTS_FILE).dtbo /boot
	sudo /opt/nvidia/jetson-io/config-by-hardware.py -n $(TARGET_OVERLAY_HEADER)=$(OVERLAY_NAME)
	
clean:
	rm -rf *.o *.ko *.mod.* *.symvers *.order *.mod.cmd *.mod .*.cmd