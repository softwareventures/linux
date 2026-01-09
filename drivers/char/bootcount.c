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
#include <linux/ptrace.h>

#define	UBOOT_BOOTCOUNT_MAGIC_OFFSET	0x04	/* offset of magic number */
#define	UBOOT_BOOTCOUNT_MAGIC		0xB001C041 /* magic number value */

static void __iomem *mem;

static ssize_t bootcount_show(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	const __u32 magic = be32_to_cpu(readl(mem + UBOOT_BOOTCOUNT_MAGIC_OFFSET));
	const __u32 counter = be32_to_cpu(readl(mem));

	if (magic == UBOOT_BOOTCOUNT_MAGIC) {
		return sysfs_emit(buf, "%u\n", counter);
	} else {
		dev_err(device, "Invalid magic number: expected 0x%08x, got 0x%08x.",
			UBOOT_BOOTCOUNT_MAGIC, magic);
		return -ENODEV;
	}
}
static ssize_t bootcount_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf,
			const size_t count)
{
	const __u32 magic = be32_to_cpu(readl(mem + UBOOT_BOOTCOUNT_MAGIC_OFFSET));
	if (magic == UBOOT_BOOTCOUNT_MAGIC) {
		writel(cpu_to_be32(simple_strtol(buf, NULL, 10)), mem);
		return count;
	} else {
		dev_err(dev, "Invalid magic number: expected 0x%08x, got 0x%08x.",
			UBOOT_BOOTCOUNT_MAGIC, magic);
		return -ENODEV;
	}
}
static DEVICE_ATTR(bootcount, S_IWUSR | S_IRUGO, bootcount_show, bootcount_store);

static int bootcount_probe(struct platform_device *ofdev)
{
	struct device_node *np = of_node_get(ofdev->dev.of_node);

	mem = of_iomap(np, 0);
	if (mem == NULL) {
		dev_err(&ofdev->dev, "%s couldnt map register.\n", __func__);
		return -ENOMEM;
	}

	if (device_create_file(&ofdev->dev, &dev_attr_bootcount))
		dev_warn(&ofdev->dev, "%s couldnt register sysFS entry.\n",
			__func__);

	return 0;
}

static void bootcount_remove(struct platform_device *ofdev)
{
	BUG();
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
	if (mem != NULL)
		iounmap(mem);
}

module_init(uboot_bootcount_init);
module_exit(uboot_bootcount_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Daniel Cassidy <mail@danielcassidy.me.uk>");
MODULE_DESCRIPTION("Provide (read/write) access to the U-Boot bootcounter via sysfs");
