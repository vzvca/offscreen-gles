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
 * Graphic headers - implementation in header
 */
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>
#define GLT_IMPLEMENTATION
#define GLT_MANUAL_VIEWPORT
#define GLT_DEBUG_PRINT
#include "gltext.h"

/*
 * Command interpreter header - implementation in header
 */
#define PICOL_IMPLEMENTATION
#include "picol.h"

typedef struct picolInterp picol_t;


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

#define PIXSZ 4

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

typedef struct state_s state_t;
struct state_s
{
  picolInterp *itp;               // command interpreter
  
  struct gbm_device *gbm;         // associated with "/dev/dri/renderD128"         
  int  gbmfd;
  EGLDisplay display;
  EGLContext context;

  char *out;                      // name of output file
  int  outfd;                     // current output file mmaped
  image_t img;                    // current image

  // OpenGL / GLES objects    
  GLuint fb;                      // framebuffer
  GLuint tex;                     // texture
  GLuint vbo;                     // vertex buffer object
  GLuint vao;                     // vertex array object
  GLint  u_time;                  // uniform
  GLint  u_mouse;                 // uniform
  GLint  u_resolution;            // uniform
  GLint  u_colorspace;            // uniform
  GLTtext *msg;                   // message to display
  GLuint prog;                    // current GLSLprogram

  int fps;                        // video framerate
  char *shader;                   // path to current fragment shader
  int colorspace;                 // RGB or YUV colorspace
  int mouse_x, mouse_y;           // current mouse position
  pid_t  pids[4];                 // array of pids to kill when fram ready

  // Statistics
  int nfr;                        // number of frames
  int msecfr[16];                 // number of msec to compute a frame (last 16 frames window)
  
};

state_t g_state;

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
int eval (state_t *st, char* cmd);
picolResult cmd_help (picolInterp *itp, int argc, const char *argv[], void *pd);
picolResult cmd_quit (picolInterp *itp, int argc, const char *argv[], void *pd);
picolResult cmd_kill (picolInterp *itp, int argc, const char *argv[], void *pd);
picolResult cmd_colorspace (picolInterp *itp, int argc, const char *argv[], void *pd);
picolResult cmd_fps (picolInterp *itp, int argc, const char *argv[], void *pd);
picolResult cmd_mouse (picolInterp *itp, int argc, const char *argv[], void *pd);
picolResult cmd_shader (picolInterp *itp, int argc, const char *argv[], void *pd);
picolResult cmd_message (picolInterp *itp, int argc, const char *argv[], void *pd);
picolResult cmd_stats (picolInterp *itp, int argc, const char *argv[], void *pd);
void do_kill (state_t *st);


// --------------------------------------------------------------------------
//   Helper func
// --------------------------------------------------------------------------
static int nblk (int w, int h)
{
   int sz = (w*h*PIXSZ + BLKSZ-1)/BLKSZ;
   return sz;
}

// --------------------------------------------------------------------------
//   Prepare output file and mmap it
// --------------------------------------------------------------------------
static int initout (state_t *st)
{
  unsigned char block[BLKSZ];
  int i, ni, fbfd = -1;
  
  // Open the file for reading and writing
  fbfd = open (st->out, O_RDWR | O_CREAT, S_IRWXU);
  if (fbfd == -1) {
    perror ("Error: cannot open output file");
    exit (1);
  }
  printf("The output file was opened successfully.\n");

  // fill image with 0
  bzero (block, sizeof(block));
  ni = nblk (st->img.w, st->img.h);
  for( i = 0; i < ni; ++i ) {
    if ( -1 == write (fbfd, block, sizeof(block)) ) {
      perror ("Error: writing output file.");
      exit (1);
    }
  }

  // Map the device to memory
  st->img.pixels = (uint32_t *) mmap (0, ni * sizeof(block), PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
  if (*st->img.pixels == -1) {
    perror("Error: failed to map output file to memory");
    exit(1);
  }
  printf("The output file was mapped to memory successfully.\n");
  
  st->img.stride = st->img.w;
  st->outfd = fbfd;
  
  return fbfd;
}

// --------------------------------------------------------------------------
//   Assertion code
// --------------------------------------------------------------------------
void assertOpenGLError (const char* msg) {
   GLenum error = glGetError ();

   if (error != GL_NO_ERROR) {
      fprintf (stderr, "OpenGL error 0x%x %s\n", error, msg);
      exit (1);
   }
}

// --------------------------------------------------------------------------
//   Assertion code
// --------------------------------------------------------------------------
void assertEGLError (const char* msg) {
   EGLint error = eglGetError ();

   if (error != EGL_SUCCESS) {
      fprintf (stderr, "EGL error 0x%x %s\n", error, msg);
      exit (1);
   }
}

// --------------------------------------------------------------------------
//   Create shader from string content
// --------------------------------------------------------------------------
GLuint mkshader (GLuint type, const char *src, GLint len)
{
  GLint cos, loglen;

  GLuint vsh = glCreateShader (type);
   assertOpenGLError ("glCreateShader");

   if (len == -1) len = strlen (src);
   glShaderSource (vsh, 1, (const GLchar **) &src, &len);
   assertOpenGLError ("glShaderSource");

   glCompileShader (vsh);
   
   glGetShaderiv(vsh, GL_COMPILE_STATUS, &cos);
   if (cos != GL_TRUE) {
     glGetShaderiv(vsh, GL_INFO_LOG_LENGTH, &loglen);
     if (loglen > 1) {
       loglen *= sizeof(GLchar);
       GLchar *log = (GLchar*)malloc(loglen);
			
       glGetShaderInfoLog(vsh, loglen, NULL, log);
       printf("Shader #%u <Info Log>:\n%s\n", vsh, log);
       free(log);
     }
     glDeleteShader(vsh);
     exit (1);
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


// --------------------------------------------------------------------------
//   Create program from shaders
// --------------------------------------------------------------------------
GLuint mkprog (state_t *st, GLuint vsh, GLuint fsh)
{
  GLint lks, len;

  if (st->prog) {
     glDeleteProgram (st->prog);
  }
  
  st->prog = glCreateProgram ();
  assertOpenGLError ("glCreateProgram");
  glAttachShader (st->prog, vsh);
  assertOpenGLError ("glAttachShader VS");
  glAttachShader (st->prog, fsh);
  assertOpenGLError ("glAttachShader FS");
  glLinkProgram (st->prog);
  glGetProgramiv(st->prog, GL_LINK_STATUS, &lks);

  if (lks != GL_TRUE) {
    glGetProgramiv(st->prog, GL_INFO_LOG_LENGTH, &len);
    if (len > 1) {
      int glen = len * sizeof(GLchar);
      GLchar *log = (GLchar*)malloc (glen);

      glGetProgramInfoLog (st->prog, glen, NULL, log);
      printf("Program #%u <Info Log>:\n%s\n", st->prog, log);
      free(log);
    }
    exit (1);
  }

  glUseProgram (st->prog);
  assertOpenGLError ("glUseProgram");

   /*
    * Release shaders
    */
  glDetachShader (st->prog, vsh); glDeleteShader(vsh);
  glDetachShader (st->prog, fsh); glDeleteShader(fsh);
   
   /*
    * Bind uniforms
    */
   st->u_time = glGetUniformLocation (st->prog, "time");
   st->u_mouse = glGetUniformLocation (st->prog, "mouse");
   st->u_resolution = glGetUniformLocation (st->prog, "resolution");
   st->u_colorspace = glGetUniformLocation (st->prog, "colorspace");
   
   return st->prog;
}


// --------------------------------------------------------------------------
//   Graphics initialisation
// --------------------------------------------------------------------------
int glinit(state_t *st)
{
   /*
    * EGL initialization and OpenGL context creation.
    */
   EGLConfig config;
   EGLint num_config;

   st->gbmfd = open ("/dev/dri/renderD128", O_RDWR);
   assert (st->gbmfd > 0);

   st->gbm = gbm_create_device (st->gbmfd);
   assert (st->gbm != NULL);

   /* setup EGL from the GBM device */
   st->display = eglGetPlatformDisplay (EGL_PLATFORM_GBM_MESA, st->gbm, NULL);
   assert (st->display != NULL);
   assertEGLError ("eglGetDisplay");

   eglInitialize (st->display, NULL, NULL);
   assertEGLError ("eglInitialize");

   eglChooseConfig (st->display, NULL, &config, 1, &num_config);
   assertEGLError ("eglChooseConfig");

   eglBindAPI (EGL_OPENGL_ES_API);
   assertEGLError ("eglBindAPI");

   static const EGLint attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 3,
      EGL_NONE
   };
        
   st->context = eglCreateContext (st->display, config, EGL_NO_CONTEXT, attribs);
   assertEGLError ("eglCreateContext");

   eglMakeCurrent (st->display, NULL, NULL, st->context);
   assertEGLError ("eglMakeCurrent");

   /*
    * Create an OpenGL framebuffer as render target.
    */
   glGenFramebuffers (1, &st->fb);
   glBindFramebuffer (GL_FRAMEBUFFER, st->fb);
   assertOpenGLError ("glBindFramebuffer");

   /*
    * Create a texture as color attachment.
    */
   glGenTextures(1, &st->tex);

   glBindTexture (GL_TEXTURE_2D, st->tex);
   glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, st->img.w, st->img.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   assertOpenGLError ("glTexImage2D");

   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   /*
    * Attach the texture to the framebuffer.
    */
   glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, st->tex, 0);
   assertOpenGLError ("glFramebufferTexture2D");

   glBindTexture (GL_TEXTURE_2D, 0);
   
   
   /*
    * Create VBO
    */
   glGenBuffers (1, &st->vbo);
   assertOpenGLError ("glGenBuffers");
   glBindBuffer (GL_ARRAY_BUFFER, st->vbo);
   assertOpenGLError ("glBindBuffer");
   glBufferData (GL_ARRAY_BUFFER, 12*sizeof(GLfloat), points, GL_STATIC_DRAW);
   assertOpenGLError ("glBufferData");

   glGenVertexArrays (1, &st->vao);
   assertOpenGLError ("glGenVertexArrays");
   glBindVertexArray (st->vao);
   assertOpenGLError ("glBindVertexArray");
   glEnableVertexAttribArray (0);
   assertOpenGLError ("glEnableVertexArrayAttrib");
   glBindBuffer (GL_ARRAY_BUFFER, st->vbo);
   assertOpenGLError ("glBindBuffer");
   glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
   assertOpenGLError ("glVertexAttribPointer");
   glBindBuffer (GL_ARRAY_BUFFER, 0);
   glBindVertexArray (0);

   /*
    * Initialize text rendering
    */

   if (!gltInit ()) {
     fprintf(stderr,"glt init failed\n");
     exit(1);
   }
   st->msg = gltCreateText();
   gltSetText(st->msg, "Hello \nWorld!");
   
   /*
    * Compile shader
    */

   GLuint vsh = mkshader (GL_VERTEX_SHADER, VERTEX_SHADER_SRC, -1);
   GLuint fsh = mkshaderf (GL_FRAGMENT_SHADER, st->shader);
   mkprog (st, vsh, fsh);

   /*
    * Create command interpreter
    * and register new commands
    */
   st->itp = picolCreateInterp ();
   picolRegisterCmd (st->itp, "help", cmd_help, st);
   picolRegisterCmd (st->itp, "quit", cmd_quit, st);
   picolRegisterCmd (st->itp, "exit", cmd_quit, st);
   picolRegisterCmd (st->itp, "colorspace", cmd_colorspace, st);
   picolRegisterCmd (st->itp, "fps", cmd_fps, st);
   picolRegisterCmd (st->itp, "mouse", cmd_mouse, st);
   picolRegisterCmd (st->itp, "kill", cmd_kill, st);
   picolRegisterCmd (st->itp, "shader", cmd_shader, st);
   picolRegisterCmd (st->itp, "message", cmd_message, st);
   picolRegisterCmd (st->itp, "stats", cmd_stats, st);
   
   return 0;
}

// --------------------------------------------------------------------------
//   End of program
// --------------------------------------------------------------------------
void glfinish (state_t *st)
{
  /*
   * Terminate text remndering engine
   */ 
  gltDeleteText (st->msg);
  gltTerminate();

  /*
   * Delete GL objects
   */
  glDeleteProgram (st->prog);
  glDeleteVertexArrays (1, &st->vao);
  glDeleteBuffers (1, &st->vbo );
  glDeleteTextures (1, &st->tex);
  glDeleteFramebuffers (1, &st->fb);
  
  /*
   * Destroy context.
   */
  glDeleteFramebuffers (1, &st->fb);
  glDeleteTextures (1, &st->tex);
  
  eglDestroyContext (st->display, st->context);
  assertEGLError ("eglDestroyContext");
  
  eglTerminate (st->display);
  assertEGLError ("eglTerminate");

  /*
   * Close device
   */
  gbm_device_destroy (st->gbm);
  close (st->gbmfd);

  /*
   * Unmap memory mapped file
   */
  munmap (st->img.pixels, BLKSZ * nblk (st->img.stride, st->img.h));
  close (st->outfd);

  /*
   * Free memory
   */
  free (st->shader);
  picolFreeInterp (st->itp);
}

// --------------------------------------------------------------------------
//   Display program banner
// --------------------------------------------------------------------------
void banner()
{
  if (isatty (fileno (stdin)) && isatty (fileno (stdout))) {
    puts ("offscreen renderer cli. Type 'help' to see available commands.");
  }
}

// --------------------------------------------------------------------------
//   Display prompt if on a terminal
// --------------------------------------------------------------------------
void prompt()
{
  if (isatty (fileno (stdin)) && isatty (fileno (stdout))) {
    printf ("=> ");
    fflush (stdout);
  }
}

// --------------------------------------------------------------------------
//   Wait a few milliseconds keeping an eye on stdin
// --------------------------------------------------------------------------
int waitmsec (state_t *st, struct timeval *start, int msec)
{
  struct timeval now;
  struct pollfd fds[1];
  int diff, ret;

  fds[0].fd = fileno(stdin);
  fds[0].events = POLLIN;

 again:
  // compute how log time to wait
  gettimeofday (&now, NULL);
  diff = ((now.tv_sec - start->tv_sec)*1000000 + (now.tv_usec - start->tv_usec))/1000;
  msec -= diff;
  if (msec <= 0) return 0;
  
  ret = poll (fds, 1, msec);
  if (ret > 0) {
    if (fds[0].revents & POLLIN) {
      char line[BLKSZ/4], *sline = line;
      int sz;
      
    doread:
      sz = read (fileno(stdin), sline, sizeof(line) - (sline-line));
      if (sz == -1) {
	// read error ! leave
	perror ("read()");
	glfinish (st);
	exit (1);
      }
      if (sz == 0) {
	// stdin closed ! leave
	fprintf (stderr, "stdin closed.\n");
	glfinish (st);
	exit (1);
      }
      else {
	char *p, *s = line;
      parse:
	for (p = s; (p - sline) < sz && *p != '\n'; ++p);
	if (*p == '\n') {
	  *p = 0;
	  eval (st, s);
	  ++p;
	  if (p - sline < sz) {
	    s = p;
	    goto parse;
	  }
	  prompt();
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
int renderloop (state_t *st)
{
  struct timeval start, now;
  int diff;
  
  /*
    * Rendering loop
    */
   glViewport (0, 0, st->img.w, st->img.h);
   assertOpenGLError ("glViewport");

   GLfloat time = 0.0f;
   gettimeofday (&start, NULL);
   while (!g_done) {
     
      glClear (GL_COLOR_BUFFER_BIT);
      assertOpenGLError ("glClear");

      glUseProgram (st->prog);
      assertOpenGLError ("glUseProgram");

      /* modify value of uniform variables */
      if (st->u_resolution != -1) {
	glUniform2f (st->u_resolution, (GLfloat) st->img.w, (GLfloat) st->img.h);
      }
      if (st->u_time != -1) {
	glUniform1f (st->u_time, time);
      }
      if (st->u_mouse != -1) {
	glUniform2f (st->u_mouse, (GLfloat) st->mouse_x, (GLfloat) st->mouse_y);
      }
      if (st->u_colorspace != -1) {
	glUniform1i (st->u_colorspace, st->colorspace);
      }

      // -- draw texture
      glBindVertexArray (st->vao);
      glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
      glBindVertexArray (0);
      glUseProgram (0);
      
      // -- draw text
      gltBeginDraw ();
      gltViewport (st->img.w, st->img.h);
      gltColor (0.0f, 0.0f, 1.0f, 1.0f);
      gltDrawText2D (st->msg, 0.0f, 0.0f, 1.5f); // x=0.0, y=0.0, scale=1.0
      gltEndDraw ();
      glUseProgram (0);

      // -- read image to mmap buffer
      glFlush ();
      glReadPixels (0, 0, st->img.w, st->img.h, GL_RGBA, GL_UNSIGNED_BYTE, st->img.pixels);

      // -- send sigusr1 to tell new frame is ready
      do_kill (st);

      // -- compute time needed to generate frame
      gettimeofday (&now, NULL);
      diff = ((now.tv_sec - start.tv_sec)*1000000 + (now.tv_usec - start.tv_usec))/1000;
      st->msecfr [st->nfr & 0x0f] = diff;
      st->nfr++;

      // -- adjust waiting time according to fps and to time needed to generate image
      diff = 1000/st->fps - diff;
      if (diff <= 0) diff = 1;
      waitmsec (st, &now, diff);

      // -- increment time counter for next image
      gettimeofday (&now, NULL);
      diff = ((now.tv_sec - start.tv_sec)*1000000 + (now.tv_usec - start.tv_usec))/1000;
      start = now;
      time += 0.001 * diff;
   }

   return 0;
}

// --------------------------------------------------------------------------
//   Send kill signal to registered client process
// --------------------------------------------------------------------------
void do_kill (state_t *st)
{
  int i;
  for (i = 0; i < 4; ++i) {
    if (st->pids[i] <= 0) continue;
    if (kill (st->pids[i], 0) == -1) {
      // target process dead maybe ? remove from table.
      st->pids[i] = 0;
    }
    else if (kill (st->pids[i], SIGUSR1) == -1) {
      perror ("kill()");
      exit (1);
    }
  }
}

// --------------------------------------------------------------------------
//   Build result and prints it
// --------------------------------------------------------------------------
int result (picolInterp *itp, int code, char *fmt, ...)
{
  char buf[BLKSZ];
  va_list va;
  va_start(va, fmt);
  vsnprintf (buf, sizeof(buf), fmt, va);
  buf[sizeof(buf)-1] = 0;
  va_end(va);
  picolSetResult (itp, buf);
  return code;
}

// --------------------------------------------------------------------------
//   Build error message regarding arguments number
// --------------------------------------------------------------------------
int wrong_num_args (picolInterp *itp, int argc, const char *argv[], char *msg)
{
  char fmt[] = "%s wrong # args: %s %s %s %s %s %s";
  fmt[16 + 3*argc] = 0;
  switch (argc) {
  case 0: return result (itp, PICOL_ERR, fmt, argv[0], msg);
  case 1: return result (itp, PICOL_ERR, fmt, argv[0], argv[0], msg);
  case 2: return result (itp, PICOL_ERR, fmt, argv[0], argv[0], argv[1], msg);
  case 3: return result (itp, PICOL_ERR, fmt, argv[0], argv[0], argv[1], argv[2], msg);
  case 4: return result (itp, PICOL_ERR, fmt, argv[0], argv[0], argv[1], argv[2], argv[3], msg);
  default:
    return result (itp, PICOL_ERR, fmt, argv[0], argv[0], argv[1], argv[2], argv[3], msg);
  };
}

// --------------------------------------------------------------------------
//   Prints help
// --------------------------------------------------------------------------
picolResult cmd_help (picolInterp *itp, int argc, const char *argv[], void *pd)
{
  if (argc != 1 && argc != 2) {
    return wrong_num_args (itp, 1, argv, "?topic?");
  }
  if (argc == 1) {
    char *helpmsg =
      "colorspace ?rgb/yuv?" "\n"
      "fps ?frame-per-second?" "\n"
      "kill ?add/rm pid?" "\n"
      "message ?msg?" "\n"
      "mouse ?x y?" "\n"
      "shader ?/path/to/fragment-shader?" "\n"
      "stats" "\n"
      "help ?topic?" "\n"
      "quit ?status?" "\n";
    return result (itp, PICOL_OK, helpmsg);
  }
  else {
    if (!strcmp (argv[1], "help")) {
      char *helpmsg =
	"Without 'topic' argument, prints general help. Otherwise prints detailed help on 'topic'.";
      return result (itp, PICOL_OK, helpmsg);
    }
    if (!strcmp (argv[1], "quit")) {
      char *helpmsg =
	"Leave program. Returns 'status' if supplied and 0 otherwise. 'status' must be a valid integer. ";
      return result (itp, PICOL_OK, helpmsg);
    }
    if (!strcmp (argv[1], "colorspace")) {
      char *helpmsg =
	"With no argument, returns current colorspace. Otherwise sets colorspace according to argument. Valid values are 'rgb' or 'yuv'.";
      return result (itp, PICOL_OK, helpmsg);
    }
    if (!strcmp (argv[1], "fps")) {
      char *helpmsg =
	"With no argument, returns current video framerate. Otherwise sets framerate according to argument. Valud values are integer between 1 and 100.";
      return result (itp, PICOL_OK, helpmsg);
    }
    if (!strcmp (argv[1], "kill")) {
      char *helpmsg =
	"With no arguments, returns the list of processes which are signaled when a video frame is ready. Otherwise adds or removes a process from the list of processes to signal with SIGUSR1.";
      return result (itp, PICOL_OK, helpmsg);
    }
    if (!strcmp (argv[1], "mouse")) {
      char *helpmsg =
	"With no arguments, returns current mouse position. Otherwise sets mouse position. Mouse position is sent to shader program which will use it or not.";
      return result (itp, PICOL_OK, helpmsg);
    }
    if (!strcmp (argv[1], "shader")) {
      char *helpmsg =
	"With no argument, returns current shader program. Otherwise changes shader program on the fly.";
      return result (itp, PICOL_OK, helpmsg);
    }
    if (!strcmp (argv[1], "message")) {
      char *helpmsg =
	"With no argument, returns current displayed message. Otherwise changes displayed message.";
      return result (itp, PICOL_OK, helpmsg);
    }
    if (!strcmp (argv[1], "stats")) {
      char *helpmsg =
	"Prints statistics.";
      return result (itp, PICOL_OK, helpmsg);
    }
  }
  return result (itp, PICOL_ERR, "unknown help topic");
}

// --------------------------------------------------------------------------
//   Changing FPS
// --------------------------------------------------------------------------
picolResult cmd_fps (picolInterp *itp, int argc, const char *argv[], void *pd)
{
  state_t *state = pd;
  int fps;
  if (argc != 1 && argc != 2) {
    return wrong_num_args (itp, 1, argv, "frame-per-second");
  }
  if (argc == 2) {
    fps = atoi (argv[1]);
    if (fps <= 0 || fps >= 100) {
      return result(itp, PICOL_ERR, "expecting integer between 0 and 100, got '%s'", argv[1]);
    }
    state->fps = fps;
    return PICOL_OK;
  }
  else {
    return result (itp, PICOL_OK, "%d", state->fps);
  }
}

// --------------------------------------------------------------------------
//   Add / remove a process to signal with SIGUSR1 when
//   a frame is ready to be read.
// --------------------------------------------------------------------------
picolResult cmd_kill (picolInterp *itp, int argc, const char *argv[], void *pd)
{
  state_t *state = pd;
  int pid;
  if (argc != 3) {
    return wrong_num_args (itp, 1, argv, "add/rm pid");
  }
  pid = atoi (argv[2]);
  if (pid <= 0) {
    return result(itp, PICOL_ERR, "expecting a valid PID number, got '%s'", argv[2]);
  }
  if (strcmp (argv[1], "add") && strcmp (argv[1], "rm")) {
    return result(itp, PICOL_ERR, "valid %s subcommands are 'add' or 'rm'", argv[0]);
  }
  if (!strcmp (argv[1], "add")) {
    int i;
    for (i = 0; i < 4; ++i) {
      if (state->pids[i] == 0) break;
    }
    if (i == 4) {
      return result (itp, PICOL_ERR, "pid table full");
    }
    if (kill (pid, 0) == -1) {
      return result (itp, PICOL_ERR, "can't send signal to pid");
    }
    state->pids[i] = pid;
  }
  if (!strcmp (argv[1], "rm")) {
    int i;
    for (i = 0; i < 4; ++i) {
      if (state->pids[i] == pid) {
	state->pids[i] = 0;
	break;
      }
    }
  }
  
  return PICOL_OK;
}

// --------------------------------------------------------------------------
//   Close program
// --------------------------------------------------------------------------
picolResult cmd_quit (picolInterp *itp, int argc, const char *argv[], void *pd)
{
  state_t *state = pd;
  int status = 0;
  if (argc != 1 && argc != 2) {
    return wrong_num_args (itp, 1, argv, "?status?");
  }
  if (argc == 2) {
    status = atoi (argv[1]);
  }
  glfinish (state);
  exit (status);
}

// --------------------------------------------------------------------------
//   Move mouse pointer
// --------------------------------------------------------------------------
picolResult cmd_mouse (picolInterp *itp, int argc, const char *argv[], void *pd)
{
  state_t *state = pd;
  if (argc != 3 && argc != 1) {
    return wrong_num_args (itp, 1, argv, "?x y?");
  }
  if (argc == 3) {
    int x, y;
    x = atoi (argv[1]);
    y = atoi (argv[2]);
    //@todo check
    state->mouse_x = x;
    state->mouse_y = y;
    return PICOL_OK;
  }
  else {
    return result(itp, PICOL_OK, "%d %d", state->mouse_x, state->mouse_y);
  }
}

// --------------------------------------------------------------------------
//   Change colorspace yuv/rgb
// --------------------------------------------------------------------------
picolResult cmd_colorspace (picolInterp *itp, int argc, const char *argv[], void *pd)
{
  state_t *state = pd;

  if ((argc != 1) && (argc != 2)) {
    return wrong_num_args (itp, 1, argv, "?yuv/rgb?");
  }
  if (argc == 2) {
    if (!strcmp (argv[1], "yuv") && !strcmp (argv[1], "rgb")) {
      return result(itp, PICOL_ERR, "expecting one of 'yuv' or 'rgb', but got '%s'.", argv[1]);
    }
    if (!strcmp (argv[1], "yuv")) {
      state->colorspace = YUV;
      gltColorspace (GLT_COL_YUV);
    }
    if (!strcmp (argv[1], "rgb")) {
      state->colorspace = RGB;
      gltColorspace (GLT_COL_RGB);
    }
    return PICOL_OK;
  }
  else {
    return result (itp, PICOL_OK, (state->colorspace == RGB) ? "rgb" : "yuv");
  }
}

// --------------------------------------------------------------------------
//   Change shader
// --------------------------------------------------------------------------
picolResult cmd_shader (picolInterp *itp, int argc, const char *argv[], void *pd)
{
  state_t *state = pd;
    
  if (argc != 1 && argc != 2) {
    return wrong_num_args (itp, 1, argv, "/path/to/shader");
  }
  if (argc == 1) {
    return result (itp, 0, state->shader);
  }
  else {
    if (!strcmp (argv[1], state->shader)) {
      return 0;
    }
    if (access (argv[1], F_OK) != 0) {
      return result (itp, 1, "File '%s' not found.", argv[1]);
    }
    if (access (argv[1], R_OK) != 0) {
      return result (itp, 1, "File '%s' not readable.", argv[1]);
    }
    free (state->shader);
    state->shader = strdup (argv[1]);
    GLuint vsh = mkshader (GL_VERTEX_SHADER, VERTEX_SHADER_SRC, -1);
    GLuint fsh = mkshaderf (GL_FRAGMENT_SHADER, state->shader);
    mkprog (state, vsh, fsh);
  }
  
  return PICOL_OK;
}

// --------------------------------------------------------------------------
//   Change message
// --------------------------------------------------------------------------
picolResult cmd_message (picolInterp *itp, int argc, const char *argv[], void *pd)
{
  state_t *state = pd;

  if (argc != 1 && argc != 2) {
    return wrong_num_args (itp, 1, argv, "?msg?");
  }
  if (argc == 1) {
    return result (itp, PICOL_OK, (char*) gltGetText (state->msg)); 
  }
  else {
    gltSetText (state->msg, argv[1]);
    return PICOL_OK;
  }
}

// --------------------------------------------------------------------------
//   Display statistics
// --------------------------------------------------------------------------
picolResult cmd_stats (picolInterp *itp, int argc, const char *argv[], void *pd)
{
  state_t *state = pd;
  int m, i;
  
  if (argc != 1) {
    return wrong_num_args (itp, 1, argv, "");
  }
  for (i = m = 0; i < 16; ++i) m += state->msecfr[i];
  m /= 16;
  return result (itp, PICOL_OK, "nframes %d msec per frame %d", state->nfr, m);
}

// --------------------------------------------------------------------------
//   Command parser and evaluator
// --------------------------------------------------------------------------
int eval (state_t *st, char* cmd)
{
  picolResult res = picolEval (st->itp, cmd);
  if (st->itp->result[0]) {
    puts (st->itp->result);
  }
  return res;
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

  g_state.img.w = DEF_WVID;
  g_state.img.h = DEF_HVID;
  g_state.fps = DEF_FPS;
  g_state.shader = strdup (DEF_SHADER);
  g_state.out = DEF_OUTPUT;
  
  while ((opt = getopt (argc, argv, "?h:o:w:f:s:")) != -1 ) {
    switch( opt ) {
    case '?':  usage (argc, argv, 0);
    case 'w':  g_state.img.w = atoi (optarg); break;
    case 'h':  g_state.img.h = atoi (optarg); break;
    case 'o':  g_state.out = optarg; break;
    case 'f':  g_state.fps = atoi (optarg); break;
    case 's':  free (g_state.shader); g_state.shader = strdup (optarg); break;
    default:
      usage (argc, argv, optind);
    }
  }

  // check arguments
  if (g_state.img.w <= 0 || g_state.img.w >= 4096) {
     fprintf (stderr, "Width (%d) out of range [1-4096].\n", g_state.img.w);
     exit (1);
  }
  if (g_state.img.h <= 0 || g_state.img.h >= 4096) {
     fprintf (stderr, "Height (%d) out of range [1-4096].\n", g_state.img.h);
     exit (1);
  }
  if (g_state.fps <= 0 || g_state.fps >= 100) {
     fprintf (stderr, "Framerate (%d) out of range [1-100].\n", g_state.fps);
     exit(1);
  }
  if (access (g_state.shader, F_OK) != 0) {
    fprintf (stderr, "File '%s' doesn't exist or can't be accessed.`n", g_state.shader);
    exit (1);
  }
  if (access (g_state.shader, R_OK) != 0) {
    fprintf (stderr, "File '%s' can't be read.`n", g_state.shader);
    exit (1);
  }
  
  // install signal handler
  signal (SIGINT, sigint);

  // Creation de l'image utilisée pour le rendu
  initout (&g_state);
  glinit (&g_state);

  banner ();
  prompt ();
  renderloop (&g_state);
  
  glfinish (&g_state);
  
  return 0;
}
