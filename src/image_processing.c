// image_processing.c
#include "../include/image_processing/image_processing.h"
#include <stdio.h>

void process_frame(void *p, int width, int height) {
    // Пример более сложной обработки: инверсия цветов
    uint8_t *data = (const uint8_t *)p;

    for (int i = 0; i < width * 2 * height; i += 2) {
        // Инверсия цвета Y (яркости)
        uint8_t inverted_y = 255 - data[i];
        data[i] = inverted_y;

        // Цвет U и V остаются без изменений
    }
}
