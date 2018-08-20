#include<linux/module.h>
#include<linux/parser.h>
#include<linux/kernel.h>
#include<linux/inet.h>
#include<linux/slab.h>
#include<linux/string.h>
#include<linux/byteorder/generic.h>
#include<linux/fs.h>

#include"convcli.h"

#define DIR_NAME_LEN	64

//for test
/*
static int port = 7169;
static char *ipstr = "127.0.0.1";

module_param(port, int, 0000);
module_param_named(ip, ipstr, charp, 0000);
*/

struct padofs_mount_opts {
	char *basedir;
	char *datadir;
};

enum {
	Opt_basedir,
	Opt_datadir
};

static const match_table_t tokens = {
	{Opt_basedir, "base=%s"},
	{Opt_datadir, "data=%s"}
};

struct padofs_fsinfo {
	struct padofs_mount_opts mount_opts;
};

int padofs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct padofs_fsinfo *fsi;

	substring_t args[MAX_OPT_ARGS];
	int token;
	char *p;

	struct inode *inode;

	//allocate memory for fs dependent info
	fsi = kzalloc(sizeof(struct padofs_fsinfo), GFP_KERNEL);
	sb->s_fs_info = fsi;
	if( !fsi )
		return -ENOMEM;

	//parse options
	while((p = strsep((char **)&data,",")) != NULL) {
		if (!*p)
			continue;

		token = match_token(p, tokens, args);
		switch(token) {
		case Opt_basedir:
			fsi->mount_opts.basedir = match_strdup(args);
			break;
		case Opt_datadir:
			fsi->mount_opts.datadir = match_strdup(args);
			break;
		}
	}

	printk(KERN_NOTICE"pado_mount options inserted are basedir=%s, datadir=%s\n",((struct padofs_fsinfo *)sb->s_fs_info)->mount_opts.basedir,((struct padofs_fsinfo *)sb->s_fs_info)->mount_opts.datadir );
	
	//create inode and dentry for root 
	inode = new_inode(sb);
	sb->s_root = d_make_root(inode);

	if( !sb->s_root )
		return -ENOMEM;

	return 0;
}

struct dentry *
padofs_mount(
	struct file_system_type *fs_type,
	int	flags,
	const char *dev_name,
	void *data)
{
	printk(KERN_NOTICE"pado_fs_mount is called device=%s\n",dev_name);
	return mount_nodev(fs_type, flags, data, padofs_fill_super);
}


void padofs_kill_super(struct super_block *sb)
{
	printk(KERN_NOTICE"padofs_kill_sb is called\n");
	kill_anon_super(sb);
}

// imformation for file system register
static struct file_system_type pado_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "padofs",
	.mount		= padofs_mount,
	.kill_sb	= padofs_kill_super,
};
MODULE_ALIAS_FS("padofs");


static int __init testmodule_init(void)
{
	int ret;

	//for test
//	unsigned int ipaddr;
//	ipaddr = htonl(in_aton(ipstr));
//	printk(" inserted input is .. IP = %s(%d), port = %d\n", ipstr, ipaddr, port);
//
	printk("Hello,world in module init\n");
	test_convcli();

	//filesystem register
	ret = register_filesystem(&pado_fs_type);
	printk(KERN_NOTICE"register padofs return = %d\n",ret);

	return ret;
}

static void __exit testmodule_exit(void)
{
	int error; 
	printk("goodbye,world\n");
	error = unregister_filesystem(&pado_fs_type);
	printk(KERN_NOTICE"unregister padofs returns %d\n",error);
}


module_init(testmodule_init);
module_exit(testmodule_exit);
MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Eunkyu Byun");
