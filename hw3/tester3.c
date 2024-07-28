#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sysmacros.h>

#define DEVICE_FILE_PATH "/dev/msg_slot"
#define MAJOR_NUM 235
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned int)
#define BUF_LEN 128

void create_device_files()
{
    for (int i = 0; i < 2; i++)
    {
        char device_path[20];
        sprintf(device_path, "%s%d", DEVICE_FILE_PATH, i);
        if (mknod(device_path, S_IFCHR | 0666, makedev(MAJOR_NUM, i)) < 0)
        {
            perror("mknod");
            exit(1);
        }
    }
}

void cleanup_device_files()
{
    for (int i = 0; i < 2; i++)
    {
        char device_path[20];
        sprintf(device_path, "%s%d", DEVICE_FILE_PATH, i);
        if (unlink(device_path) < 0)
        {
            perror("unlink");
        }
    }
}

void load_module()
{
    system("insmod message_slot.ko");
}

void unload_module()
{
    system("rmmod message_slot");
}

void run_test(const char *test_name, int (*test_func)())
{
    printf("Running %s: ", test_name);
    cleanup_device_files();
    unload_module();
    load_module();
    create_device_files();

    if (test_func() == 0)
    {
        printf("\033[1;32mPASSED\033[0m\n");
    }
    else
    {
        printf("\033[1;31mFAILED\033[0m\n");
    }
}

int write_and_read_test()
{
    int fd;
    char buffer[BUF_LEN];
    char *msg = "Hello, World!";
    ssize_t bytes_written, bytes_read;

    fd = open(DEVICE_FILE_PATH "0", O_RDWR);
    if (fd < 0)
    {
        perror("open");
        return 1;
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, 1) < 0)
    {
        perror("ioctl");
        close(fd);
        return 1;
    }

    bytes_written = write(fd, msg, strlen(msg));
    if (bytes_written != strlen(msg))
    {
        perror("write");
        close(fd);
        return 1;
    }

    bytes_read = read(fd, buffer, BUF_LEN);
    if (bytes_read < 0)
    {
        perror("read");
        close(fd);
        return 1;
    }

    buffer[bytes_read] = '\0';
    if (strcmp(buffer, msg) != 0)
    {
        fprintf(stderr, "Message mismatch: expected \"%s\", got \"%s\"\n", msg, buffer);
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
int sharing_channels_test()
{
    int fd1, fd2;
    char buffer1[BUF_LEN], buffer2[BUF_LEN];
    char *msg1 = "Message for slot 0";
    char *msg2 = "Message for slot 1";
    ssize_t bytes_written, bytes_read;

    fd1 = open(DEVICE_FILE_PATH "0", O_RDWR);
    if (fd1 < 0)
    {
        perror("open fd1");
        return 1;
    }

    fd2 = open(DEVICE_FILE_PATH "1", O_RDWR);
    if (fd2 < 0)
    {
        perror("open fd2");
        close(fd1);
        return 1;
    }

    if (ioctl(fd1, MSG_SLOT_CHANNEL, 1) < 0 || ioctl(fd2, MSG_SLOT_CHANNEL, 1) < 0)
    {
        perror("ioctl");
        close(fd1);
        close(fd2);
        return 1;
    }

    bytes_written = write(fd1, msg1, strlen(msg1));
    if (bytes_written != strlen(msg1))
    {
        perror("write fd1");
        close(fd1);
        close(fd2);
        return 1;
    }

    bytes_written = write(fd2, msg2, strlen(msg2));
    if (bytes_written != strlen(msg2))
    {
        perror("write fd2");
        close(fd1);
        close(fd2);
        return 1;
    }

    bytes_read = read(fd1, buffer1, BUF_LEN);
    if (bytes_read < 0)
    {
        perror("read fd1");
        close(fd1);
        close(fd2);
        return 1;
    }

    buffer1[bytes_read] = '\0';
    if (strcmp(buffer1, msg1) != 0)
    {
        fprintf(stderr, "Message mismatch for fd1: expected \"%s\", got \"%s\"\n", msg1, buffer1);
        close(fd1);
        close(fd2);
        return 1;
    }

    bytes_read = read(fd2, buffer2, BUF_LEN);
    if (bytes_read < 0)
    {
        perror("read fd2");
        close(fd1);
        close(fd2);
        return 1;
    }

    buffer2[bytes_read] = '\0';
    if (strcmp(buffer2, msg2) != 0)
    {
        fprintf(stderr, "Message mismatch for fd2: expected \"%s\", got \"%s\"\n", msg2, buffer2);
        close(fd1);
        close(fd2);
        return 1;
    }

    close(fd1);
    close(fd2);
    return 0;
}

int two_processes_on_device_test()
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0)
    {
        // Child process
        int fd;
        char *msg = "Child process message";
        fd = open(DEVICE_FILE_PATH "0", O_RDWR);
        if (fd < 0)
        {
            perror("open child");
            exit(1);
        }

        if (ioctl(fd, MSG_SLOT_CHANNEL, 1) < 0)
        {
            perror("ioctl child");
            close(fd);
            exit(1);
        }

        if (write(fd, msg, strlen(msg)) != strlen(msg))
        {
            perror("write child");
            close(fd);
            exit(1);
        }

        close(fd);
        exit(0);
    }
    else
    {
        // Parent process
        int fd;
        char buffer[BUF_LEN];
        char *msg = "Parent process message";
        ssize_t bytes_read;

        fd = open(DEVICE_FILE_PATH "0", O_RDWR);
        if (fd < 0)
        {
            perror("open parent");
            return 1;
        }

        if (ioctl(fd, MSG_SLOT_CHANNEL, 2) < 0)
        {
            perror("ioctl parent");
            close(fd);
            return 1;
        }

        if (write(fd, msg, strlen(msg)) != strlen(msg))
        {
            perror("write parent");
            close(fd);
            return 1;
        }

        waitpid(pid, &status, 0);

        if (WEXITSTATUS(status) != 0)
        {
            fprintf(stderr, "Child process failed\n");
            close(fd);
            return 1;
        }

        if (ioctl(fd, MSG_SLOT_CHANNEL, 1) < 0)
        {
            perror("ioctl parent read");
            close(fd);
            return 1;
        }

        bytes_read = read(fd, buffer, BUF_LEN);
        if (bytes_read < 0)
        {
            perror("read parent");
            close(fd);
            return 1;
        }

        buffer[bytes_read] = '\0';
        if (strcmp(buffer, "Child process message") != 0)
        {
            fprintf(stderr, "Message mismatch: expected \"Child process message\", got \"%s\"\n", buffer);
            close(fd);
            return 1;
        }

        close(fd);
        return 0;
    }
}

int ioctl_affects_open_fds_test()
{
    int fd1, fd2;
    char *msg = "Hello, Open FDs!";
    char buffer[BUF_LEN];
    ssize_t bytes_written, bytes_read;

    fd1 = open(DEVICE_FILE_PATH "0", O_RDWR);
    if (fd1 < 0)
    {
        perror("open fd1");
        return 1;
    }

    fd2 = open(DEVICE_FILE_PATH "0", O_RDWR);
    if (fd2 < 0)
    {
        perror("open fd2");
        close(fd1);
        return 1;
    }

    if (ioctl(fd1, MSG_SLOT_CHANNEL, 1) < 0)
    {
        perror("ioctl fd1");
        close(fd1);
        close(fd2);
        return 1;
    }

    bytes_written = write(fd1, msg, strlen(msg));
    if (bytes_written != strlen(msg))
    {
        perror("write fd1");
        close(fd1);
        close(fd2);
        return 1;
    }

    if (ioctl(fd2, MSG_SLOT_CHANNEL, 1) < 0)
    {
        perror("ioctl fd1");
        close(fd1);
        close(fd2);
        return 1;
    }

    bytes_read = read(fd2, buffer, BUF_LEN);
    if (bytes_read < 0)
    {
        perror("read fd2");
        close(fd1);
        close(fd2);
        return 1;
    }

    buffer[bytes_read] = '\0';
    if (strcmp(buffer, msg) != 0)
    {
        fprintf(stderr, "Message mismatch for fd2: expected \"%s\", got \"%s\"\n", msg, buffer);
        close(fd1);
        close(fd2);
        return 1;
    }

    close(fd1);
    close(fd2);
    return 0;
}

int ioctl_affects_not_yet_born_fds_test()
{
    int fd;
    char *msg = "Hello, unborn FDs!";
    char buffer[BUF_LEN];
    ssize_t bytes_written, bytes_read;

    fd = open(DEVICE_FILE_PATH "0", O_RDWR);
    if (fd < 0)
    {
        perror("open fd");
        return 1;
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, 10) < 0)
    {
        perror("ioctl fd");
        close(fd);
        return 1;
    }

    bytes_written = write(fd, msg, strlen(msg));
    if (bytes_written != strlen(msg))
    {
        perror("write fd");
        close(fd);
        return 1;
    }

    close(fd);

    fd = open(DEVICE_FILE_PATH "1", O_RDWR);
    if (fd < 0)
    {
        perror("open new fd");
        return 1;
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, 10) < 0)
    {
        perror("ioctl new fd");
        close(fd);
        return 1;
    }
    // write to the new fd
    bytes_written = write(fd, msg, strlen(msg));
    if (bytes_written != strlen(msg))
    {
        perror("write fd");
        close(fd);
        return 1;
    }

    bytes_read = read(fd, buffer, BUF_LEN);
    if (bytes_read < 0)
    {
        perror("read new fd");
        close(fd);
        return 1;
    }

    buffer[bytes_read] = '\0';
    if (strcmp(buffer, msg) != 0)
    {
        fprintf(stderr, "Message mismatch for new fd: expected \"%s\", got \"%s\"\n", msg, buffer);
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

int overwrite_test()
{
    int fd;
    char *msg1 = "First message";
    char *msg2 = "Second message";
    char buffer[BUF_LEN];
    ssize_t bytes_written, bytes_read;

    fd = open(DEVICE_FILE_PATH "0", O_RDWR);
    if (fd < 0)
    {
        perror("open fd");
        return 1;
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, 1) < 0)
    {
        perror("ioctl fd");
        close(fd);
        return 1;
    }

    bytes_written = write(fd, msg1, strlen(msg1));
    if (bytes_written != strlen(msg1))
    {
        perror("write msg1");
        close(fd);
        return 1;
    }

    bytes_written = write(fd, msg2, strlen(msg2));
    if (bytes_written != strlen(msg2))
    {
        perror("write msg2");
        close(fd);
        return 1;
    }

    bytes_read = read(fd, buffer, BUF_LEN);
    if (bytes_read < 0)
    {
        perror("read fd");
        close(fd);
        return 1;
    }

    buffer[bytes_read] = '\0';
    if (strcmp(buffer, msg2) != 0)
    {
        fprintf(stderr, "Message mismatch: expected \"%s\", got \"%s\"\n", msg2, buffer);
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

int read_msg_twice_test()
{
    int fd;
    char *msg = "Repeat message";
    char buffer[BUF_LEN];
    ssize_t bytes_written, bytes_read;

    fd = open(DEVICE_FILE_PATH "0", O_RDWR);
    if (fd < 0)
    {
        perror("open fd");
        return 1;
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, 1) < 0)
    {
        perror("ioctl fd");
        close(fd);
        return 1;
    }

    bytes_written = write(fd, msg, strlen(msg));
    if (bytes_written != strlen(msg))
    {
        perror("write fd");
        close(fd);
        return 1;
    }

    bytes_read = read(fd, buffer, BUF_LEN);
    if (bytes_read < 0)
    {
        perror("read fd");
        close(fd);
        return 1;
    }

    buffer[bytes_read] = '\0';
    if (strcmp(buffer, msg) != 0)
    {
        fprintf(stderr, "Message mismatch on first read: expected \"%s\", got \"%s\"\n", msg, buffer);
        close(fd);
        return 1;
    }

    bytes_read = read(fd, buffer, BUF_LEN);
    if (bytes_read < 0)
    {
        perror("read fd second time");
        close(fd);
        return 1;
    }

    buffer[bytes_read] = '\0';
    if (strcmp(buffer, msg) != 0)
    {
        fprintf(stderr, "Message mismatch on second read: expected \"%s\", got \"%s\"\n", msg, buffer);
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

int write_and_read_bytes_test()
{
    int fd;
    char *msg = "This is a UTF-8 encoded string";
    char buffer[BUF_LEN];
    ssize_t bytes_written, bytes_read;

    fd = open(DEVICE_FILE_PATH "0", O_RDWR);
    if (fd < 0)
    {
        perror("open fd");
        return 1;
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, 1) < 0)
    {
        perror("ioctl fd");
        close(fd);
        return 1;
    }

    bytes_written = write(fd, msg, strlen(msg));
    if (bytes_written != strlen(msg))
    {
        perror("write fd");
        close(fd);
        return 1;
    }

    bytes_read = read(fd, buffer, BUF_LEN);
    if (bytes_read < 0)
    {
        perror("read fd");
        close(fd);
        return 1;
    }

    buffer[bytes_read] = '\0';
    if (strcmp(buffer, msg) != 0)
    {
        fprintf(stderr, "Message mismatch: expected \"%s\", got \"%s\"\n", msg, buffer);
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

int reading_empty_channel_test()
{
    int fd;
    char buffer[BUF_LEN];
    ssize_t bytes_read;

    fd = open(DEVICE_FILE_PATH "0", O_RDWR);
    if (fd < 0)
    {
        perror("open fd");
        return 1;
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, 1) < 0)
    {
        perror("ioctl fd");
        close(fd);
        return 1;
    }

    bytes_read = read(fd, buffer, BUF_LEN);
    if (bytes_read >= 0 || errno != EWOULDBLOCK)
    {
        printf("bytes_read: %ld\n", bytes_read);
        printf("errno: %d\n", errno);
        fprintf(stderr, "Expected EWOULDBLOCK, but read succeeded or different error\n");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

int writing_0_size_msg_test()
{
    int fd;
    char *msg = "";
    ssize_t bytes_written;

    fd = open(DEVICE_FILE_PATH "0", O_RDWR);
    if (fd < 0)
    {
        perror("open fd");
        return 1;
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, 1) < 0)
    {
        perror("ioctl fd");
        close(fd);
        return 1;
    }

    bytes_written = write(fd, msg, 0);
    if (bytes_written >= 0 || errno != EMSGSIZE)
    {
        fprintf(stderr, "Expected EMSGSIZE, but write succeeded or different error\n");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

int writing_too_big_size_msg_test()
{
    int fd;
    char msg[BUF_LEN + 1];
    ssize_t bytes_written;

    memset(msg, 'A', BUF_LEN + 1);

    fd = open(DEVICE_FILE_PATH "0", O_RDWR);
    if (fd < 0)
    {
        perror("open fd");
        return 1;
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, 1) < 0)
    {
        perror("ioctl fd");
        close(fd);
        return 1;
    }

    bytes_written = write(fd, msg, BUF_LEN + 1);
    if (bytes_written >= 0 || errno != EMSGSIZE)
    {
        fprintf(stderr, "Expected EMSGSIZE, but write succeeded or different error\n");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

int write_with_no_channel_test()
{
    int fd;
    char *msg = "No channel set";
    ssize_t bytes_written;

    fd = open(DEVICE_FILE_PATH "0", O_RDWR);
    if (fd < 0)
    {
        perror("open fd");
        return 1;
    }

    bytes_written = write(fd, msg, strlen(msg));
    if (bytes_written >= 0 || errno != EINVAL)
    {
        fprintf(stderr, "Expected EINVAL, but write succeeded or different error\n");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

int read_with_no_channel_test()
{
    int fd;
    char buffer[BUF_LEN];
    ssize_t bytes_read;

    fd = open(DEVICE_FILE_PATH "0", O_RDWR);
    if (fd < 0)
    {
        perror("open fd");
        return 1;
    }

    bytes_read = read(fd, buffer, BUF_LEN);
    if (bytes_read >= 0 || errno != EINVAL)
    {
        fprintf(stderr, "Expected EINVAL, but read succeeded or different error\n");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

int read_to_insufficient_len_buff_err_test()
{
    int fd;
    char *msg = "A long message";
    char buffer[5];
    ssize_t bytes_written, bytes_read;

    fd = open(DEVICE_FILE_PATH "0", O_RDWR);
    if (fd < 0)
    {
        perror("open fd");
        return 1;
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, 1) < 0)
    {
        perror("ioctl fd");
        close(fd);
        return 1;
    }

    bytes_written = write(fd, msg, strlen(msg));
    if (bytes_written != strlen(msg))
    {
        perror("write fd");
        close(fd);
        return 1;
    }

    bytes_read = read(fd, buffer, 5);
    if (bytes_read >= 0 || errno != ENOSPC)
    {
        fprintf(stderr, "Expected ENOSPC, but read succeeded or different error\n");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

int main()
{

    run_test("Write_and_read_test", write_and_read_test);
    run_test("Sharing_channels_test", sharing_channels_test);
    run_test("Two_processes_on_device_test", two_processes_on_device_test);
    run_test("Ioctl_affects_open_fds_test", ioctl_affects_open_fds_test);
    run_test("Ioctl_affects_not_yet_born_fds_test", ioctl_affects_not_yet_born_fds_test);
    run_test("Overwrite_test", overwrite_test);
    run_test("Read_msg_twice", read_msg_twice_test);
    run_test("Write_and_read_bytes_test", write_and_read_bytes_test);
    run_test("Reading_empty_channel_test", reading_empty_channel_test);
    run_test("Writing_0_size_msg_test", writing_0_size_msg_test);
    run_test("Writing_too_big_size_msg_test", writing_too_big_size_msg_test);
    run_test("Write_with_no_channel_test", write_with_no_channel_test);
    run_test("Read_with_no_channel_test", read_with_no_channel_test);
    run_test("Read_to_insufficient_len_buff_err_test", read_to_insufficient_len_buff_err_test);

    return 0;
}