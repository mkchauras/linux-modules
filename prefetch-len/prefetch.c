#include "linux/list.h"
#include "linux/mm_types.h"
#include "linux/printk.h"
#include "linux/sched.h"
#include "linux/vmalloc.h"
#include <asm-generic/errno-base.h>
#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/rmap.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/debugfs.h>

#define DEBUGFS_DIR_NAME "prefetch-controller"
#define DEBUGFS_FILE_NAME "prefetch-depth"

static struct dentry *debugfs_dir;
static struct dentry *debugfs_file;
void __exit prefetch_exit(void);
int __init prefetch_init(void);


static int prefetch_open(struct inode *inode, struct file *filp)
{
	pr_info("Prefetch controller Device Open\n");
	return 0; /* success */
}

static int prefetch_release(struct inode *inode, struct file *filp)
{
	pr_info("Prefetch controller Device Close\n");
	return 0;
}

static ssize_t prefetch_write(struct file *filp,
                              const char __user *buf,
                              size_t count,
                              loff_t *f_pos)
{
        char data[8];
        unsigned long long depth;
        unsigned long long dscr;

        memset(data, 0, sizeof(data));

        /* Expect exactly 1 digit: 0–7 */
        if (count < 1 || count > 2) {
                pr_err("Only values 0–7 allowed (PowerISA Book II §4.2)\n");
                return -EINVAL;
        }

        if (copy_from_user(data, buf, count))
                return -EFAULT;

        if (kstrtoull(data, 16, &depth)) {
                pr_warn("Invalid value '%s'\n", data);
                return -EINVAL;
        }

        if (depth > 7) {
                pr_err("DSCR depth must be in range 0–7\n");
                return -EINVAL;
        }

        /* Read DSCR (SPR 3) */
        asm volatile(
                "mfspr %0, 3\n"
                : "=r"(dscr)
        );

        /* Clear bits [0:2], preserve upper 61 bits */
        dscr &= ~0x7ULL;

        /* Set new depth */
        dscr |= (depth & 0x7ULL);

        /* Write DSCR back */
        asm volatile(
                "mtspr 3, %0\n"
                :
                : "r"(dscr)
                : "memory"
        );

        return count;
}


static ssize_t prefetch_read(struct file *file,
                             char __user *buf,
                             size_t count,
                             loff_t *ppos)
{
        char kbuf[4];
        unsigned long long dscr;
        unsigned long depth;
        int len;

        /* Allow read only once (EOF semantics) */
        if (*ppos != 0)
                return 0;

        /* Read DSCR (SPR 3) */
        asm volatile(
                "mfspr %0, 3\n"
                : "=r"(dscr)
        );

        /* Extract bits [0:2] */
        depth = dscr & 0x7ULL;

        /* Format as hex + newline */
        len = scnprintf(kbuf, sizeof(kbuf), "%lx\n", depth);

        if (len > count)
                return -EINVAL;

        if (copy_to_user(buf, kbuf, len))
                return -EFAULT;

        *ppos += len;
        return len;
}

static struct file_operations prefetch_fops = {
	.owner = THIS_MODULE,
	.read = prefetch_read,
	.write = prefetch_write,
	.open = prefetch_open,
	.release = prefetch_release,
};

void __exit prefetch_exit(void)
{
	debugfs_remove_recursive(debugfs_dir);
	pr_info("Address Translation Module Unloaded\n");
}

int __init prefetch_init(void)
{
	debugfs_dir = debugfs_create_dir(DEBUGFS_DIR_NAME, NULL);
	if (!debugfs_dir) {
		pr_err("Failed to create debugfs directory\n");
		return -ENOMEM;
	}

	debugfs_file = debugfs_create_file(DEBUGFS_FILE_NAME, 0644, debugfs_dir,
					   NULL, &prefetch_fops);
	if (!debugfs_file) {
		pr_err("Failed to create debugfs file\n");
		debugfs_remove_recursive(debugfs_dir);
		return -ENOMEM;
	}
	return 0;
}

module_init(prefetch_init);
module_exit(prefetch_exit);

MODULE_AUTHOR("Mukesh Kumar Chaurasiya");
MODULE_DESCRIPTION("Address Translation Module");
MODULE_LICENSE("GPL");
