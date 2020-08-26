#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/major.h>
#include<linux/byteorder/generic.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/smp.h>
#include<asm/uaccess.h>

static int maj = 99, min = 0;
static dev_t bench_dev;
static struct cdev bench_cdev;

static int bench_open(struct inode *,struct file *);
static int bench_release(struct inode *,struct file *);
static long bench_ioctl(struct file *,unsigned int, unsigned long);

static int bench_open(struct inode *i,struct file *f) {
	printk("Bench device opened\n");
	return 0;
};

static int bench_release(struct inode *i,struct file *f) {
	printk("Bench device closed\n");
	return 0;
};

struct bench_ud {
	uint16_t cpuid;
	uint64_t addr;
	long kwt;
	void *userpage;
};

static long bench_ioctl(struct file *f,unsigned int cmd, unsigned long arg) {
	int ret;
	
	struct bench_ud user;

	ret = copy_from_user(&user, (void *)arg, sizeof(struct bench_ud));

	printk("ioctl called. copy_from_user ret = %d\n",ret);
	user.cpuid = smp_processor_id();
	user.addr = (uint64_t) user.userpage;

	ret = copy_to_user((void *)arg, &user, sizeof(struct bench_ud));
	return 0;
};

static struct file_operations bench_fops = {
	.owner = THIS_MODULE,
	.open = bench_open,
	.release = bench_release,
	.unlocked_ioctl = bench_ioctl,
	.compat_ioctl = bench_ioctl
};

static int __init bench_init(void)
{
	int error;
	printk("Bench module init .....\n");

	bench_dev = MKDEV(maj,min);
	error = register_chrdev_region(bench_dev,1,"bench");
	if( error < 0 ) {
		printk(KERN_WARNING "bench: cat't get major %d, error= %d\n",maj,error);
		return error;
	}

	cdev_init(&bench_cdev, &bench_fops);
	bench_cdev.owner = THIS_MODULE;
	bench_cdev.ops = &bench_fops;

	error = cdev_add(&bench_cdev, bench_dev, 1);
	
	if(error) {
		printk(KERN_NOTICE "bench register error %d\n",error);
	} else {
		printk("Bench module init success\n");
	}
	return error;
}

static void __exit bench_exit(void)
{
	printk("Bench module exit\n");
	cdev_del(&bench_cdev);
	unregister_chrdev_region(bench_dev,1);
}


module_init(bench_init);
module_exit(bench_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eunkyu Byun");
