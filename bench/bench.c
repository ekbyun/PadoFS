#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/major.h>
#include<linux/byteorder/generic.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/smp.h>
#include<linux/uaccess.h>
#include<linux/time.h>
#include<linux/mm.h>
#include<asm/types.h>

#ifndef NUMA_PREF
#define NUMA_PREF	1
#endif

#define ORDER	2

static int maj = 99, min = 0;
static dev_t bench_dev;
static struct cdev bench_cdev;

static struct page *spage = NULL;
static void *svirt = 0;
static phys_addr_t sphys = 0;

static int bench_open(struct inode *,struct file *);
static int bench_release(struct inode *,struct file *);
static long bench_ioctl(struct file *,unsigned int, unsigned long);

static int bench_open(struct inode *i,struct file *f) {
	spage = alloc_pages_node(NUMA_PREF, GFP_KERNEL, ORDER);
	svirt = page_address(spage);
	sphys = virt_to_phys((void *)svirt);
	printk("Bench device opened shared page address (%lx, %lx, %lx)\n",(unsigned long)svirt, (unsigned long)sphys, (unsigned long)(svirt-sphys));
	return 0;
};

static int bench_release(struct inode *i,struct file *f) {
	printk("Bench device closed\n");
	free_page((unsigned long)svirt);
	return 0;
};

struct bench_ud {
	void *address;
	u64 wt;
	int cpuid;
};

static long bench_ioctl(struct file *f,unsigned int cmd, unsigned long arg) {
	int i, j, ret;
	struct bench_ud user;
	u64 st, et;
	void *claddr;
	int offset;
	void *virt = svirt;
	void *ddr = 0;
	int pow=1;
	phys_addr_t phys = sphys;

#ifdef ALLOC_IOCTL
	virt = __get_free_pages(GFP_KERNEL,ORDER);
	phys = virt_to_phys((void *)virt);
#endif

	ret = copy_from_user(&user, (void *)arg, sizeof(struct bench_ud));

	ddr = page_address(alloc_pages_node(0, GFP_KERNEL,0));
	ret = copy_from_user(ddr, user.address, 4096);

	for(j=0;j<ORDER;j++) pow *= 2;

	st = ktime_get_ns();
	for(i=0; i < 2048;i++) {
		for(j=0 ; j < 1 ; j++) {
			offset = (64*(64*i + j))%(4096 * pow);
			claddr = virt + offset;
			memcpy(claddr, ddr+offset, 64);
//			ret = copy_from_user(claddr, user.address, 64);
//			wbinvd();
			clflush(claddr);
		}
	}
	et = ktime_get_ns();

	user.cpuid = smp_processor_id();
	user.address = (void *)phys;
	user.wt = et-st;

#ifdef ALLOC_IOCTL
	free_page((unsigned long)virt);
#endif
	free_page((unsigned long)ddr);

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
