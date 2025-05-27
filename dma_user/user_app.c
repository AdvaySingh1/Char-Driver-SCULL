#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#define DEVICE_FILE "/dev/simple_dma"
#define DMA_BUFFER_SIZE (4 * 4096) // Must match kernel module (4 pages)
#define SIMPLE_DMA_MAGIC 's'
#define SIMPLE_DMA_START_TRANSFER _IO(SIMPLE_DMA_MAGIC, 1)

int main() {
    int fd;
    char *dma_buffer_user = NULL;

    // 1. Open the character device
    fd = open(DEVICE_FILE, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device file");
        return EXIT_FAILURE;
    }

    printf("Device file %s opened successfully.\n", DEVICE_FILE);

    // 2. Map the DMA buffer into user space
    dma_buffer_user = mmap(NULL, DMA_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (dma_buffer_user == MAP_FAILED) {
        perror("Failed to mmap DMA buffer");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("DMA buffer mapped to user space address: %p\n", dma_buffer_user);

    // 3. Write some data to the mapped buffer from user space
    const char *message = "Hello from user space!";
    size_t message_len = strlen(message) + 1; // Include null terminator

    if (message_len > DMA_BUFFER_SIZE) {
        fprintf(stderr, "Message is too large for the DMA buffer.\n");
        munmap(dma_buffer_user, DMA_BUFFER_SIZE);
        close(fd);
        return EXIT_FAILURE;
    }

    // Ensure the buffer is clear before writing
    memset(dma_buffer_user, 0, DMA_BUFFER_SIZE);
    memcpy(dma_buffer_user, message, message_len);

    printf("Wrote message to DMA buffer: \"%s\"\n", dma_buffer_user);

    // 4. Tell the kernel module to perform the "DMA transfer" via ioctl
    printf("Sending ioctl to trigger simulated DMA transfer...\n");
    if (ioctl(fd, SIMPLE_DMA_START_TRANSFER, 0) < 0) {
        perror("Failed to send ioctl command");
        munmap(dma_buffer_user, DMA_BUFFER_SIZE);
        close(fd);
        return EXIT_FAILURE;
    }

    printf("ioctl sent. Kernel should have simulated DMA (reversed data).\n");

    // 5. Read the data back from the mapped buffer (after the simulated DMA)
    printf("Reading data from DMA buffer after simulated DMA:\n");
    // The kernel module reversed the data, so we expect to see the reversed string.
    printf("Data in buffer: \"%s\"\n", dma_buffer_user);

    // 6. Unmap the buffer and close the device file
    if (munmap(dma_buffer_user, DMA_BUFFER_SIZE) < 0) {
        perror("Failed to munmap DMA buffer");
    }

    close(fd);
    printf("Device file closed.\n");

    return EXIT_SUCCESS;
}