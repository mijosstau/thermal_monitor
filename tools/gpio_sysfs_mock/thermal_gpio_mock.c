#include <linux/gpio/driver.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>

#define THERMAL_GPIO_BASE 10
#define THERMAL_GPIO_COUNT 3

static DEFINE_MUTEX(values_lock);
static unsigned long values;

static int thermal_gpio_get_direction(struct gpio_chip *chip, unsigned int offset)
{
    return GPIO_LINE_DIRECTION_OUT;
}

static int thermal_gpio_direction_output(struct gpio_chip *chip,
                                         unsigned int offset,
                                         int value)
{
    mutex_lock(&values_lock);
    if (value)
        __set_bit(offset, &values);
    else
        __clear_bit(offset, &values);
    mutex_unlock(&values_lock);
    return 0;
}

static void thermal_gpio_set(struct gpio_chip *chip, unsigned int offset, int value)
{
    mutex_lock(&values_lock);
    if (value)
        __set_bit(offset, &values);
    else
        __clear_bit(offset, &values);
    mutex_unlock(&values_lock);
}

static int thermal_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
    int value;

    mutex_lock(&values_lock);
    value = test_bit(offset, &values) ? 1 : 0;
    mutex_unlock(&values_lock);
    return value;
}

static struct gpio_chip thermal_gpio_chip = {
    .label = "thermal-gpio-mock",
    .base = THERMAL_GPIO_BASE,
    .ngpio = THERMAL_GPIO_COUNT,
    .can_sleep = false,
    .get_direction = thermal_gpio_get_direction,
    .direction_output = thermal_gpio_direction_output,
    .set = thermal_gpio_set,
    .get = thermal_gpio_get,
};

static int __init thermal_gpio_mock_init(void)
{
    return gpiochip_add_data(&thermal_gpio_chip, NULL);
}

static void __exit thermal_gpio_mock_exit(void)
{
    gpiochip_remove(&thermal_gpio_chip);
}

module_init(thermal_gpio_mock_init);
module_exit(thermal_gpio_mock_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ThermalMonitor integration test");
MODULE_DESCRIPTION("GPIO sysfs mock chip for ThermalMonitor integration tests");
