```bash
sudo ffmpeg -f v4l2 -i /dev/video0 -pix_fmt bgra -vf 'format=bgra' -f sdl 'SDL output window'
```

```bash
sudo apt-get install libv4l-dev
sudo apt-get install libsdl2-dev

sudo apt-get install libgl1-mesa-glx
export LIBGL_ALWAYS_SOFTWARE=1
```

