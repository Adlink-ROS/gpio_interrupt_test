#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#define DRIVER_NAME "adlink-pps-gpio"

s64 g_timestamp_ns;

struct pps_gpio_device_data {
	int irq;			/* IRQ used as PPS source */
	struct gpio_desc *gpio_pin;	/* GPIO port descriptors */
	bool assert_falling_edge;
	bool base_gpio;
};

// Top ISR, deal with real-time task
static irqreturn_t _irq_top_handler(int irq, void *data)
{
	/* Get the time stamp */
	g_timestamp_ns = ktime_get_ns();
	printk(KERN_INFO "irq=%d, ns=%lld", irq, g_timestamp_ns);

	return IRQ_HANDLED;
}

// Bottom ISR, run after Top ISR
static irqreturn_t _irq_bottom_handler(int irq, void *data)
{
	printk("irq=%d, _irq_bottom_handler, ts_ns=%lld", irq, g_timestamp_ns);

	return IRQ_HANDLED;
}


static int pps_gpio_setup(struct device *dev)
{
	struct pps_gpio_device_data *data = dev_get_drvdata(dev);
	
	data->assert_falling_edge =
		device_property_read_bool(dev, "assert-falling-edge");
	
	data->base_gpio =
		device_property_read_bool(dev, "base-gpio");
		
	data->gpio_pin = devm_gpiod_get(dev, NULL, GPIOD_IN);
	if (IS_ERR(data->gpio_pin))
		return dev_err_probe(dev, PTR_ERR(data->gpio_pin),
				     "failed to request PPS GPIO\n");

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
	ret = gpiod_to_irq(data->gpio_pin);
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

static int pps_gpio_remove(struct platform_device *pdev)
{
	struct pps_gpio_device_data *data = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "removed IRQ %d as PPS source\n", data->irq);
	
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
MODULE_DESCRIPTION("Receive FPGA PPS signal through GPIO pin");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");