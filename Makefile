
all: Makefile offscreen sdl-win grab-png grab-jpeg h264enc

CFLAGS=-Wall -g3 -I /usr/include/libdrm

init.c: init.picol
	awk 'BEGIN {print "char *inititp ="} {print "\"" $$0 "\\n\""} END {print ";"}' < $< > $@

offscreen: gltext.h picol.h Makefile
offscreen: offscreen.o init.o
	$(CC) -o $@ offscreen.o init.o `pkg-config --libs --cflags glesv2 egl gbm`

sdl-win: Makefile
sdl-win: sdl-win.o
	$(CC) -o $@ $< `sdl2-config --cflags --libs`

grab-png: Makefile
grab-png: grab-png.o
	$(CC) -o $@ $< -lpng

jpegenc.c: jpegenc_utils.h
	@touch $@

grab-png: Makefile
grab-jpeg: jpegenc.o va_display_drm.o
	$(CC) $(CFLAGS) jpegenc.o va_display_drm.o   -o $@ -lva -lva-drm -ldrm

h264enc: h264encode.o va_display_drm.o
	$(CC) $(CFLAGS) h264encode.o va_display_drm.o -o $@ -lva -lva-drm -ldrm -lm

clean:
	-rm *o
	-rm -f offscreen
	-rm -f sdl-win
	-rm -f grab-png
	-rm -f grab-jpeg
	-rm -f h264enc

.PHONY: clean
