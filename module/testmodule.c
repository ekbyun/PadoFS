#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/inet.h>
#include<linux/byteorder/generic.h>
#include"convcli.h"

static int port = 7169;
static char *ipstr = "127.0.0.1";

module_param(port, int, 0000);
module_param_named(ip, ipstr, charp, 0000);

static int __init testmodule_init(void)
{
	unsigned int ipaddr;
	printk("Hello,world in module init\n");
	ipaddr = htonl(in_aton(ipstr));
	printk(" inserted input is .. IP = %s(%d), port = %d\n", ipstr, ipaddr, port);
	test_convcli();
	return 0;	
}

static void __exit testmodule_exit(void)
{
	printk("goodbye,world\n");
}


module_init(testmodule_init);
module_exit(testmodule_exit);
MODULE_LICENSE("GPL");
