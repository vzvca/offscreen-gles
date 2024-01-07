/*
 * MIT License
 *
 * Copyright (c) 2023 vzvca
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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <png.h>

#define DEF_WIDTH 720
#define DEF_HEIGHT 576
#define DEF_INPUT "/tmp/frame"
#define DEF_OUTPUT "/tmp/capture.png"

int   g_signal = 0;
int   g_width  = DEF_WIDTH;
int   g_height = DEF_HEIGHT;
char *g_input  = DEF_INPUT;
char *g_output = DEF_OUTPUT;

/*
 * --------------------------------------------------------------------------
 *   Usage
 * --------------------------------------------------------------------------
 */
void usage( int argc, char *argv[], int optind )
{
   char *what = (optind > 0) ? "error" : "usage";
   fprintf( stderr, "%s: %s [-?] -i file -o file [-w width] [-h height]\n",
            what, argv[0]);
  
   fprintf( stderr, "\t-?\t\tPrints this message.\n");
   fprintf( stderr, "\t-i\t\tSets input video frame file (default %s).\n", DEF_INPUT);
   fprintf( stderr, "\t-o\t\tSet output PNG file name (default %s).\n", DEF_OUTPUT);
   fprintf( stderr, "\t-w\t\tSets the width of the image (default %d).\n", DEF_WIDTH);
   fprintf( stderr, "\t-h\t\tSets the height of the image (default %d).\n", DEF_HEIGHT);
   fprintf (stderr, "\t-s\t\tEncode input on SIGUSR1 signal. Wait at most 10 sec for signal.\n");
  
   /* exit with error only if option parsng failed */
   exit(optind > 0);
}

// --------------------------------------------------------------------------
//   Save PNG image
// --------------------------------------------------------------------------
static int save_png(const char *filename, int width, int height,
                    int bitdepth, int colortype,
                    unsigned char* data, int pitch, int transform)
{
   int i = 0;
   int r = 0;
   FILE* fp = NULL;
   png_structp png_ptr = NULL;
   png_infop info_ptr = NULL;
   png_bytep* row_pointers = NULL;
 
   if (NULL == data) {
      printf("Error: failed to save the png because the given data is NULL.\n");
      r = -1;
      goto error;
   }
 
   if (0 == strlen(filename)) {
      printf("Error: failed to save the png because the given filename length is 0.\n");
      r = -2;
      goto error;
   }
 
   if (0 == pitch) {
      printf("Error: failed to save the png because the given pitch is 0.\n");
      r = -3;
      goto error;
   }
 
   fp = fopen(filename, "wb");
   if (NULL == fp) {
      printf("Error: failed to open the png file: %s\n", filename);
      r = -4;
      goto error;
   }
 
   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
   if (NULL == png_ptr) {
      printf("Error: failed to create the png write struct.\n");
      r = -5;
      goto error;
   }
 
   info_ptr = png_create_info_struct(png_ptr);
   if (NULL == info_ptr) {
      printf("Error: failed to create the png info struct.\n");
      r = -6;
      goto error;
   }
 
   png_set_IHDR(png_ptr,
                info_ptr,
                width,
                height,
                bitdepth,                 /* e.g. 8 */
                colortype,                /* PNG_COLOR_TYPE_{GRAY, PALETTE, RGB, RGB_ALPHA, GRAY_ALPHA, RGBA, GA} */
                PNG_INTERLACE_NONE,       /* PNG_INTERLACE_{NONE, ADAM7 } */
                PNG_COMPRESSION_TYPE_BASE,
                PNG_FILTER_TYPE_BASE);
 
   row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);

   // note: y flip
   for (i = 0; i < height; ++i) {
      row_pointers[i] = data + (height-i) * pitch;
   }
 
   png_init_io(png_ptr, fp);
   png_set_rows(png_ptr, info_ptr, row_pointers);
   png_write_png(png_ptr, info_ptr, transform, NULL);
 
 error:
 
   if (NULL != fp) {
      fclose(fp);
      fp = NULL;
   }
 
   if (NULL != png_ptr) {
 
      if (NULL == info_ptr) {
         printf("Error: info ptr is null. not supposed to happen here.\n");
      }
 
      png_destroy_write_struct(&png_ptr, &info_ptr);
      png_ptr = NULL;
      info_ptr = NULL;
   }
 
   if (NULL != row_pointers) {
      free(row_pointers);
      row_pointers = NULL;
   }
 
   printf("And we're all free.\n");
 
   return r;
}

/* --------------------------------------------------------------------------
 *   Signal handler
 * --------------------------------------------------------------------------*/
void sigusr1 (int dummy)
{
  /* nothing */
}

/* --------------------------------------------------------------------------
 *   Main program
 * --------------------------------------------------------------------------*/
int main (int argc, char *argv[])
{
   int opt, fbfd;
   unsigned char *pixels;
   
   while ( (opt = getopt( argc, argv, "?si:o:w:h:")) != -1 ) {
      switch( opt ) {
      case '?':  usage( argc, argv, 0); break;
      case 'i':  g_input = optarg; break;
      case 'o':  g_output = optarg; break;
      case 'w':  g_width = atoi(optarg); break;
      case 'h':  g_height = atoi(optarg); break;
      case 's':  g_signal = 1; break;
      default:
         usage(argc, argv, optind);
      }
   }

   puts("mmap video source");
   fbfd = open( g_input, O_RDWR );
   if (fbfd == -1) {
      perror("Error: cannot open output file");
      exit(1);
   }
   puts("The output file was opened successfully.");
   pixels = (unsigned char *)mmap(0, g_width*g_height*4, PROT_READ, MAP_SHARED, fbfd, 0);
   if (pixels == MAP_FAILED) {
      perror("Error: failed to map output file to memory");
      exit(1);
   }
   puts("The output file was mapped to memory successfully.\n");

   if (g_signal) {
     puts ("Registering signal handler and wait (at most 10 sec) for SIGUSR1.");
     signal (SIGUSR1, sigusr1);
     if (usleep (10*1000000) == -1) {
       if (errno != EINTR) {
	 perror ("usleep()");
	 exit (1);
       }
       puts("Signal caught!");
     }
     else {
       puts ("Timer expired !");
     }
   }
   
   puts("Saving PNG file");
   save_png(g_output, g_width, g_height, 8, PNG_COLOR_TYPE_RGBA, pixels, g_width*4, PNG_TRANSFORM_IDENTITY);

   puts("Clean");
   munmap(pixels, g_width*g_height*4);
   close(fbfd);
  
   return 0;
}
