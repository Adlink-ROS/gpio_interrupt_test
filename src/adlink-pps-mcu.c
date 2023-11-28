#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>

#define DRIVER_NAME "adlink-pps-mcu"
#define GPRMC_UART_TX "/dev/ttyTHS0"
#define KERNEL_BUF_SIZE 128

struct pps_gpio_device_data {
	int irq;			/* IRQ used as PPS source */
	struct gpio_desc *pps_in_desc;	/* GPIO port descriptors */
	int pps_out_pinnum;
	bool assert_falling_edge;
	bool base_gpio;
	time64_t time;
	struct file *fptr;
};

static int gprmc_serial_open(struct pps_gpio_device_data *data)
{
    data->fptr = filp_open(GPRMC_UART_TX, O_RDWR|O_NOCTTY|O_NONBLOCK, 0);

	if (!data->fptr) {
		printk("Failed to open %s", GPRMC_UART_TX);
		return -1;
	}

	return 0;
}

static void gprmc_serial_close(struct pps_gpio_device_data *data)
{
	filp_close(data->fptr, NULL);
}

static int gprmc_serial_write(struct pps_gpio_device_data *data,
		const unsigned char *buf, size_t count)
{
	int wlen;
	loff_t pos = data->fptr->f_pos;
	mm_segment_t oldfs;
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	
	wlen = kernel_write(data->fptr, buf, count, &pos);
	
	set_fs(oldfs);
	return wlen;
}

// Top ISR, deal with the real-time tasks
static irqreturn_t _irq_top_handler(int irq, void *data)
{
	// Get the time stamp
	struct pps_gpio_device_data *_data = data;
	_data->time = ktime_get_real_seconds();
	
	// // Pull high the PPS_OUT
	// if (gpio_is_valid(_data->pps_out_pinnum)) {
	// 	gpio_set_value(_data->pps_out_pinnum, 1);
	// }
	
	return IRQ_WAKE_THREAD; // schedule the bottom half
}

// Bottom ISR, run the remain tasks after Top ISR
static irqreturn_t _irq_bottom_handler(int irq, void *data)
{
	struct pps_gpio_device_data *_data = data;
    char *gprmc_buf;
    char *tmp_buf;
    int CRC;
    int i;
    int sec, min, hour;

	// Pull low the PPS_OUT after 100us
	// if (gpio_is_valid(_data->pps_out_pinnum)) {
	// 	udelay(100);
	// 	gpio_set_value(_data->pps_out_pinnum, 0);
	// }

	// TODO: Do we need spin_lock here? PPS interrupt triggers once a second, will it be preempted?
	sec = _data->time % 60;
	min = (_data->time / 60) % 60;
	hour = (_data->time / 3600) % 24 + (sys_tz.tz_minuteswest / 60);
	printk("irq=%d, _irq_bottom_handler, %02d:%02d:%02d", irq, hour, min, sec);
	
    // Prepare GPRMC msg
	// gprmc_buf = kmalloc(KERNEL_BUF_SIZE, GFP_KERNEL | __GFP_ZERO);
	// tmp_buf = kmalloc(KERNEL_BUF_SIZE, GFP_KERNEL | __GFP_ZERO);
	// if (!gprmc_buf || !tmp_buf) {
	// 	printk("Failed to allocate memory.");
	// } else {
    //     snprintf(tmp_buf, KERNEL_BUF_SIZE, "GPRMC,%02d%02d%02d,A,%s,N,%s,E,022.4,084.4,070423,,A", hour, min, sec, "25.04776", "121.53185");
    //     for (i = 0; i < strlen(tmp_buf); i++) {
    //         // XOR every character between '$' and '*'
    //         CRC = CRC ^ tmp_buf[i];
    //     }

	// 	snprintf(gprmc_buf, KERNEL_BUF_SIZE, "$%s*%02X\r\n", tmp_buf, CRC);
		
	// 	// Write to UART TX port
	// 	// gprmc_serial_write(data, gprmc_buf, strlen(gprmc_buf));
		
	// 	kfree(gprmc_buf);
	// 	kfree(tmp_buf);
	// }
	
	return IRQ_HANDLED;
}


static int pps_gpio_setup(struct device *dev)
{
	struct pps_gpio_device_data *data = dev_get_drvdata(dev);
	struct device_node *node = dev->of_node;
	
	data->assert_falling_edge =
		device_property_read_bool(dev, "assert-falling-edge");
		
	data->pps_in_desc = devm_gpiod_get(dev, "pps-mcu", GPIOD_IN);
	if (IS_ERR(data->pps_in_desc)) {
		return dev_err_probe(dev, PTR_ERR(data->pps_in_desc),
				     "failed to request pps-mcu-gpios");
	}

	// data->pps_out_pinnum = of_get_named_gpio(node, "pps-out-gpios", 0);
	// if (!gpio_is_valid(data->pps_out_pinnum)) {
	// 	dev_err(dev, "faiiled to request pps-out-gpios");
	// 	return -1;
	// }
	// gpio_direction_output(data->pps_out_pinnum, 0);

	// open GPRMC_UART_TX
	// gprmc_serial_open(data);

	return 0;
}

static unsigned long
get_irqf_trigger_flags(const struct pps_gpio_device_data *data)
{
	unsigned long flags = data->assert_falling_edge ?
		IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;

	return flags;
}

static int pps_gpio_probe(struct platform_device *pdev)
{
	struct pps_gpio_device_data *data;
	struct device *dev = &(pdev->dev);
	int ret;

	/* allocate space for device info */
	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	dev_set_drvdata(dev, data);

	/* GPIO setup */
	ret = pps_gpio_setup(dev);
	if (ret) {
	    dev_err(dev, "failed to setup GPIO: %d\n", ret);
		return ret;
    }

	/* IRQ setup */
	ret = gpiod_to_irq(data->pps_in_desc);
	if (ret < 0) {
		dev_err(dev, "failed to map GPIO to IRQ: %d\n", ret);
		return -EINVAL;
	}
	data->irq = ret;


	ret = devm_request_threaded_irq(dev, data->irq, _irq_top_handler, _irq_bottom_handler,
									get_irqf_trigger_flags(data), DRIVER_NAME, data);
	if (ret) {
		dev_err(dev, "failed to acquire IRQ %d, ret=%d\n", data->irq, ret);
		return -EINVAL;
	}

	dev_info(dev, "Driver %s has been successfully probed\n", DRIVER_NAME);

	return 0;
}

static int pps_gpio_remove(struct platform_device *pdev)
{
	struct pps_gpio_device_data *data = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "removed IRQ %d as PPS source\n", data->irq);
	
	// close GPRMC_UART_TX
	// gprmc_serial_close(data);
	
	return 0;
}

static const struct of_device_id pps_gpio_dt_ids[] = {
	{ .compatible = DRIVER_NAME, },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, pps_gpio_dt_ids);

static struct platform_driver pps_gpio_driver = {
	.probe		= pps_gpio_probe,
	.remove		= pps_gpio_remove,
	.driver		= {
		.name	= DRIVER_NAME,
		.of_match_table	= pps_gpio_dt_ids,
	},
};

module_platform_driver(pps_gpio_driver);
MODULE_AUTHOR("Ting Chang <ting.chang@adlinktech.com>");
MODULE_DESCRIPTION("Receive PPS-MCU signal from FPGA");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");