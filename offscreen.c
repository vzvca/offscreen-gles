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
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <gbm.h>

/*
 * EGL headers.
 */
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
int    g_done = 0;
int    g_width  = DEF_WVID;
int    g_height = DEF_HVID;
int    g_mouse_x = 0;
int    g_mouse_y = 0;
const char*  g_out = DEF_OUTPUT;
int    g_framerate = DEF_FPS;
const char*  g_shader = DEF_SHADER;
image_t g_im, *g_pim = &g_im;

static const GLfloat points[] = {
   // front
   -1.0f, -1.0f, +1.0f,
   +1.0f, -1.0f, +1.0f,
   -1.0f, +1.0f, +1.0f,
   +1.0f, +1.0f, +1.0f
};

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
  fbfd = open(file, O_RDWR | O_CREAT, S_IRWXU);
  if (fbfd == -1) {
    perror("Error: cannot open output file");
    exit(1);
  }
  printf("The output file was opened successfully.\n");

  // fill image with 0
  bzero( block, sizeof(block));
  ni = nblk(width,height);
  for( i = 0; i < ni; ++i ) {
    if ( -1 == write( fbfd, block, sizeof(block)) ) {
      perror("Error: writing output file.");
      exit(1);
    }
  }

  // Map the device to memory
  fbp = (char *)mmap(0, ni * sizeof(block), PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
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
   GLenum error = glGetError();

   if (error != GL_NO_ERROR) {
      fprintf(stderr, "OpenGL error 0x%x %s\n", error, msg);
      exit(1);
   }
}

// --------------------------------------------------------------------------
//   Assertion code
// --------------------------------------------------------------------------
void assertEGLError(const char* msg) {
   EGLint error = eglGetError();

   if (error != EGL_SUCCESS) {
      fprintf(stderr, "EGL error 0x%x %s\n", error, msg);
      exit(1);
   }
}

// --------------------------------------------------------------------------
//   Create shader from string content
// --------------------------------------------------------------------------
GLuint mkshader(GLuint type, const char *src, GLint len)
{
   GLuint vsh = glCreateShader(type);
   assertOpenGLError("glCreateShader");

   if (len == -1) len = strlen(src);
   glShaderSource(vsh, 1, (const GLchar **) &src, &len);
   assertOpenGLError("glShaderSource");

   glCompileShader(vsh);
   assertOpenGLError("glCompileShader");

   char log[BLKSZ];
   int  loglen = sizeof(log);
   log[0] = 0;
   glGetShaderInfoLog(vsh, sizeof(log), &loglen, log);
   if (loglen) {
      puts("Shader compilation log:");
      puts(log);
   }

   return vsh;
}

// --------------------------------------------------------------------------
//   Create shader from file content
// --------------------------------------------------------------------------
GLuint mkshaderf(GLuint type, const char *fname)
{
   GLint len;
   char src[8*BLKSZ];

   FILE *fin = fopen(fname,"r");
   if (fin == NULL) {
      fprintf(stderr, "Can't open file %s\n", fname);
      exit(1);
   }
   len = fread(src, 1, sizeof(src), fin);
   fclose(fin);

   return mkshader(type, (const char*) src, len);
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

   int32_t fd = open ("/dev/dri/renderD128", O_RDWR);
   assert (fd > 0);

   struct gbm_device *gbm = gbm_create_device (fd);
   assert (gbm != NULL);

   /* setup EGL from the GBM device */
   display = eglGetPlatformDisplay (EGL_PLATFORM_GBM_MESA, gbm, NULL);
   //display = eglGetDisplay (EGL_DEFAULT_DISPLAY);
   assert (display != NULL);
   assertEGLError("eglGetDisplay");

   eglInitialize(display, NULL, NULL);
   assertEGLError("eglInitialize");

   eglChooseConfig(display, NULL, &config, 1, &num_config);
   assertEGLError("eglChooseConfig");

   eglBindAPI(EGL_OPENGL_ES_API);
   assertEGLError("eglBindAPI");

   static const EGLint attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 3,
      EGL_NONE
   };
        
   context = eglCreateContext(display, config, EGL_NO_CONTEXT, attribs);
   assertEGLError("eglCreateContext");

   eglMakeCurrent(display, NULL, NULL, context);
   assertEGLError("eglMakeCurrent");

   /*
    * Create an OpenGL framebuffer as render target.
    */
   GLuint frameBuffer;
   glGenFramebuffers(1, &frameBuffer);
   glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
   assertOpenGLError("glBindFramebuffer");

   /*
    * Create a texture as color attachment.
    */
   GLuint t;
   glGenTextures(1, &t);

   glBindTexture(GL_TEXTURE_2D, t);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   assertOpenGLError("glTexImage2D");

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   /*
    * Attach the texture to the framebuffer.
    */
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t, 0);
   assertOpenGLError("glFramebufferTexture2D");

   /*
    * Create VBO
    */
   GLuint vbo = 0;
   glGenBuffers(1, &vbo);
   assertOpenGLError("glGenBuffers");
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   assertOpenGLError("glBindBuffer");
   glBufferData(GL_ARRAY_BUFFER, 12*sizeof(GLfloat), points, GL_STATIC_DRAW);
   assertOpenGLError("glBufferData");

   GLuint vao = 0;
   glGenVertexArrays(1, &vao);
   assertOpenGLError("glGenVertexArrays");
   glBindVertexArray(vao);
   assertOpenGLError("glBindVertexArray");
   glEnableVertexAttribArray(0);
   assertOpenGLError("glEnableVertexArrayAttrib");
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   assertOpenGLError("glBindBuffer");
   glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
   assertOpenGLError("glVertexAttribPointer");

   /*
    * Compile shader
    */

   GLuint vsh = mkshader(GL_VERTEX_SHADER, VERTEX_SHADER_SRC, -1);
   GLuint fsh = mkshaderf(GL_FRAGMENT_SHADER, g_shader);

   GLuint prog = glCreateProgram();
   assertOpenGLError("glCreateProgram");
   glAttachShader(prog, vsh);
   assertOpenGLError("glAttachShader VS");
   glAttachShader(prog, fsh);
   assertOpenGLError("glAttachShader FS");
   glLinkProgram(prog);
   assertOpenGLError("glLinkProgram");
   puts("Program log");
   char msg[2048] = {0};
   int msglen;
   glGetProgramInfoLog(prog, sizeof(msg), &msglen, msg);
   puts(msg);

   glUseProgram(prog);
   assertOpenGLError("glUseProgram");

   /*
    * Bind uniforms
    */
   GLint u_time = glGetUniformLocation(prog, "time");
   GLint u_mouse = glGetUniformLocation(prog, "mouse");
   GLint u_resolution = glGetUniformLocation(prog, "resolution");
   
   /*
    * Rendering loop
    */
   glViewport(0,0,width,height);
   assertOpenGLError("glViewport");

   if (u_resolution != -1) {
      glUniform2f(u_resolution, (GLfloat) g_width, (GLfloat) g_height);
   }

   GLfloat time = 0.0f;
   while (!g_done) {

      /* modify value of uniform variables */
      if (u_time != -1) {
         glUniform1f(u_time, time);
      }
      if (u_mouse != -1) {
         glUniform2f(u_mouse, (GLfloat) g_mouse_x, (GLfloat) g_mouse_y);
      }
      
      glClear(GL_COLOR_BUFFER_BIT);
      assertOpenGLError("glClear");

      glBindVertexArray(vao);
      assertOpenGLError("glBindVertexArray");
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      assertOpenGLError("glDrawArrays");
   
      glFlush();

      glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, g_pim->pixels);

      usleep(1000000/g_framerate);
      time += 1.0/g_framerate;
   }

   /*
    * Destroy context.
    */
   glDeleteFramebuffers(1, &frameBuffer);
   glDeleteTextures(1, &t);

   eglDestroyContext(display, context);
   assertEGLError("eglDestroyContext");

   eglTerminate(display);
   assertEGLError("eglTerminate");

   return 0;
}


// --------------------------------------------------------------------------
//   sigint handler
// --------------------------------------------------------------------------
void sigint(int dummy)
{
   g_done = 1;
   signal(SIGINT, sigint);
}

// --------------------------------------------------------------------------
//   Usage
// --------------------------------------------------------------------------
void usage(int argc, char **argv, int optind)
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
    case 'f':  g_framerate = atoi(optarg); break;
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
  if (g_framerate <= 0 || g_framerate >= 100) {
     fprintf(stderr, "Framerate (%d) out of range [1-100].\n", g_framerate);
     exit(1);
  }
  
  // Creation de l'image utilisée pour le rendu
  int imfd = initout( g_out, g_width, g_height, g_pim );

  // install signal handler
  signal(SIGINT, sigint);

  renderloop(g_width, g_height);

  munmap(g_pim->pixels, nblk(g_width,g_height)*BLKSZ);
  close(imfd);
  
  return 0;
}


