#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include "message_slot.h"

MODULE_LICENSE("GPL");

static message_slot_t *slots = NULL;

static message_slot_t* get_slot(int minor) {
    message_slot_t *slot = slots;
    while (slot != NULL) {
        if (slot->minor == minor)
            return slot;
        slot = slot->next;
    }
    return NULL;
}

static channel_t* get_channel(message_slot_t *slot, unsigned int id) {
    channel_t *channel = slot->channels;
    while (channel != NULL) {
        if (channel->id == id)
            return channel;
        channel = channel->next;
    }
    return NULL;
}

static int device_open(struct inode *inode, struct file *file) {
    int minor = iminor(inode);
    message_slot_t *slot = get_slot(minor);
    if (slot == NULL) {
        slot = kmalloc(sizeof(message_slot_t), GFP_KERNEL);
        if (!slot) return -ENOMEM;
        slot->minor = minor;
        slot->channels = NULL;
        slot->active_channel = NULL;
        slot->next = slots;
        slots = slot;
    }
    file->private_data = slot;
    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    return 0;
}

static long device_ioctl(struct file *file, unsigned int ioctl_command_id, unsigned long ioctl_param) {
    message_slot_t *slot = (message_slot_t*)file->private_data;
    unsigned int channel_id = (unsigned int)ioctl_param;
    if (ioctl_command_id != MSG_SLOT_CHANNEL || channel_id == 0) {
        return -EINVAL;
    }
    channel_t *channel = get_channel(slot, channel_id);
    if (channel == NULL) {
        channel = kmalloc(sizeof(channel_t), GFP_KERNEL);
        if (!channel) return -ENOMEM;
        channel->id = channel_id;
        channel->msg_size = 0;
        channel->next = slot->channels;
        slot->channels = channel;
    }
    slot->active_channel = channel;
    return 0;
}

static ssize_t device_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset) {
    message_slot_t *slot = (message_slot_t*)file->private_data;
    if (slot->active_channel == NULL) return -EINVAL;
    if (length == 0 || length > MAX_MSG_SIZE) return -EMSGSIZE;
    if (copy_from_user(slot->active_channel->message, buffer, length)) return -EFAULT;
    slot->active_channel->msg_size = length;
    return length;
}

static ssize_t device_read(struct file *file, char __user *buffer, size_t length, loff_t *offset) {
    message_slot_t *slot = (message_slot_t*)file->private_data;
    if (slot->active_channel == NULL) return -EINVAL;
    if (slot->active_channel->msg_size == 0) return -EWOULDBLOCK;
    if (length < slot->active_channel->msg_size) return -ENOSPC;
    if (copy_to_user(buffer, slot->active_channel->message, slot->active_channel->msg_size)) return -EFAULT;
    return slot->active_channel->msg_size;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .release = device_release,
    .unlocked_ioctl = device_ioctl,
    .read = device_read,
    .write = device_write,
};

int init_module(void) {
    int ret = register_chrdev(MAJOR_NUM, DEVICE_NAME, &fops);
    if (ret < 0) {
        printk(KERN_ERR "Failed to register device with %d\n", MAJOR_NUM);
        return ret;
    }
    printk(KERN_INFO "Message slot module loaded\n");
    return 0;
}

void cleanup_module(void) {
    message_slot_t *slot = slots;
    while (slot != NULL) {
        message_slot_t *tmp_slot = slot;
        channel_t *channel = slot->channels;
        while (channel != NULL) {
            channel_t *tmp_channel = channel;
            channel = channel->next;
            kfree(tmp_channel);
        }
        slot = slot->next;
        kfree(tmp_slot);
    }
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
    printk(KERN_INFO "Message slot module unloaded\n");
}

module_init(init_module);
module_exit(cleanup_module);
