/*
 * This driver gives access(read/write) to the bootcounter used by u-boot.
 * Access is supported via sysfs.
 *
 * Copyright 2025 Software Ventures Limited
 * Copyright 2008 DENX Software Engineering GmbH
 * Author: Daniel Cassidy <mail@danielcassidy.me.uk>
 * Author: Heiko Schocher <hs@denx.de>
 * Based on work from: Steffen Rumler <Steffen.Rumler@siemens.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/capability.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#define	UBOOT_BOOTCOUNT_MAGIC_OFFSET	0x04	/* offset of magic number */
#define	UBOOT_BOOTCOUNT_MAGIC		0xB001C041 /* magic number value */

struct bootcount_data {
	void __iomem *mem;
};

static ssize_t bootcount_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	const struct bootcount_data *data = dev_get_drvdata(dev);
	const __u32 magic = ioread32be(data->mem + UBOOT_BOOTCOUNT_MAGIC_OFFSET);
	const __u32 counter = ioread32be(data->mem);

	if (magic == UBOOT_BOOTCOUNT_MAGIC) {
		return sysfs_emit(buf, "%u\n", counter);
	} else {
		dev_err(dev, "invalid magic number: expected 0x%08x, got 0x%08x\n",
			UBOOT_BOOTCOUNT_MAGIC, magic);
		return -ENODEV;
	}
}
static ssize_t bootcount_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf,
			const size_t count)
{
	const struct bootcount_data *data = dev_get_drvdata(dev);
	const __u32 magic = ioread32be(data->mem + UBOOT_BOOTCOUNT_MAGIC_OFFSET);
	__u32 counter;

	if (kstrtou32(buf, 10, &counter)) {
		return -EINVAL;
	}

	if (magic == UBOOT_BOOTCOUNT_MAGIC) {
		iowrite32be(counter, data->mem);
		return (ssize_t)count;
	} else {
		dev_err(dev, "invalid magic number: expected 0x%08x, got 0x%08x\n",
			UBOOT_BOOTCOUNT_MAGIC, magic);
		return -ENODEV;
	}
}
static DEVICE_ATTR_RW(bootcount);

static int bootcount_probe(struct platform_device *ofdev)
{
	struct device_node *np = of_node_get(ofdev->dev.of_node);
	struct bootcount_data *data = devm_kzalloc(&ofdev->dev, sizeof(*data), GFP_KERNEL);

	if (!data)
		return -ENOMEM;

	data->mem = of_iomap(np, 0);
	if (data->mem == NULL) {
		dev_err(&ofdev->dev, "couldn't map register\n");
		return -ENOMEM;
	}

	platform_set_drvdata(ofdev, data);

	const int result = device_create_file(&ofdev->dev, &dev_attr_bootcount);
	if (result) {
		dev_err(&ofdev->dev, "couldn't register sysfs entry\n");
		iounmap(data->mem);
		return result;
	}

	return 0;
}

static void bootcount_remove(struct platform_device *ofdev)
{
	const struct bootcount_data *data = platform_get_drvdata(ofdev);

	device_remove_file(&ofdev->dev, &dev_attr_bootcount);
	iounmap(data->mem);
}

static __initconst const struct of_device_id bootcount_match[] = {
	{
		.compatible = "uboot,bootcount",
	},
	{},
};
MODULE_DEVICE_TABLE(of, bootcount_match);

static struct platform_driver bootcount_driver = {
	.driver = {
		.name = "bootcount",
		.of_match_table = of_match_ptr(bootcount_match),
		.owner = THIS_MODULE,
	},
	.probe = bootcount_probe,
	.remove = bootcount_remove,
};

static int __init uboot_bootcount_init(void)
{
	return platform_driver_register(&bootcount_driver);
}

static void __exit uboot_bootcount_cleanup(void)
{
	platform_driver_unregister(&bootcount_driver);
}

module_init(uboot_bootcount_init);
module_exit(uboot_bootcount_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Cassidy <mail@danielcassidy.me.uk>");
MODULE_DESCRIPTION("Provide read/write access to the u-boot bootcount via sysfs");
