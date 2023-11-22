/*
 * MIT License
 *
 * Copyright (c) 2023 vzvca
 *
 * C code inspired by:
 *    https://github.com/elima/gpu-playground.git with MIT license
 *
 * Shaders for glslsandbox
 *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

/*
 * Graphic headers
 */
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>


//--------------------------------------------------------------------------
//  constants
//--------------------------------------------------------------------------
#define BLKSZ 4096
#define DEF_WVID 720
#define DEF_HVID 576
#define DEF_FPS 20
#define DEF_OUTPUT "/tmp/frame"
#define DEF_SHADER "shaders/plasma.frag"

#define YUV 1
#define RGB 0

//--------------------------------------------------------------------------
//  Vertex Shader source code
//--------------------------------------------------------------------------
#define VERTEX_SHADER_SRC "attribute vec3 position; void main() { gl_Position = vec4(position,1.0); }"


//--------------------------------------------------------------------------
//  Image data structure
//--------------------------------------------------------------------------
struct image_s {
  uint32_t w, h, stride;
  uint32_t  *pixels;
};
typedef struct image_s image_t;

//--------------------------------------------------------------------------
//  Globals
//--------------------------------------------------------------------------
int          g_done = 0;
int          g_width  = DEF_WVID;
int          g_height = DEF_HVID;
int          g_mouse_x = 0;
int          g_mouse_y = 0;
int          g_colorspace = RGB;
const char*  g_out = DEF_OUTPUT;
const char*  g_shader = DEF_SHADER;
int          g_fps = DEF_FPS;
image_t      g_im;
image_t*     g_pim = &g_im;
int          g_gbmfd = -1;
int          g_outfd = -1;
struct gbm_device*  g_gbm = NULL;


static const GLfloat points[] = {
   // front
   -1.0f, -1.0f, +1.0f,
   +1.0f, -1.0f, +1.0f,
   -1.0f, +1.0f, +1.0f,
   +1.0f, +1.0f, +1.0f
};

// --------------------------------------------------------------------------
//   Forward
// --------------------------------------------------------------------------
int eval (char* cmd);

// --------------------------------------------------------------------------
//   Helper func
// --------------------------------------------------------------------------
int nblk(int w, int h)
{
   int sz = (w*h*4 + BLKSZ-1)/BLKSZ;
   return sz;
}

// --------------------------------------------------------------------------
//   Prepare output file and mmap it
// --------------------------------------------------------------------------
int initout( const char *file, int width, int height, image_t *img )
{
  unsigned char block[BLKSZ];
  int i, ni, fbfd = -1;
  char *fbp = 0;
  
  // Open the file for reading and writing
  fbfd = open (file, O_RDWR | O_CREAT, S_IRWXU);
  if (fbfd == -1) {
    perror ("Error: cannot open output file");
    exit (1);
  }
  printf("The output file was opened successfully.\n");

  // fill image with 0
  bzero (block, sizeof(block));
  ni = nblk (width,height);
  for( i = 0; i < ni; ++i ) {
    if ( -1 == write (fbfd, block, sizeof(block)) ) {
      perror ("Error: writing output file.");
      exit (1);
    }
  }

  // Map the device to memory
  fbp = (char *) mmap (0, ni * sizeof(block), PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
  if (*(int*)fbp == -1) {
    perror("Error: failed to map output file to memory");
    exit(1);
  }
  printf("The output file was mapped to memory successfully.\n");
  
  img->w = width;
  img->h = height;
  img->stride = width;
  img->pixels  = (uint32_t*) fbp;
    
  return fbfd;
}

// --------------------------------------------------------------------------
//   Assertion code
// --------------------------------------------------------------------------
void assertOpenGLError(const char* msg) {
   GLenum error = glGetError ();

   if (error != GL_NO_ERROR) {
      fprintf (stderr, "OpenGL error 0x%x %s\n", error, msg);
      exit (1);
   }
}

// --------------------------------------------------------------------------
//   Assertion code
// --------------------------------------------------------------------------
void assertEGLError(const char* msg) {
   EGLint error = eglGetError ();

   if (error != EGL_SUCCESS) {
      fprintf (stderr, "EGL error 0x%x %s\n", error, msg);
      exit (1);
   }
}

// --------------------------------------------------------------------------
//   Create shader from string content
// --------------------------------------------------------------------------
GLuint mkshader(GLuint type, const char *src, GLint len)
{
   GLuint vsh = glCreateShader (type);
   assertOpenGLError ("glCreateShader");

   if (len == -1) len = strlen (src);
   glShaderSource (vsh, 1, (const GLchar **) &src, &len);
   assertOpenGLError ("glShaderSource");

   glCompileShader (vsh);
   assertOpenGLError ("glCompileShader");

   char log[BLKSZ];
   int  loglen = sizeof (log);
   log[0] = 0;
   glGetShaderInfoLog (vsh, sizeof(log), &loglen, log);
   if (loglen) {
      puts ("Shader compilation log:");
      puts (log);
   }

   return vsh;
}

// --------------------------------------------------------------------------
//   Create shader from file content
// --------------------------------------------------------------------------
GLuint mkshaderf (GLuint type, const char *fname)
{
   GLint len;
   char src[8*BLKSZ];

   FILE *fin = fopen (fname,"r");
   if (fin == NULL) {
      fprintf (stderr, "Can't open file %s\n", fname);
      exit (1);
   }
   len = fread (src, 1, sizeof(src), fin);
   fclose (fin);

   return mkshader (type, (const char*) src, len);
}

int glinit()
{
  return 0;
}

// --------------------------------------------------------------------------
//   Wait a few milliseconds keeping an eye on stdin
// --------------------------------------------------------------------------
int waitmsec (int msec)
{
  struct timeval start, now;
  struct pollfd fds[1];
  int diff, ret;

  gettimeofday (&start, NULL);
  
  fds[0].fd = fileno(stdin);
  fds[0].events = POLLIN;

 again:
  // compute how log time to wait
  gettimeofday (&now, NULL);
  diff = ((now.tv_sec - start.tv_sec)*1000000 + (now.tv_usec - start.tv_usec))/1000;
  msec -= diff;
  if (msec <= 0) return 0;
  
  ret = poll (fds, 1, msec - diff);
  if (ret > 0) {
    if (fds[0].revents & POLLIN) {
      char line[BLKSZ/4], *sline = line;
      int sz;
      
    doread:
      sz = read (fileno(stdin), sline, sizeof(line) - (sline-line));
      if (sz == -1) {
	// read error ! leave
	perror ("read()");
	exit (1);
      }
      if (sz == 0) {
	// stdin closed ! leave
	fprintf (stderr, "stdin closed.\n");
	exit (1);
      }
      else {
	char *p, *s = line;
      parse:
	for (p = s; (p - sline) < sz && *p != '\n'; ++p);
	if (*p == '\n') {
	  *p = 0;
	  eval (s);
	  ++p;
	  if (p - sline < sz) {
	    s = p;
	    goto parse;
	  }
	}
	else {
	  // line not complete - read more input or leave
	  if (s == line) {
	    fprintf (stderr, "line too long.\n");
	    exit (1);
	  }
	  else {
	    memcpy (line, s, p-s);
	    sline = line + (p-s);
	    goto doread;
	  }
	}
      }
    }
    else {
      // not supposed to happen ! leave
      fprintf (stderr, "Unexpected poll() result.\n");
      exit (1);
    }
  }
  else if (ret == -1) {
    if (errno == EINTR) {
      if (g_done) return 0;
      goto again;
    }
    else {
      // treat other errors as fatal
      perror ("poll()");
      exit (1);
    }
  }
  return 0;
}
  
// --------------------------------------------------------------------------
//   GL initialisation and rendering
// --------------------------------------------------------------------------
int renderloop(int width, int height) {
   /*
    * EGL initialization and OpenGL context creation.
    */
   EGLDisplay display;
   EGLConfig config;
   EGLContext context;
   EGLint num_config;

   g_gbmfd = open ("/dev/dri/renderD128", O_RDWR);
   assert (g_gbmfd > 0);

   g_gbm = gbm_create_device (g_gbmfd);
   assert (g_gbm != NULL);

   /* setup EGL from the GBM device */
   display = eglGetPlatformDisplay (EGL_PLATFORM_GBM_MESA, g_gbm, NULL);
   //display = eglGetDisplay (EGL_DEFAULT_DISPLAY);
   assert (display != NULL);
   assertEGLError ("eglGetDisplay");

   eglInitialize (display, NULL, NULL);
   assertEGLError ("eglInitialize");

   eglChooseConfig (display, NULL, &config, 1, &num_config);
   assertEGLError ("eglChooseConfig");

   eglBindAPI (EGL_OPENGL_ES_API);
   assertEGLError ("eglBindAPI");

   static const EGLint attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 3,
      EGL_NONE
   };
        
   context = eglCreateContext (display, config, EGL_NO_CONTEXT, attribs);
   assertEGLError ("eglCreateContext");

   eglMakeCurrent (display, NULL, NULL, context);
   assertEGLError ("eglMakeCurrent");

   /*
    * Create an OpenGL framebuffer as render target.
    */
   GLuint frameBuffer;
   glGenFramebuffers (1, &frameBuffer);
   glBindFramebuffer (GL_FRAMEBUFFER, frameBuffer);
   assertOpenGLError ("glBindFramebuffer");

   /*
    * Create a texture as color attachment.
    */
   GLuint t;
   glGenTextures(1, &t);

   glBindTexture (GL_TEXTURE_2D, t);
   glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   assertOpenGLError ("glTexImage2D");

   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   /*
    * Attach the texture to the framebuffer.
    */
   glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t, 0);
   assertOpenGLError ("glFramebufferTexture2D");

   /*
    * Create VBO
    */
   GLuint vbo = 0;
   glGenBuffers (1, &vbo);
   assertOpenGLError ("glGenBuffers");
   glBindBuffer (GL_ARRAY_BUFFER, vbo);
   assertOpenGLError ("glBindBuffer");
   glBufferData (GL_ARRAY_BUFFER, 12*sizeof(GLfloat), points, GL_STATIC_DRAW);
   assertOpenGLError ("glBufferData");

   GLuint vao = 0;
   glGenVertexArrays (1, &vao);
   assertOpenGLError ("glGenVertexArrays");
   glBindVertexArray (vao);
   assertOpenGLError ("glBindVertexArray");
   glEnableVertexAttribArray (0);
   assertOpenGLError ("glEnableVertexArrayAttrib");
   glBindBuffer (GL_ARRAY_BUFFER, vbo);
   assertOpenGLError ("glBindBuffer");
   glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
   assertOpenGLError ("glVertexAttribPointer");

   /*
    * Compile shader
    */

   GLuint vsh = mkshader (GL_VERTEX_SHADER, VERTEX_SHADER_SRC, -1);
   GLuint fsh = mkshaderf (GL_FRAGMENT_SHADER, g_shader);

   GLuint prog = glCreateProgram ();
   assertOpenGLError ("glCreateProgram");
   glAttachShader (prog, vsh);
   assertOpenGLError ("glAttachShader VS");
   glAttachShader (prog, fsh);
   assertOpenGLError ("glAttachShader FS");
   glLinkProgram (prog);
   assertOpenGLError ("glLinkProgram");
   puts ("Program log");
   char msg[2048] = {0};
   int msglen;
   glGetProgramInfoLog (prog, sizeof(msg), &msglen, msg);
   puts (msg);

   glUseProgram (prog);
   assertOpenGLError ("glUseProgram");

   /*
    * Bind uniforms
    */
   GLint u_time = glGetUniformLocation (prog, "time");
   GLint u_mouse = glGetUniformLocation (prog, "mouse");
   GLint u_resolution = glGetUniformLocation (prog, "resolution");
   
   /*
    * Rendering loop
    */
   glViewport (0,0,width,height);
   assertOpenGLError ("glViewport");

   if (u_resolution != -1) {
      glUniform2f (u_resolution, (GLfloat) g_width, (GLfloat) g_height);
   }

   
   GLfloat time = 0.0f;
   while (!g_done) {

      /* modify value of uniform variables */
      if (u_time != -1) {
         glUniform1f (u_time, time);
      }
      if (u_mouse != -1) {
         glUniform2f (u_mouse, (GLfloat) g_mouse_x, (GLfloat) g_mouse_y);
      }
      
      glClear (GL_COLOR_BUFFER_BIT);
      assertOpenGLError ("glClear");

      glBindVertexArray (vao);
      assertOpenGLError ("glBindVertexArray");
      glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
      assertOpenGLError ("glDrawArrays");
   
      glFlush ();

      glReadPixels (0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, g_pim->pixels);

      waitmsec (1000/g_fps);
      time += 1.0/g_fps;
   }

   /*
    * Destroy context.
    */
   glDeleteFramebuffers (1, &frameBuffer);
   glDeleteTextures (1, &t);

   eglDestroyContext (display, context);
   assertEGLError ("eglDestroyContext");

   eglTerminate (display);
   assertEGLError ("eglTerminate");

   return 0;
}

// --------------------------------------------------------------------------
//   Build result and prints it
// --------------------------------------------------------------------------
int result (int code, char *fmt, ...)
{
  va_list va;
  va_start(va, fmt);
  vprintf(fmt, va);
  va_end(va);
  return code;
}

// --------------------------------------------------------------------------
//   Build error message regarding arguments number
// --------------------------------------------------------------------------
int wrong_num_args (int argc, char **argv, char *msg)
{
  char fmt[] = "%s wrong # args: %s %s %s %s %s %s";
  fmt[16 + 3*argc] = 0;
  switch (argc) {
  case 0: return result (1, fmt, argv[0], msg);
  case 1: return result (1, fmt, argv[0], argv[0], msg);
  case 2: return result (1, fmt, argv[0], argv[0], argv[1], msg);
  case 3: return result (1, fmt, argv[0], argv[0], argv[1], argv[2], msg);
  case 4: return result (1, fmt, argv[0], argv[0], argv[1], argv[2], argv[3], msg);
  default:
    return result (1, fmt, argv[0], argv[0], argv[1], argv[2], argv[3], msg);
  };
}

// --------------------------------------------------------------------------
//   Changing FPS
// --------------------------------------------------------------------------
int cmd_help (int argc, char **argv)
{
  if (argc != 1 && argc != 2) {
    return wrong_num_args (1, argv, "?topic?");
  }
  if (argc == 1) {
    char *helpmsg =
      "colorspace rgb/yuv" "\n"
      "fps frame-per-second" "\n"
      "kill pid" "\n"
      "mouse x y" "\n"
      "shader /path/to/fragment-shader" "\n"
      "help ?topic?" "\n"
      "quit" "\n";
    result (0, helpmsg);
  }
  else {
    if (!strcmp (argv[1], "help")) {
      char *helpmsg =
	"";
      result (0, helpmsg);
    }
    if (!strcmp (argv[1], "quit")) {
      char *helpmsg =
	"";
      result (0, helpmsg);
    }
    if (!strcmp (argv[1], "colospace")) {
      char *helpmsg =
	"";
      result (0, helpmsg);
    }
    if (!strcmp (argv[1], "fps")) {
      char *helpmsg =
	"";
      result (0, helpmsg);
    }
    if (!strcmp (argv[1], "kill")) {
      char *helpmsg =
	"";
      result (0, helpmsg);
    }
    if (!strcmp (argv[1], "mouse")) {
      char *helpmsg =
	"";
      result (0, helpmsg);
    }
    if (!strcmp (argv[1], "shader")) {
      char *helpmsg =
	"";
      result (0, helpmsg);
    }
  }
  return 0;
}

// --------------------------------------------------------------------------
//   Changing FPS
// --------------------------------------------------------------------------
int cmd_fps (int argc, char **argv)
{
  int fps;
  if (argc != 2) {
    return wrong_num_args (1, argv, "frame-per-second");
  }
  fps = atoi (argv[1]);
  if (fps <= 0 || fps >= 100) {
    return result(1, "expecting integer between 0 and 100, got '%s'", argv[1]);
  }
  g_fps = fps;
  return 0;
}

// --------------------------------------------------------------------------
//   Add / remove a process to signal with SIGUSR1 when
//   a frame is ready to be read.
// --------------------------------------------------------------------------
int cmd_kill (int argc, char **argv)
{
  int pid;
  if (argc != 3) {
    return wrong_num_args (1, argv, "add/rm pid");
  }
  pid = atoi (argv[2]);
  if (pid <= 0) {
    return result(1, "expecting a valid PID number, got '%s'", argv[2]);
  }
  if (strcmp (argv[1], "add") && strcmp (argv[1], "rm")) {
    return result(1, "valid %s subcommands are 'add' or 'rm'", argv[0]);
  }
  //@todo do something
  return 0;
}

// --------------------------------------------------------------------------
//   Close program
// --------------------------------------------------------------------------
int cmd_quit (int argc, char **argv)
{
  if (argc !=1) {
    return wrong_num_args (1, argv, "");
  }
  exit (0);
}

// --------------------------------------------------------------------------
//   Move mouse pointer
// --------------------------------------------------------------------------
int cmd_mouse (int argc, char **argv)
{
  int x, y;
  if (argc != 3) {
    return wrong_num_args (1, argv, "x y");
  }
  x = atoi (argv[1]);
  y = atoi (argv[2]);
  //@todo check
  g_mouse_x = x;
  g_mouse_y = y;
  return 0;
}

// --------------------------------------------------------------------------
//   Change colospace yuv/rgb
// --------------------------------------------------------------------------
int cmd_colorspace (int argc, char **argv)
{
  if ((argc != 1) && (argc != 2)) {
    return wrong_num_args (1, argv, "?yuv/rgb?");
  }
  if (argc == 2) {
    if (!strcmp (argv[1], "yuv") && !strcmp (argv[1], "rgb")) {
      return result(1, "expecting one of 'yuv' or 'rgb', but got '%s'.", argv[1]);
    }
    if (!strcmp (argv[1], "yuv")) {
      g_colorspace = YUV;
    }
    if (!strcmp (argv[1], "rgb")) {
      g_colorspace = RGB;
    }
  }
  else {
    result (0, (g_colorspace == RGB) ? "rgb" : "yuv");
  }
  return 0;
}

// --------------------------------------------------------------------------
//   Change shader
// --------------------------------------------------------------------------
int cmd_shader (int argc, char **argv)
{
  if (argc != 2) {
    return wrong_num_args (1, argv, "/path/to/shader");
  }
  if (!strcmp (argv[1], g_shader)) {
    return 0;
  }
  if (access (argv[1], F_OK) != 0) {
    result (1, "File '%s' not found.", argv[1]);
  }
  if (access (argv[1], R_OK) != 0) {
    result (1, "File '%s' not readable.", argv[1]);
  }
  g_shader = argv[1];
  // compute gl shader program
  return 0;
}

// --------------------------------------------------------------------------
//   Command parser and evaluator
// --------------------------------------------------------------------------
int eval (char* cmd)
{
  char *argv[16], *p;
  int argc = 0;
  for(p = strtok (cmd," \t"); (p != NULL) && (argc < 16); p = strtok(NULL, " \t")) {
    argv[argc++] = p;
  }
  if (argc == 16) {
    fprintf (stderr, "At most 16 words in a command.\n");
    return 1;
  }
  switch (argv[0][0]) {
  case 'c':
    if (!strcmp (argv[0], "colorspace")) {
      return cmd_kill (argc, argv);
    }
    break;
  case 'f':
    if (!strcmp (argv[0], "fps")) {
      return cmd_fps (argc, argv);
    }
    break;
  case 'h':
    if (!strcmp (argv[0], "help")) {
      return cmd_help (argc, argv);
    }
    break;
  case 'k':
    if (!strcmp (argv[0], "kill")) {
      return cmd_kill (argc, argv);
    }
    break;
  case 'm':
    if (!strcmp (argv[0], "mouse")) {
      return cmd_mouse (argc, argv);
    }
    break;
  case 'q':
    if (!strcmp (argv[0], "quit")) {
      return cmd_quit (argc, argv);
    }
    break;
  case 's':
    if (!strcmp (argv[0], "shader")) {
      return cmd_shader (argc, argv);
    }
    break;
  }
  return result(1, "Unknown command '%s'", argv[0]);
}

// --------------------------------------------------------------------------
//   sigint handler
// --------------------------------------------------------------------------
void sigint (int dummy)
{
   g_done = 1;
   signal(SIGINT, sigint);
}

// --------------------------------------------------------------------------
//   Usage
// --------------------------------------------------------------------------
void usage (int argc, char **argv, int optind)
{
  const char *what = (optind > 0) ? "error" : "usage";
  const char *fmt =
    "%s: %s [-w width] [-h height] [-f framerate] [-o /path/to/file] [-s /path/to/fragment-shader]\n"
    "    -?                        Print this help message.\n"
    "    -h height                 Desired image height. Defaults to %d\n"
    "    -w width                  Desired image width. Defaults to %d\n"
    "    -f fps                    Number of images per second. Defaults to %d.\n"
    "    -o /path/to/file          Path of mmap-ed file that will hold image. Defaults to '%s'.\n"
    "    -s /path/to/file          Path of of fragent shader. Defaults to '%s'\n";

  fprintf(stderr, fmt, what, argv[0], DEF_WVID, DEF_HVID, DEF_FPS, DEF_OUTPUT, DEF_SHADER);
  exit(optind > 0);
}

// --------------------------------------------------------------------------
//   Arg parser
// --------------------------------------------------------------------------
int main(int argc, char **argv)
{
  int opt;
  
  while ( (opt = getopt( argc, argv, "?ho:w:f:s:")) != -1 ) {
    switch( opt ) {
    case '?':  usage( argc, argv, 0);
    case 'w':  g_width = atoi(optarg); break;
    case 'h':  g_height = atoi(optarg); break;
    case 'o':  g_out = optarg; break;
    case 'f':  g_fps = atoi(optarg); break;
    case 's':  g_shader = optarg; break;
    default:
      usage(argc, argv, optind);
    }
  }

  // check arguments
  if (g_width <= 0 || g_width >= 4096) {
     fprintf(stderr, "Width (%d) out of range [1-4096].\n", g_width);
     exit(1);
  }
  if (g_height <= 0 || g_height >= 4096) {
     fprintf(stderr, "Height (%d) out of range [1-4096].\n", g_height);
     exit(1);
  }
  if (g_fps <= 0 || g_fps >= 100) {
     fprintf(stderr, "Framerate (%d) out of range [1-100].\n", g_fps);
     exit(1);
  }
  if (access(g_shader,F_OK) != 0) {
    fprintf(stderr, "File '%s' doesn't exist or can't be accessed.`n", g_shader);
    exit(1);
  }
  if (access(g_shader,R_OK) != 0) {
    fprintf(stderr, "File '%s' can't be read.`n", g_shader);
    exit(1);
  }
  
  // install signal handler
  signal(SIGINT, sigint);

  // Creation de l'image utilisée pour le rendu
  g_outfd = initout( g_out, g_width, g_height, g_pim );

  renderloop(g_width, g_height);

  munmap(g_pim->pixels, nblk(g_width,g_height)*BLKSZ);
  close(g_outfd);
  
  return 0;
}
