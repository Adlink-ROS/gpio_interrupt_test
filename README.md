# GPIO Interrupt Test on NVIDIA Jetson Orin

There are two drivers in the repo:
- **adlink-pps-gpio** - Test GPIO Interrupt with customized top/bottom ISR
- **tegra194_gte_test** - Test NVIDIA GTE (Generic Timestamp Engine) https://docs.nvidia.com/jetson/archives/r35.4.1/DeveloperGuide/text/SD/Kernel/GenericTimestampEngine.html

    - The Limitations of GTE
    
        GTE gives timestamp using TSC (Time Stamps Counter @ 31.25Mhz) which is a free running counter. It starts as soon as system boots. Therefore, the timestamp is different to Linux system time.
        
        GTE only supports AON GPIO and Legacy Interrupt Controller. It doesn't support Main GPIO.


## Build Code

It will also compile the device tree and configure the overlay.

```bash
make

# Please reboot so that the new device tree will take effect.
sudo reboot
```

## Load Driver 

```bash
# for adlink-pps-gpio driver
sudo insmod adlink-pps-gpio.ko

# for tegra192_gte_test driver, the gpio pin mapping can be found at `sudo cat /sys/kernel/debug/gpio`
sudo insmod tegra194_gte_test.ko lic_irq=25 gpio_in=314 gpio_out=313
```

## Unload driver

```bash
sudo rmmod adlink-pps-gpio
sudo rmmod tegra194_gte_test
```

## Evaluation the result

1. cat /proc/interrupts
2. dmesg