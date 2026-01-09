/*
 * This driver gives access(read/write) to the bootcounter used by u-boot.
 * Access is supported via procFS and sysFS.
 *
 * Copyright 2008 DENX Software Engineering GmbH
 * Author: Heiko Schocher <hs@denx.de>
 * Based on work from: Steffen Rumler  (Steffen.Rumler@siemens.com)
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
#include <linux/ptrace.h>
#include <linux/uaccess.h>

#ifndef CONFIG_PROC_FS
#error "PROC FS support must be switched-on"
#endif
#include <linux/proc_fs.h>

#define	UBOOT_BOOTCOUNT_MAGIC_OFFSET	0x04	/* offset of magic number */
#define	UBOOT_BOOTCOUNT_MAGIC		0xB001C041 /* magic number value */

#define	UBOOT_BOOTCOUNT_PROC_ENTRY	"driver/bootcount"

/*
 * This macro frees the machine specific function from bounds checking and
 * this like that...
 */
#define PRINT_PROC(fmt, args...) \
	do { \
		*len += sprintf(buffer + *len, fmt, ##args); \
		if (*begin + *len > offset + size) \
			return 0; \
		if (*begin + *len < offset) { \
			*begin += *len; \
			*len = 0; \
		} \
	} while (0)

void __iomem *mem;
/*
 * read U-Boot bootcounter
 */
static int
read_bootcounter_info(char *buffer, int *len, off_t * begin, off_t offset,
		       int size)
{
	unsigned long magic;
	unsigned long counter;


	magic = be32_to_cpu(readl(mem + UBOOT_BOOTCOUNT_MAGIC_OFFSET));
	counter = be32_to_cpu(readl(mem));

	if (magic == UBOOT_BOOTCOUNT_MAGIC) {
		PRINT_PROC("%lu\n", counter);
	} else {
		PRINT_PROC("bad magic: 0x%lu != 0x%lu\n", magic,
			    (unsigned long)UBOOT_BOOTCOUNT_MAGIC);
	}

	return 1;
}

/*
 * read U-Boot bootcounter (wrapper)
 */
static int
read_bootcounter(char *buffer, char **start, off_t offset, int size,
		  int *eof, void *arg)
{
	int len = 0;
	off_t begin = 0;


	*eof = read_bootcounter_info(buffer, &len, &begin, offset, size);

	if (offset >= begin + len)
		return 0;

	*start = buffer + (offset - begin);
	return size < begin + len - offset ? size : begin + len - offset;
}

/*
 * write new value to U-Boot bootcounter
 */
static int
write_bootcounter(struct file *file, const char *buffer, unsigned long count,
		   void *data)
{
	unsigned long magic;

	magic = be32_to_cpu(readl(mem + UBOOT_BOOTCOUNT_MAGIC_OFFSET));
	if (magic == UBOOT_BOOTCOUNT_MAGIC)
		writel(cpu_to_be32(simple_strtol(buffer, NULL, 10)), mem);
	else
		return -EINVAL;

	return count;
}

/* helper for the sysFS */
static int show_str_bootcount(struct device *device,
				struct device_attribute *attr,
				char *buf)
{
	int ret = 0;
	off_t begin = 0;

	read_bootcounter_info(buf, &ret, &begin, 0, 20);
	return ret;
}
static int store_str_bootcount(struct device *dev,
			struct device_attribute *attr,
			const char *buf,
			size_t count)
{
	write_bootcounter(NULL, buf, count, NULL);
	return count;
}
static DEVICE_ATTR(bootcount, S_IWUSR | S_IRUGO, show_str_bootcount,
		store_str_bootcount);

static int __devinit bootcount_probe(struct platform_device *ofdev)
{
	struct device_node *np = of_node_get(ofdev->dev.of_node);
	struct proc_dir_entry *bootcount;

	mem = of_iomap(np, 0);
	if (mem == NULL)
		dev_err(&ofdev->dev, "%s couldnt map register.\n", __func__);

	/* init ProcFS */
	bootcount = create_proc_entry(UBOOT_BOOTCOUNT_PROC_ENTRY, 0600, NULL);
	if (bootcount == NULL) {
		dev_err(&ofdev->dev, "\n%s (%d): cannot create /proc/%s\n",
			__FILE__, __LINE__, UBOOT_BOOTCOUNT_PROC_ENTRY);
	} else {

		bootcount->read_proc = read_bootcounter;
		bootcount->write_proc = write_bootcounter;
		dev_info(&ofdev->dev, "created \"/proc/%s\"\n",
			UBOOT_BOOTCOUNT_PROC_ENTRY);
	}

	if (device_create_file(&ofdev->dev, &dev_attr_bootcount))
		dev_warn(&ofdev->dev, "%s couldnt register sysFS entry.\n",
			__func__);

	return 0;
}

static int __devexit bootcount_remove(struct platform_device *ofdev)
{
	BUG();
	return 0;
}

static const struct of_device_id __devinitconst bootcount_match[] = {
	{
		.compatible = "uboot,bootcount",
	},
	{},
};
MODULE_DEVICE_TABLE(of, bootcount_match);

static struct platform_driver bootcount_driver = {
	.driver = {
		.name = "bootcount",
		.of_match_table = bootcount_match,
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
	remove_proc_entry(UBOOT_BOOTCOUNT_PROC_ENTRY, NULL);
}

module_init(uboot_bootcount_init);
module_exit(uboot_bootcount_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Steffen Rumler <steffen.rumler@siemens.com>");
MODULE_DESCRIPTION("Provide (read/write) access to the U-Boot bootcounter via PROC FS");
