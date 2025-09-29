// SPDX-License-Identifier: GPL-2.0
#define DEBUG

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/debugfs.h>
#include <linux/list.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Hello Kernelcare module");
MODULE_AUTHOR("Konstantine");

static struct dentry *pBaseDentry;
static struct dentry *pFileEntryJiffies, *pFileEntryData;

static char *kernel_buf;
static struct rw_semaphore data_sem;
static size_t data_size;



struct identity{
        char name[20];
        int id;
        bool hired;
        struct list_head list;
};

static LIST_HEAD(identity_list);

static int identity_create(char *name, int id)
{
        struct identity *new; // create the new struct identity

        new = kzalloc(sizeof(*new), GFP_KERNEL);
        if (!new){
                pr_err("failed to create identity\n");
                return -ENOMEM;
        }

        strscpy(new->name, name, sizeof(new->name));
        new->id = id;
        new->hired = false;
        list_add/*_tail*/(&(new->list), &identity_list);


        return 0;
}



static struct identity *identity_find(int id)
{
        struct identity *tmp;

        list_for_each_entry(tmp, &identity_list, list){
                if (tmp->id == id)
                        return tmp;
        }

        return NULL;
}


static void identity_destroy(int id)
{
        struct identity *tmp, *next;

        list_for_each_entry_safe(tmp, next, &identity_list, list){
                if (tmp->id == id){
                        list_del(&tmp->list);
                        kfree(tmp);
                }
        }
        pr_info("[+] freeing memory of identity %d is done\n", id);
}

static int identity_hire(int id)
{
        struct identity *tmp;
        list_for_each_entry(tmp, &identity_list, list){
                if(tmp->id == id){
                        tmp->hired = true;
                        return 0;
                }
        }

        return -ENOENT;
}






/* ssize_t (*read) (struct file *, char __user *, size_t, loff_t *); */
static ssize_t read_data(struct file *flip, char __user *buf, size_t size, loff_t *f_pos)
{
        ssize_t ret;

        down_read(&data_sem);
        ret = simple_read_from_buffer(buf, size, f_pos, kernel_buf, data_size);

        up_read(&data_sem);
        return ret;
}


static ssize_t write_data(struct file *flip, const char __user *buf, size_t size, loff_t *f_pos)
{
        ssize_t ret;

        if (size > PAGE_SIZE)
                size = PAGE_SIZE;

        down_write(&data_sem);
        ret = simple_write_to_buffer(kernel_buf, PAGE_SIZE, f_pos, buf, size);
        if (ret >= 0)
                data_size = min(*f_pos, (loff_t)PAGE_SIZE); //update valid data length. f_pos now points to the end of the valid data - that's why we set data_size to f_pos.
        up_write(&data_sem);

        return ret;
}

struct file_operations read_write_fops = {
        .owner = THIS_MODULE,
        .read = &read_data,
        .write = &write_data,
};



// ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
static ssize_t read_jiffies(struct file *flip, char __user *buf, size_t size, loff_t *f_pos)
{
        //jiffies is 64 bit(long long) 2^64=18446744073709551616, which is 20 characters. null-terminator and newline is +2 characters = 22.
        char tmp[25];
        int written;

        written = scnprintf(tmp, sizeof(tmp), "%llu\n", (unsigned long long)get_jiffies_64());

        return simple_read_from_buffer(buf, size, f_pos, tmp, written);
}


struct file_operations jiffies_fops = {
        .owner = THIS_MODULE,
        .read = read_jiffies,
};


static int hello_init(void)
{
        pBaseDentry = debugfs_create_dir("kernelcare", NULL);
        if(IS_ERR(pBaseDentry)){
                pr_err("kernelcare: failed to create debugfs directory\n");
                return PTR_ERR(pBaseDentry);
        }


        pFileEntryJiffies = debugfs_create_file("jiffies", 0444, pBaseDentry, NULL, &jiffies_fops);
        if (IS_ERR(pFileEntryJiffies)){
                pr_err("kernelcare: failed to create jiffies file\n");
                return PTR_ERR(pFileEntryJiffies);
        }

        kernel_buf = (char*) kzalloc(PAGE_SIZE, GFP_KERNEL);
        if (!kernel_buf){
                pr_err("[-] kernelcare: failed to allocate memory for buffer\n");
                debugfs_remove(pBaseDentry);
                return -ENOMEM;
        }
        data_size = 0;

        init_rwsem(&data_sem); //initialize the rw_semaphore.

        /* struct dentry *debugfs_create_file(const char *name, umode_t mode, struct dentry *parent, void *data, const struct file_operations *fops); */
        pFileEntryData = debugfs_create_file("data", 0644, pBaseDentry, NULL, &read_write_fops);
        if (IS_ERR(pFileEntryData)){
                pr_err("[-] kernelcare: failed to create file\n");
                return PTR_ERR(pFileEntryData);
        }



        struct identity *temp;

        if(identity_create("Konstantine", 1)){
                pr_err("[-] could not create identity Konstantine\n");
                return -ENOMEM;
        }

        pr_info("[+] identity Konstantine created\n");

        if(identity_create("Anonymous Goose", 2)){
                pr_err("[-] could not create identity Anonymous Goose\n");
                return -ENOMEM;
        }

        pr_info("[+] identity Anonymous Goose created\n");

        temp = identity_find(1);
        pr_info("id 1 = %s\n", temp->name);

        if(identity_hire(1))
                return -ENOENT;

        pr_info("[+] Konstantine is hired\n");

        temp = identity_find(10);
        if (temp == NULL)
                pr_debug("id 10 not found\n");

        identity_destroy(2);
        identity_destroy(1);



        pr_debug("Hello, Kernelcare!\n");
        return 0;
}

static void hello_exit(void)
{
        debugfs_remove(pBaseDentry);
        kfree(kernel_buf);

        struct identity *tmp, *next;
        list_for_each_entry_safe(tmp, next, &identity_list, list){
                list_del(&tmp->list);   // unlink first before freeing
                kfree(tmp); // we need to unlink first so we don't just follow invalid pointers and go around freeing all kinds of memory
        }

        pr_debug("Goodbye, Kernelcare!\n");
}

module_init(hello_init);
module_exit(hello_exit);
