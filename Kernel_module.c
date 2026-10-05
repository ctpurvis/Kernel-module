#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/dcache.h>
#include <linux/path.h>
#include <linux/string.h>

#define MODULE_TAG "wordswap" 

static char *target_path = "/tmp/test.txt";
module_param(target_path, charp, 0444);
MODULE_PARM_DESC(target_path, "Absolute path of the file to intercept reads on");

static char *from_word = "bat";
module_param(from_word, charp, 0444);
MODULE_PARM_DESC(from_word, "Word to look for in the buffer (must be same length as to_word)");

static char *to_word = "cat";
module_param(to_word, charp, 0444);
MODULE_PARM_DESC(to_word, "Word to substitute in (must be same length as from_word)");
struct probe_data {
	struct file *file;
	char __user *buf;
	size_t count;
	bool is_target;
};

static struct kretprobe krp;

static bool file_matches_target(struct file *file)
{
	char pathbuf[256];
	char *p;

	if (!file || !file->f_path.dentry)
		return false;

	p = d_path(&file->f_path, pathbuf, sizeof(pathbuf));
	if (IS_ERR(p))
		return false;

	return strcmp(p, target_path) == 0;
}

static void substitute_word(char *buf, size_t len, const char *from,
			     const char *to, size_t wlen)
{
	size_t i;

	if (wlen == 0 || len < wlen)
		return;

	for (i = 0; i + wlen <= len; i++) {
		if (memcmp(buf + i, from, wlen) == 0) {
			memcpy(buf + i, to, wlen);
			i += wlen - 1;
		}
	}
}


static int entry_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct probe_data *data = (struct probe_data *)ri->data;
	struct file *file = (struct file *)regs->di;

	if (!file_matches_target(file))
		return 1; /* not our file: skip the return probe entirely */

	data->file = file;
	data->buf = (char __user *)regs->si;
	data->count = (size_t)regs->dx;
	data->is_target = true;

	return 0;
}


static int ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct probe_data *data = (struct probe_data *)ri->data;
	ssize_t nread = regs_return_value(regs);
	size_t wlen = strlen(from_word);
	char *kbuf;

	if (!data->is_target || nread <= 0)
		return 0;

	kbuf = kmalloc(nread, GFP_ATOMIC);
	if (!kbuf)
		return 0;

	if (copy_from_user(kbuf, data->buf, nread)) {
		kfree(kbuf);
		return 0;
	}

	substitute_word(kbuf, nread, from_word, to_word, wlen);

	copy_to_user(data->buf, kbuf, nread);

	kfree(kbuf);
	pr_info(MODULE_TAG ": rewrote read of %s (%zd bytes)\n",
		target_path, nread);

	return 0;
}


static int __init wordswap_init(void)
{	 int ret;

	if (strlen(from_word) != strlen(to_word)) {
		pr_err(MODULE_TAG ": from_word and to_word must be the same length (\"%s\" vs \"%s\")\n",
		       from_word, to_word);
		return -EINVAL;
	}

	krp.kp.symbol_name = "vfs_read";
	krp.entry_handler = entry_handler;
	krp.handler = ret_handler;
	krp.data_size = sizeof(struct probe_data);
	krp.maxactive = 20; /* max concurrent in-flight reads we can track */

	ret = register_kretprobe(&krp);
	if (ret < 0) {
		pr_err(MODULE_TAG ": register_kretprobe failed, returned %d\n", ret);
		return ret;
	}

	pr_info(MODULE_TAG ": loaded, watching \"%s\" (\"%s\" -> \"%s\")\n",
		target_path, from_word, to_word);
	return 0;

}

static void __exit wordswap_exit(void)
{
	unregister_kretprobe(&krp);
	pr_info(MODULE_TAG ": unloaded, missed %d probe hits\n", krp.nmissed);	
	
}

module_init(wordswap_init);
module_exit(wordswap_exit);

MODULE_LICENSE ("GPL");


