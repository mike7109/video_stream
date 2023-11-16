#include <SDL2/SDL.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <errno.h>

#include "../include/image_processing/image_processing.h"

#define WIDTH 640
#define HEIGHT 480
#define NUM_CAMERAS 4
#define NUM_BUFFERS 1

struct Buffer {
    void *start;
    size_t length;
};

struct Camera {
    int fd;
    struct Buffer *buffers;
};

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *textures[NUM_CAMERAS];

void handleError(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

void initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        handleError("SDL initialization failed");
    }

    window = SDL_CreateWindow("Camera Display", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH * 2,
                              HEIGHT * 2, SDL_WINDOW_SHOWN);
    if (!window) {
        handleError("SDL window creation failed");
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        handleError("SDL renderer creation failed");
    }
}

void initCamera(struct Camera *camera, const char *dev_name) {
    camera->fd = open(dev_name, O_RDWR);
    if (camera->fd == -1) {
        handleError("Error opening camera device");
    }

    struct v4l2_capability cap;
    if (-1 == ioctl(camera->fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s is no V4L2 device\n", dev_name);
            exit(EXIT_FAILURE);
        } else {
            handleError("VIDIOC_QUERYCAP");
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "%s is no video capture device\n", dev_name);
        exit(EXIT_FAILURE);
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "%s does not support streaming i/o\n", dev_name);
        exit(EXIT_FAILURE);
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;

    if (ioctl(camera->fd, VIDIOC_S_FMT, &fmt) == -1) {
        handleError("Error setting video format");
    }

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = NUM_BUFFERS;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(camera->fd, VIDIOC_REQBUFS, &req) == -1) {
        handleError("Error requesting buffers");
    }

    camera->buffers = calloc(req.count, sizeof(*camera->buffers));
    if (!camera->buffers) {
        handleError("Error allocating memory for buffers");
    }

    for (unsigned int i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(camera->fd, VIDIOC_QUERYBUF, &buf) == -1) {
            handleError("Error VIDIOC_QBUF");
        }

        camera->buffers->length = buf.length;
        camera->buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, camera->fd, buf.m.offset);

        if (camera->buffers[i].start == MAP_FAILED) {
            handleError("Error mapping buffer");
        }
    }
}

void initCameras(struct Camera *cameras) {
    char dev_name[13];
    for (int i = 0; i < NUM_CAMERAS; ++i) {
        if (i == 0)
            sprintf(dev_name, "/dev/video0");
        if (i == 1)
            sprintf(dev_name, "/dev/video2");
        if (i == 2)
            sprintf(dev_name, "/dev/video4");
        if (i == 3)
            sprintf(dev_name, "/dev/video6");

        initCamera(&cameras[i], dev_name);
    }
}

void initTextures() {
    for (int i = 0; i < NUM_CAMERAS; ++i) {
        textures[i] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YUY2, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
        if (!textures[i]) {
            handleError("Error creating SDL texture");
        }
    }
}

void startCapturing(struct Camera *cameras) {
    for (int i = 0; i < NUM_CAMERAS; ++i) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (-1 == ioctl(cameras[i].fd, VIDIOC_STREAMON, &type))
            handleError("VIDIOC_STREAMON");

        struct v4l2_buffer buf;
        for (int j = 0; j < NUM_BUFFERS; j++) {
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = j; // Указываем индекс буфера

            // Поместите буфер в очередь для захвата
            if (ioctl(cameras[i].fd, VIDIOC_QBUF, &buf) == -1) {
                handleError("Error VIDIOC_QBUF");
            }
        }

    }
}

void updateTexture(struct Camera *camera, SDL_Texture *texture) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(camera->fd, VIDIOC_DQBUF, &buf) == -1) {
        handleError("Error dequeuing buffer");
    }

    //process_frame(camera->buffers[buf.index].start, WIDTH, HEIGHT);

    SDL_UpdateTexture(texture, NULL, camera->buffers[buf.index].start, WIDTH * 2);

    if (ioctl(camera->fd, VIDIOC_QBUF, &buf) == -1) {
        handleError("Error enqueuing buffer");
    }
}

void displayCameras(struct Camera *cameras) {
    SDL_Event event;
    int quit = 0;
    while (!quit) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = 1;
            }
        }

        for (int i = 0; i < NUM_CAMERAS; ++i) {
            updateTexture(&cameras[i], textures[i]);
        }

        SDL_RenderClear(renderer);

        for (int i = 0; i < NUM_CAMERAS; ++i) {
            if (i < 2) {
                SDL_RenderCopy(renderer, textures[i], NULL, &(SDL_Rect){i * WIDTH, 0, WIDTH, HEIGHT});
            } else {
                SDL_RenderCopy(renderer, textures[i], NULL, &(SDL_Rect){(i-2) * WIDTH, HEIGHT, WIDTH, HEIGHT});
            }

        }

        SDL_RenderPresent(renderer);
    }
}

void stopCapturing(struct Camera *cameras) {
    for (int i = 0; i < NUM_CAMERAS; ++i) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(cameras[i].fd, VIDIOC_STREAMOFF, &type) == -1) {
            handleError("Error stopping capture");
        }
    }
}

void closeCameras(struct Camera *cameras) {
    for (int i = 0; i < NUM_CAMERAS; ++i) {
        munmap(cameras[i].buffers->start, cameras[i].buffers->length);
        free(cameras[i].buffers);
        if (close(cameras[i].fd) == -1) {
            handleError("Error closing camera device");
        }
    }
}

void cleanup() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main() {
    struct Camera cameras[NUM_CAMERAS];

    initSDL();
    initCameras(cameras);
    initTextures();
    startCapturing(cameras);
    displayCameras(cameras);
    stopCapturing(cameras);
    closeCameras(cameras);
    cleanup();

    return 0;
}
