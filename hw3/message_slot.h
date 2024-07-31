#include <linux/ioctl.h>

#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H

#define MAJOR_NUM 235 
#define DEVICE_NAME "message_slot"
#define MAX_CHANNELS 1048576 // 2^20
#define MAX_MSG_SIZE 128
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned int)

typedef struct channel {
    unsigned int id;
    char message[MAX_MSG_SIZE];
    int msg_size;
    struct channel *next;
} channel_t;

typedef struct message_slot {
    int minor;
    channel_t *channels;
    channel_t *active_channel;
    struct message_slot *next;
} message_slot_t;

#endif
