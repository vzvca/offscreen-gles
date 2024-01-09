
all: Makefile offscreen sdl-win grab-png grab-jpeg h264enc h265enc

CFLAGS=-Wall -g3 -I /usr/include/libdrm

init.c: init.picol
	awk 'BEGIN {print "char *inititp ="} {print "\"" $$0 "\\n\""} END {print ";"}' < $< > $@

offscreen: Makefile
offscreen: gltext.h picol.h Makefile
offscreen: offscreen.o init.o
	$(CC) -o $@ offscreen.o init.o `pkg-config --libs --cflags glesv2 egl gbm`

sdl-win: Makefile
sdl-win: sdl-win.o
	$(CC) -o $@ $< `sdl2-config --cflags --libs`

grab-png: Makefile
grab-png: grab-png.o
	$(CC) -o $@ $< -lpng

grab-jpeg: Makefile
grab-jpeg: jpegenc_utils.h bitstream.h
grab-jpeg: jpegenc.o va_display_drm.o bitstream.o
	$(CC) $(CFLAGS) jpegenc.o va_display_drm.o bitstream.o -o $@ -lva -lva-drm -ldrm

h264enc: Makefile
h264enc: loadsurface.h bitstream.h
h264enc: h264encode.o va_display_drm.o bitstream.o
	$(CC) $(CFLAGS) h264encode.o va_display_drm.o bitstream.o -o $@ -lva -lva-drm -ldrm -lm

h265enc: Makefile
h265enc: loadsurface.h bitstream.h
h265enc: hevcencode.o va_display_drm.o bitstream.o
	$(CC) $(CFLAGS) hevcencode.o va_display_drm.o bitstream.o -o $@ -lva -lva-drm -ldrm -lpthread -lm

clean:
	-rm *o
	-rm -f offscreen
	-rm -f sdl-win
	-rm -f grab-png
	-rm -f grab-jpeg
	-rm -f h264enc
	-rm -f h265enc

.PHONY: clean
