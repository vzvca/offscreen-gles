
all: Makefile offscreen sdl-win grab-png grab-jpeg

CFLAGS=-Wall -g3
CXXFLAGS=-Wall

offscreen: gltext.h picol.h
offscreen: offscreen.c 
	$(CC) $(CFLAGS) -o $@ $< `pkg-config --libs --cflags glesv2 egl gbm`

sdl-win: sdl-win.c
	$(CC) $(CFLAGS) -o $@ $< `sdl2-config --cflags --libs`

grab-png: grab-png.c
	$(CC) $(CFLAGS) -o $@ $< -lpng

jpegenc.c: jpegenc_utils.h
	@touch $@

grab-jpeg: jpegenc.c va_display_drm.c
	$(CC) $(CFLAGS) $^ -I /usr/include/libdrm -o $@ -lva -lva-drm -ldrm -g3

h264enc: h264encode.c va_display_drm.c
	$(CC) $(CFLAGS) $^ -I /usr/include/libdrm -o $@ -lva -lva-drm -ldrm -lm -g3

clean:
	-rm -f offscreen
	-rm -f sdl-win
	-rm -f grab-png
	-rm -f grab-jpeg

.PHONY: clean
