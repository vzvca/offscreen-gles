# Offscreen OpenGL/GLES rendering

This example program shows how to do OpenGL/GLES rendering without any windowing system.
Should work on headless system provided that `/dev/dri/renderD128` device exists.

Code shown her:

* is inspired by work found ![here](https://github.com/elima/gpu-playground.git).
* uses glText from ![here](https://github.com/vallentin/glText.git).
* uses shaders found ![here](https://glslsandbox.com)


There are 3 distinct programs:

* **offscreen**: does the rendering and stores the images (RGBA32 pixels) in a memory mapped file (defaults to `/tmp/frame`). Images are generated at a given frame rate (default to 20 fps).
* **grab-png**: takes a screenshot in PNG by reading the file filled by **offscreen**.
* **sdl-win**: Read images at a given framerate (default to 20) from the file written by **offscreen** which is memory mapped.

## Usage

### offscreen

    $ offscreen -?
    usage: ./offscreen [-w width] [-h height] [-f framerate] [-o /path/to/file] [-s /path/to/fragment-shader]
        -?                        Print this help message.
        -h height                 Desired image height. Defaults to 720
        -w width                  Desired image width. Defaults to 576
        -f fps                    Number of images per second. Defaults to 20.
        -o /path/to/file          Path of mmap-ed file that will hold image. Defaults to '/tmp/frame'.
        -s /path/to/file          Path of of fragent shader. Defaults to 'shaders/plasma.frag'

### sdl-win

    $ ./sdl-win -?
    usage: ./sdl-win [-?] -i file [-f fps] [-x x] [-y y] [-w width] [-h height]
        -?                        Prints this message.
        -i                        Sets input video frame file (default /tmp/frame).
        -f                        Sets the video framerate (default 20).
        -w                        Sets the width of the image (default 720).
        -h                        Sets the height of the image (default 576).
        -x                        Sets the x position of the window (default 100).
        -y                        Sets the y position of the window (default 100).

### grap-png

    $ ./grab-png -?
    usage: ./grab-png [-?] -i file -o file [-w width] [-h height]
        -?                        Prints this message.
        -i                        Sets input video frame file (default /tmp/frame).
        -o                        Set output PNG file name (default /tmp/capture.png).
        -w                        Sets the width of the image (default 720).
        -h                        Sets the height of the image (default 576).

## Demo

In a first terminal run `offscreen`:

   $ ./offscreen -s shaders/voronoi.frag


In a second terminal run `sdl-win` which will displayed an animated graphic:

   $ ./sdl-win

In the first terminal, the `offscreen` prompt `=>` is displayed.
Type `help` to see the list of available commands and `help <<command>>` to get help on a specific command..

Framerate and shader can be changed dynamically using `fps` and `shader` command.
The message printed on the video can be changed using `message` command.


## Building

Type `make` to build all the programs.

There are some prerequisites, given here on debian 10 buster used during development.

* `libpng-dev`
* `libsdl2-dev`
* `libgb-dev`
* `libgles2-mesa-dev`
* `libegl1-mesa-dev`

## Windows WSL2

It is possible to use the `offscreen` program under WSL2 (tested on ubuntu-20.04).
On this system the device `/dev/dri/renderD128` is present and can be used.
Probably mesa will not be able to initialize properly without help and you will get a message like this:

    MESA-LOADER: failed to open vgem: /usr/lib/dri/vgem_dri.so: cannot open shared object file: No such file or directory (search paths /usr/lib/x86_64-linux-gnu/dri:\$${ORIGIN}/dri:/usr/lib/dri, suffix _dri)
    failed to load driver: vgem

To circumvent this issue, you need to help mesa to choose the right libray to load. For example:

    export MESA_LOADER_DRIVER_OVERRIDE=i965

It is possible to use `sdl-win` too. It has been tested using `MobaXterm X11 server` with moba version 21.2, the following server modes works (others don't):

* "Windowed mode": X11 server constrained to a single container window
* "Windowed mode with DWM": X11 server with DWM desktop in a container window
* "Windowed mode with Tvwm desktop": X11 server and Fvwm desktop in a container window
* "Rootless": Transparent X11 server with Fvwm window borders (experimantal)

`grab-png` wroks out of the box.

## Future directions

* Add command interface to offscreen to allow:

   * dynamic change of shader
   * viewpoint change
   * ...

* Add to **offscreen** the ability to send a signal to client programs when a frame is ready
* Add circular buffer of frames. At least 2.
* Add RTP raw video streaming as an external program which consumes frames (like `sdl-win` and `grap-png`).
* Add H264/H265 video RTP streaming backend using `ffmpeg` or `x264` + `live555` or `vaapi` + `live555`.




