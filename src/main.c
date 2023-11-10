#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <SDL2/SDL.h>

#include <linux/videodev2.h>

#define BUF_COUNT 3

#define VIDEO_DEVICE "/dev/video0"

void closeCamera(int fd, void *buffer, size_t bufferSize) {
    munmap(buffer, bufferSize);
    close(fd);
}

int main() {
    int fd = open(VIDEO_DEVICE, O_RDWR | O_NONBLOCK, 0);
    if (fd == -1) {
        perror("Cannot open video device");
        return -1;
    }

    // query capabilities
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        perror("Cannot get video capabilities");
        close(fd);
        return -1;
    }

    printf("Driver: %s\nCard: %s\nBus: %s\nVersion: %u\nCapabilities: %08X\n",
           cap.driver, cap.card, cap.bus_info, cap.version, cap.capabilities);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "Device does not support video capturing\n");
        close(fd);
        return -1;
    }

    // set format
    struct v4l2_format fmt;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("Cannot set video fmt");
        close(fd);
        return -1;
    }

    size_t bufferSize = fmt.fmt.pix.sizeimage;

    // request buffers
    struct v4l2_requestbuffers req;
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (-1 == ioctl(fd, VIDIOC_REQBUFS, &req)) {
        perror("Cannot request buffers");
        close(fd);
        return -1;
    }

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    //buf.index = bufferindex;
    if(-1 == ioctl(fd, VIDIOC_QUERYBUF, &buf)) {
        perror("Cannot set video fmt");
        close(fd);
        return -1;
    }

    void *buffer = mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    if (buffer == MAP_FAILED) {
        perror("Cannot map video buffer");
        close(fd);
        return -1;
    }

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("Video Stream", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          fmt.fmt.pix.width, fmt.fmt.pix.height, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        closeCamera(fd, buffer, bufferSize);
        SDL_Quit();
        return -1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        closeCamera(fd, buffer, bufferSize);
        SDL_Quit();
        return -1;
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR32,
                                             SDL_TEXTUREACCESS_STREAMING, fmt.fmt.pix.width, fmt.fmt.pix.height);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        closeCamera(fd, buffer, bufferSize);
        SDL_Quit();
        return -1;
    }

    if (ioctl(fd, VIDIOC_STREAMON, &buf.type) == -1) {
        perror("Cannot start streaming");
        closeCamera(fd, buffer, bufferSize);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    SDL_Event e;
    int quit = 0;
    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
        }

        if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            perror("Cannot dequeue buffer");
            break;
        }

        // Процессинг и отображение кадра в текстуре
        SDL_UpdateTexture(texture, NULL, buffer, fmt.fmt.pix.width * 4);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        if (ioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Cannot enqueue buffer");
            break;
        }
    }

    if (ioctl(fd, VIDIOC_STREAMOFF, &buf.type) == -1) {
        perror("Cannot stop streaming");
    }

    closeCamera(fd, buffer, bufferSize);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
