#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>

#define DRIVER_NAME "adlink-base-gpio"

struct base_gpio_device_data {
	int irq;
	bool base_gpio;
	time64_t time;
	struct gpio_desc *base_gpio_desc;	/* GPIO port descriptors */
};

// Top ISR, deal with the real-time tasks
static irqreturn_t _irq_top_handler(int irq, void *data)
{
	// Get the time stamp
	struct base_gpio_device_data *_data = data;
	_data->time = ktime_get_real_seconds();
	
	printk("irq=%d, _irq_top_handler", irq);
	
	return IRQ_WAKE_THREAD; // schedule the bottom half
}

// Bottom ISR, run the remain tasks after Top ISR
static irqreturn_t _irq_bottom_handler(int irq, void *data)
{
	struct base_gpio_device_data *_data = data;

	// TODO: Do we need spin_lock here?
	printk("irq=%d, _irq_bottom_handler", irq);

	return IRQ_HANDLED;
}


static int base_gpio_setup(struct device *dev)
{
	const char *ptr;
	struct base_gpio_device_data *data = dev_get_drvdata(dev);
	int ret;

    device_property_read_string(dev, "interrupt", &ptr);
    dev_info(dev, "interrupt=%s", ptr);

	data->base_gpio_desc = devm_gpiod_get(dev, "interrupt", GPIOD_IN);
	if (IS_ERR(data->base_gpio_desc)) {
		return dev_err_probe(dev, PTR_ERR(data->base_gpio_desc),
				     "failed to request base-gpios");
	}
	return 0;
}

static int base_gpio_probe(struct platform_device *pdev)
{
	struct base_gpio_device_data *data;
	struct device *dev = &(pdev->dev);
	int ret;

	dev_info(dev, "starting base_gpio_probe\n");

	/* allocate space for device info */
	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	dev_set_drvdata(dev, data);

	/* GPIO setup */
	ret = base_gpio_setup(dev);
	if (ret) {
	    dev_err(dev, "failed to setup GPIO: %d\n", ret);
		return ret;
    }

	/* IRQ setup */
	ret = gpiod_to_irq(data->base_gpio_desc);
	if (ret < 0) {
		dev_err(dev, "failed to map GPIO to IRQ: %d\n", ret);
		return -EINVAL;
	}
	data->irq = ret;

	// base gpios will not invoke top handler because it happends in tca953x driver.
	ret = devm_request_threaded_irq(dev, data->irq, NULL, _irq_bottom_handler, IRQF_TRIGGER_RISING|IRQF_ONESHOT, DRIVER_NAME, data);
	if (ret) {
		dev_err(dev, "failed to acquire IRQ %d, ret=%d\n", data->irq, ret);
		return -EINVAL;
	}

	dev_info(dev, "Driver %s has been successfully probed\n", DRIVER_NAME);

	return 0;
}

static int base_gpio_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "base_gpio_remove\n");

	return 0;
}

static const struct of_device_id base_gpio_dt_ids[] = {
	{ .compatible = DRIVER_NAME, },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, base_gpio_dt_ids);

static struct platform_driver base_gpio_driver = {
	.probe		= base_gpio_probe,
	.remove		= base_gpio_remove,
	.driver		= {
		.name	= DRIVER_NAME,
		.of_match_table	= base_gpio_dt_ids,
	},
};

module_platform_driver(base_gpio_driver);
MODULE_AUTHOR("Ting Chang <ting.chang@adlinktech.com>");
MODULE_DESCRIPTION("Kernel module for base gpios");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");