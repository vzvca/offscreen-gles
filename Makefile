
all: Makefile offscreen sdl-win grab-png

CFLAGS=-Wall -g3
CXXFLAGS=-Wall

offscreen: offscreen.c
	$(CC) $(CFLAGS) -o $@ $< `pkg-config --libs --cflags glesv2 egl gbm`

sdl-win: sdl-win.c
	$(CC) $(CFLAGS) -o $@ $< `sdl2-config --cflags --libs`

grab-png: grab-png.c
	$(CC) $(CFLAGS) -o $@ $< -lpng

clean:
	-rm -f offscreen
	-rm -f sdl-win
	-rm -f grab-png

.PHONY: clean
