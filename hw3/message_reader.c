#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "message_slot.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file> <channel_id>\n", argv[0]);
        return 1;
    }

    char *file_path = argv[1];
    unsigned int channel_id = atoi(argv[2]);
    char buffer[MAX_MSG_SIZE];
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

    ssize_t msg_len = read(fd, buffer, MAX_MSG_SIZE);
    if (msg_len < 0) {
        perror("Failed to read message");
        close(fd);
        return 1;
    }

    if (write(STDOUT_FILENO, buffer, msg_len) != msg_len) {
        perror("Failed to print message");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
