#!/bin/bash

# gpio_in=325 // PBB.01
sudo insmod ./tegra194_gte_test.ko lic_irq=25 gpio_in=325 gpio_out=441

# enable GTE
sudo su -c "echo 1 > /sys/kernel/tegra_gte_test/gpio_en_dis"
