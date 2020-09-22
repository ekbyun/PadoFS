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

#define ORDER	10
#define CLSIZE	64
#define PROC_WIDTH	( CLSIZE * 64 * 16)	//width between each open of the device file

static int maj = 99, min = 0;
static dev_t bench_dev;
static struct cdev bench_cdev;

static struct page *spage = NULL;
static void *svirt = 0;
static phys_addr_t sphys = 0;
static int opencnt = 0;

static int bench_open(struct inode *,struct file *);
static int bench_release(struct inode *,struct file *);
static long bench_ioctl(struct file *,unsigned int, unsigned long);

static int bench_open(struct inode *i,struct file *f) {
#ifdef ALLOC_OPEN
	spage = alloc_pages_node(NUMA_PREF, GFP_KERNEL, ORDER);
	svirt = page_address(spage);
	sphys = virt_to_phys((void *)svirt);
	printk("Bench device opened shared page address (%lx, %lx, %lx)\n",(unsigned long)svirt, (unsigned long)sphys, (unsigned long)(svirt-sphys));
#endif
	opencnt++;	//address width between test threads. PERF measures the # of near accesses seperately.
	return 0;
};

static int bench_release(struct inode *i,struct file *f) {
	printk("Bench device closed\n");
#ifdef ALLOC_OPEN
	free_page((unsigned long)svirt);
#endif
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
	int offset = 0, boffset = 0;
	void *virt = svirt;
	void *ddr = 0;
	int pow=1;
	phys_addr_t phys = sphys;

#ifndef ALLOC_OPEN
	boffset= (opencnt - 1) * PROC_WIDTH;
#endif
#ifdef ALLOC_IOCTL
	virt = __get_free_pages(GFP_KERNEL,ORDER);
	phys = virt_to_phys((void *)virt);
	bofset = 0;
#endif

	ret = copy_from_user(&user, (void *)arg, sizeof(struct bench_ud));

	ddr = page_address(alloc_pages_node(0, GFP_KERNEL,0));
	ret = copy_from_user(ddr, user.address, PAGE_SIZE);

	for(j=0;j<ORDER;j++) pow *= 2;

	st = ktime_get_ns();
	for(i=0; i < 10000;i++) {	//iteration on the same address, one cache line. to amplify the result
		for(j=0 ; j < 1 ; j++) {	//number of consequent cache lines 
			offset = ( CLSIZE*( i * 0 + j * 1) + boffset )%(PAGE_SIZE * pow);
			claddr = virt + offset;
			memcpy(claddr, ddr+offset, CLSIZE);
//			wbinvd();
			clflush(claddr);
		}
	}
	et = ktime_get_ns();

	user.cpuid = smp_processor_id();
	user.address = (void *)(phys+boffset);
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
	}

	spage = alloc_pages_node(NUMA_PREF, GFP_KERNEL, ORDER);
	svirt = page_address(spage);
	sphys = virt_to_phys((void *)svirt);
	printk("Bench device initialized. shared page address (%lx, %lx)\n",(unsigned long)svirt, (unsigned long)sphys);
	return error;
}

static void __exit bench_exit(void)
{
	if( svirt ) free_page((unsigned long)svirt);
	printk("Bench module exit\n");
	cdev_del(&bench_cdev);
	unregister_chrdev_region(bench_dev,1);
}


module_init(bench_init);
module_exit(bench_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eunkyu Byun");
