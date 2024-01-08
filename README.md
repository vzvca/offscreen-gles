# Offscreen OpenGL/GLES rendering

These examples programs show:

* how to do OpenGL/GLES rendering without any windowing system,
* how to retrieve the video and play it in a window
* how to take video snapshots in PNG or JPEG. For JPEG it uses hardware acceleration for picture encoding.

Should work on headless system provided that `/dev/dri/renderD128` device exists.

Code shown here:

* is inspired by work found [here](https://github.com/elima/gpu-playground.git)
* uses glText from [here](https://github.com/vallentin/glText.git)
* uses shaders found [here](https://glslsandbox.com)
* uses huge code chunks from [libva-utils](https://github.com/intel/libva-utils).
* uses a tiny TCL interpreter from [picol](https://github.com/dbohdan/picol).

There are 5 distinct programs:

* **offscreen**: does the rendering and stores the images (RGBA32 pixels) in a memory mapped file (defaults to `/tmp/frame`). Images are generated at a given frame rate (default to 20 fps).
* **grab-png**: takes a screenshot in PNG by reading the file filled by **offscreen**.
* **grab-jpeg**: takes a screenshot in JPEG by reading the file filled by **offscreen**. It uses `vaapi` (Video Acceleration API) to delegate JPEG computation to the hardware.
* **h264enc**: Encode generated frames as an h264 raw video file.
* **sdl-win**: Read images at a given framerate (default to 20) from the file written by **offscreen** which is memory mapped.

Apart for `sdl-win` it is easier to start `grab-png`, `grab-jpeg` and `h264enc` from `offscreen` (see below).

## Usage

### offscreen

This program performs hardware accelerated video rendering using GL fragment shaders.

    $ offscreen -?
    usage: ./offscreen [-w width] [-h height] [-f framerate] [-o /path/to/file] [-s /path/to/fragment-shader]
        -?                        Print this help message.
        -h height                 Desired image height. Defaults to 720
        -w width                  Desired image width. Defaults to 576
        -f fps                    Number of images per second. Defaults to 20.
        -o /path/to/file          Path of mmap-ed file that will hold image. Defaults to '/tmp/frame'.
        -s /path/to/file          Path of of fragent shader. Defaults to 'shaders/plasma.frag'

There is a crude command interface which is based on a tiny TCL interpreter. If started from a terminal the program displays a prompt.

    $ ./offscreen 
    The output file was opened successfully.
    The output file was mapped to memory successfully.
    offscreen renderer cli. Type 'help' to see available commands.
    => help
    colorspace ?rgb/yuv?
    fps ?frame-per-second?
    kill ?add/rm pid?
    message ?msg?
    mouse ?x y?
    shader ?/path/to/fragment-shader?
    stats
    help ?topic?
    quit ?status?

    => 

There are commands for taking snapshots (jpeg and png) and recording video, at `offscreen` prompt enter:

    => png /path/to/capture.png
    => jpeg /path/to/capture.jpeg
    => h264enc /path/to/video.h264 1000  ;# will record 1000 video frames

Note that jpeg and h264 use hardware acceleration using VAAPI.


### sdl-win

    $ ./sdl-win -?
    usage: ./sdl-win [-?] -i file [-f fps] [-x x] [-y y] [-w width] [-h height]
        -?                        Prints this message.
        -i                        Set input video frame file (default /tmp/frame).
        -f                        Set the video framerate (default 20).
        -w                        Set the width of the image (default 720).
        -h                        Set the height of the image (default 576).
        -x                        Set the x position of the window (default 100).
        -y                        Set the y position of the window (default 100).

### grap-png

    $ ./grab-png -?
    usage: ./grab-png [-?] -i file -o file [-w width] [-h height]
        -?                        Prints this message.
        -i                        Set input video frame file (default /tmp/frame).
        -o                        Set output PNG file name (default /tmp/capture.png).
        -w                        Set the width of the image (default 720).
        -h                        Set the height of the image (default 576).
        -s                        Encode input on SIGUSR1 signal. Wait at most 10 sec for signal.

### grab-jpeg

    $ ./grab-jpeg -?
    usage: ./grab-jpeg [-?] -i file -o file [-w width] [-h height]
        -?                        Prints this message.
        -i                        Set input video frame file (default /tmp/frame).
        -o                        Set output JPEG file name (default /tmp/capture.jpeg).
        -w                        Set the width of the image (default 720).
        -h                        Set the height of the image (default 576).
        -f                        Set 4CC value 0(I420)/1(NV12)/2(UYVY)/3(YUY2)/4(Y8)/5(RGBA) (default 5).
        -q                        Set quality of the image (default 50).
        -s                        Encode input on SIGUSR1 signal. Wait at most 10 sec for signal.
	
    Example: ./grab-jpeg -w 1024 -h 768 -i input_file.yuv -o output.jpeg -f 0 -q 50

For the moment, only 4CC RGBA is supported. But it is not real RGBA, you need to perform rendering in YUV colorspace by entering `colorspace yuv` at `offscreen` command prompt.

### h264enc

    $ ./h264enc -?
    ./h264encode <options>
       -w <width> -h <height>
       -framecount <frame number>
       -n <frame number>
	  if set to 0 and srcyuv is set, the frame count is from srcuv file
       -o <coded file>
       -f <frame rate>
       --intra_period <number>
       --idr_period <number>
       --ip_period <number>
       --bitrate <bitrate>
       --initialqp <number>
       --minqp <number>
       --rcmode <NONE|CBR|VBR|VCM|CQP|VBR_CONTRAINED>
       --srcyuv <filename> load YUV from a file
       --entropy <0|1>, 1 means cabac, 0 cavlc
       --profile <BP|MP|HP>
       --low_power <num> 0: Normal mode, 1: Low power mode, others: auto mode


## Demo

In a first terminal run `offscreen`:

   $ ./offscreen -w 800 -h 600 -s shaders/voronoi.frag


In a second terminal run `sdl-win` which will displayed an animated graphic:

   $ ./sdl-win -w 800 -h 600

You should see something like this:

![voronoi shader](images/image-1.png)

In the first terminal, the `offscreen` prompt `=>` is displayed.
Type `help` to see the list of available commands and `help <<command>>` to get help on a specific command..

Framerate and shader can be changed dynamically using `fps` and `shader` command.
The message printed on the video can be changed using `message` command.


## Building

Type `make` to build all the programs.

There are some prerequisites, given here on debian 10 buster used during development.

* `libpng-dev`
* `libsdl2-dev`
* `libgbm-dev`
* `libgles2-mesa-dev`
* `libegl1-mesa-dev`
* `liva-dev`

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

`grab-png` works out of the box.


## VAAPI video encoding

Starting point is [libva-utils](https://github.com/intel/libva-utils)

For `i965` driver, the following package from `non-free` section needs to be installed.
    $ sudo apt-get install i965-va-driver-shaders


## Future directions

* Add circular buffer of frames. At least 2.
* Add RTP raw video streaming as an external program which consumes frames (like `sdl-win` and `grap-png`).
* Add H264/H265 video RTP streaming backend using `ffmpeg` or `x264` + `live555` or `vaapi` + `live555`.




