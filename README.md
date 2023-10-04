# The Test Drivers for GPIO Interrupt and GTE on NVIDIA Jetson Orin

This repo includes the below drivers:
- **adlink-base-gpio** - Driver for base gpio (from TCA953x IO expander)
- **adlink-pps-gpio** - Driver for PPS-in and PPS-out
- **adlink-fsync-gpio** - Driver for 4 FPGA trigger pins
- **tegra194_gte_test** - Test NVIDIA GTE (Generic Timestamp Engine) https://docs.nvidia.com/jetson/archives/r35.4.1/DeveloperGuide/text/SD/Kernel/GenericTimestampEngine.html

    - The Limitations of GTE
    
        GTE gives timestamp using TSC (Time Stamps Counter @ 31.25Mhz) which is a free running counter. It starts as soon as system boots. Therefore, the timestamp is different to Linux system time.
        
        GTE only supports AON GPIO and Legacy Interrupt Controller. It doesn't support Main GPIO.


## Build Code

It will also compile the device tree and configure the overlay.

```bash
cd gpio_interrupt_test/src
make

# Please reboot so that the new device tree will take effect.
sudo reboot
```

## Load Driver 

```bash
cd gpio_interrupt_test/src

# for adlink-base-gpio driver
sudo insmod adlink-base-gpio.ko

# for adlink-pps-gpio driver
sudo insmod adlink-pps-gpio.ko

# for adlink-fsync-gpio driver
sudo insmod adlink-fsync-gpio.ko

# for tegra192_gte_test driver, the gpio pin mapping can be found at `sudo cat /sys/kernel/debug/gpio`
sudo insmod tegra194_gte_test.ko lic_irq=25 gpio_in=314 gpio_out=313
```

## Unload driver

```bash
cd gpio_interrupt_test/
sudo rmmod adlink-base-gpio
sudo rmmod adlink-pps-gpio
sudo rmmod adlink-fsync-gpio
sudo rmmod tegra194_gte_test
```

## Evaluation the result

1. cat /proc/interrupts
2. sudo cat /sys/kernel/debug/gpio
3. dmesg

## Troubleshooting

The interrupt from base-gpio may not be triggered automatically, you have to keep polling the GPIO status.
For example:

```bash
watch -n 0.1 sudo cat /sys/kernel/debug/gpio
```