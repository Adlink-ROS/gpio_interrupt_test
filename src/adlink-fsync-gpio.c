#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#define DRIVER_NAME "adlink-fsync-gpio"

struct fsync_gpio_device_data {
	int irq;
	struct gpio_desc *fsync_gpio_desc;
	bool assert_falling_edge;
	bool base_gpio;
	time64_t time;
	u64 nsec;
};

// Top ISR, deal with the real-time tasks
static irqreturn_t _irq_top_handler(int irq, void *data)
{
	// Get the time stamp
	struct fsync_gpio_device_data *_data = data;
	_data->time = ktime_get_real_seconds();
	_data->nsec = ktime_get_real_ns();
		
	return IRQ_WAKE_THREAD; // schedule the bottom half
}

// Bottom ISR, run the remain tasks after Top ISR
static irqreturn_t _irq_bottom_handler(int irq, void *data)
{
	struct fsync_gpio_device_data *_data = data;
    unsigned int ms, sec, min, hour;

	// TODO: Consider to use spin_lock here
	ms = (_data->nsec / 1000000) % 1000;
	sec = _data->time % 60;
	min = (_data->time / 60) % 60;
	hour = (_data->time / 3600) % 24 + (sys_tz.tz_minuteswest / 60);
	printk("bottom-irq=%d, %02u:%02u:%02u.%03u", irq, hour, min, sec, ms);
	
	return IRQ_HANDLED;
}

static int fsync_gpio_setup(struct device *dev)
{
	struct fsync_gpio_device_data *data = dev_get_drvdata(dev);
	
	data->assert_falling_edge =
		device_property_read_bool(dev, "assert-falling-edge");
		
	data->fsync_gpio_desc = devm_gpiod_get(dev, "dser", GPIOD_IN);
	if (IS_ERR(data->fsync_gpio_desc)) {
		return dev_err_probe(dev, PTR_ERR(data->fsync_gpio_desc),
				     "failed to request dser-gpios");
	}

	return 0;
}

static unsigned long
get_irqf_trigger_flags(const struct fsync_gpio_device_data *data)
{
	unsigned long flags = data->assert_falling_edge ?
		IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;

	return flags;
}

static int fsync_gpio_probe(struct platform_device *pdev)
{
	struct fsync_gpio_device_data *data;
	struct device *dev = &(pdev->dev);
	int ret;

	/* allocate space for device info */
	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	dev_set_drvdata(dev, data);

	/* GPIO setup */
	ret = fsync_gpio_setup(dev);
	if (ret) {
	    dev_err(dev, "failed to setup GPIO: %d\n", ret);
		return ret;
    }

	/* IRQ setup */
	ret = gpiod_to_irq(data->fsync_gpio_desc);
	if (ret < 0) {
		dev_err(dev, "failed to map GPIO to IRQ: %d\n", ret);
		return -EINVAL;
	}
	data->irq = ret;

	if (data->base_gpio) {		
		// base-gpio doesn't need an IRQ Top handler because the interrupt occurs on PCA953x
		ret = devm_request_threaded_irq(dev, data->irq, NULL, _irq_bottom_handler,
			get_irqf_trigger_flags(data), DRIVER_NAME, data);
	} else {
		ret = devm_request_threaded_irq(dev, data->irq, _irq_top_handler, _irq_bottom_handler,
			get_irqf_trigger_flags(data), DRIVER_NAME, data);
	}
	if (ret) {
		dev_err(dev, "failed to acquire IRQ %d, ret=%d\n", data->irq, ret);
		return -EINVAL;
	}

	dev_info(dev, "Driver %s has been successfully probed\n", DRIVER_NAME);

	return 0;
}

static int fsync_gpio_remove(struct platform_device *pdev)
{
	struct fsync_gpio_device_data *data = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "removed IRQ %d\n", data->irq);
	
	return 0;
}

static const struct of_device_id fsync_gpio_dt_ids[] = {
	{ .compatible = DRIVER_NAME, },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, fsync_gpio_dt_ids);

static struct platform_driver fsync_gpio_driver = {
	.probe		= fsync_gpio_probe,
	.remove		= fsync_gpio_remove,
	.driver		= {
		.name	= DRIVER_NAME,
		.of_match_table	= fsync_gpio_dt_ids,
	},
};

module_platform_driver(fsync_gpio_driver);
MODULE_AUTHOR("Ting Chang <ting.chang@adlinktech.com>");
MODULE_DESCRIPTION("Feedback the FPGA Fsync signal to CPU");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");