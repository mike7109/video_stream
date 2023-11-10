#!/bin/bash

# Путь к вашей скомпилированной программе
EXECUTABLE=./bin/video_stream

# Проверяем, существует ли файл
if [ -f "$EXECUTABLE" ]; then
    $EXECUTABLE
else
    echo "Ошибка: файл $EXECUTABLE не найден. Скомпилируйте программу с помощью make."
fi
