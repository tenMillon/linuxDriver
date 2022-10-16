#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define GPIOLED_CNT 1
#define GPIOLED_NAME "gpioled"
#define LEDOFF '0'
#define LEDON '1'

 /* gpioled 设备结构体 */
struct gpioled_dev{
    dev_t devid;             /* 设备号 */
    struct cdev cdev;        /* cdev */
    struct class *class;     /* 类 */
    struct device *device;    /* 设备 */
    int major;               /* 主设备号 */
    int minor;               /* 次设备号 */
    struct device_node *nd;  /* 用于设备树中的对应的设备节点 */
    int led_gpio;            /* 驱动对应的GPIO编号 */
};

/* 声明一个设备结构体的对象，此对象在xxx_init函数中进行初始化 */
struct gpioled_dev gpioled;


/* 声明与用户空间调用函数对应的函数 */
static int led_open(struct inode *inode, struct file *filp)
{
    /* 私有化设备数据，方便在其他函数中调用 */
    filp->private_data = &gpioled;
    return 0;
}

static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue;
    unsigned char databuf[1];
    unsigned char ledstat;
    struct gpioled_dev *dev = filp->private_data;

    retvalue = copy_from_user(databuf, buf, cnt);

    if(retvalue < 0) {
        printk("kernal write failed!\r\n");
        return -EFAULT;
    }

    ledstat = databuf[0];

    if(ledstat == LEDON) {
        gpio_set_value(dev->led_gpio, 0);
    } else {
        gpio_set_value(dev->led_gpio, 1);
    }

    return 0;
}

static ssize_t led_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/* 设备操作函数，初始化与用户空间对应函数的结构体 */
static struct file_operations gpioled_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_release,
};

/* 设备入口函数，内核加载驱动的时候，会先调用这个函数 */
static int __init led_init(void)
{
    int ret = 0;

    /* get gpioled node */
    gpioled.nd = of_find_node_by_path("/gpioled");
    if(gpioled.nd == NULL) {
        printk("gpioled node can not find!\r\n");
        return -EINVAL;
    } else {
        printk("gpioled node has been found");
    }

    /* get led gpio num */
    gpioled.led_gpio = of_get_named_gpio(gpioled.nd, "led-gpio", 0);
    if(gpioled.led_gpio < 0) {
        printk("Can not get led-gpio\r\n");
        return -EINVAL;
    }
    printk("led-gpio num = %d\r\n", gpioled.led_gpio);

    /* 设置 GPIO1_IO03 为输出，并且输出高电平，默认关闭 LED 灯 */
    gpio_request(timerdev.led_gpio, "led");
    ret = gpio_direction_output(gpioled.led_gpio, 1);
    if(ret < 0) {
        printk("can't set gpio!\r\n");
    }

    /* 注册字符设备驱动 */
    if (gpioled.major) {
        gpioled.devid = MKDEV(gpioled.major, 0);
        register_chrdev_region(gpioled.devid, GPIOLED_CNT, GPIOLED_NAME);
    } else {
        alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_CNT, GPIOLED_NAME);
        gpioled.major = MAJOR(gpioled.devid);
        gpioled.minor = MINOR(gpioled.devid);
    }

    /* 初始化cdev */
    gpioled.cdev.owner = THIS_MODULE;
    cdev_init(&gpioled.cdev, &gpioled_fops);

    /* 添加一个cdev */
    cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_CNT);

    /* 创建类 */
    gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
    if (IS_ERR(gpioled.class)) {
        return PTR_ERR(gpioled.class);
    }

    /* 创建设备 */
    gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);
    if (IS_ERR(gpioled.device)) {
        return PTR_ERR(gpioled.device);
    }

    return 0;
}


/* 设备出口函数，内核卸载驱动的时候，会调用这个函数 */
static void __exit led_exit(void) {
    cdev_del(&gpioled.cdev);
    unregister_chrdev_region(gpioled.devid, GPIOLED_CNT); /* 注销 */

    device_destroy(gpioled.class, gpioled.devid);
    class_destroy(gpioled.class);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("wensheng.wang");
