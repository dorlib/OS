#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "message_slot.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <file> <channel_id> <message>\n", argv[0]);
        return 1;
    }

    char *file_path = argv[1];
    unsigned int channel_id = atoi(argv[2]);
    char *message = argv[3];
    int fd = open(file_path, O_RDWR);
    if (fd < 0) {
        perror("Failed to open file");
        return 1;
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, channel_id) < 0) {
        perror("Failed to set channel");
        close(fd);
        return 1;
    }

    ssize_t msg_len = strlen(message);
    if (write(fd, message, msg_len) != msg_len) {
        perror("Failed to write message");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
