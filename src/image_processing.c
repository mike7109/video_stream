// image_processing.c
#include "../include/image_processing/image_processing.h"
#include <stdio.h>

void process_frame(uint8_t *p, int width, int height) {
    // Пример более сложной обработки: инверсия цветов

    for (int i = 0; i < width * 2 * height; i += 2) {
        // Инверсия цвета Y (яркости)
        uint8_t inverted_y = 255 - p[i];
        p[i] = inverted_y;
    }
}
