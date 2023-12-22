// Modified dec 2023 vzvca
// Original source at https://github.com/intel/libva-utils.git

/*
 * Copyright (c) 2008-2009 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

static int scale_2dimage(unsigned char *src_img, int src_imgw, int src_imgh,
                         unsigned char *dst_img, int dst_imgw, int dst_imgh)
{
    int row = 0, col = 0;

    for (row = 0; row < dst_imgh; row++) {
        for (col = 0; col < dst_imgw; col++) {
            *(dst_img + row * dst_imgw + col) = *(src_img + (row * src_imgh / dst_imgh) * src_imgw + col * src_imgw / dst_imgw);
        }
    }

    return 0;
}


#ifdef LIBVA_UTILS_UPLOAD_DOWNLOAD_YUV_SURFACE

/*
 * Upload YUV data from memory into a surface
 * if src_fourcc == NV12, assume the buffer pointed by src_U
 * is UV interleaved (src_V is ignored)
 */
static int upload_surface_yuv(VADisplay va_dpy, VASurfaceID surface_id,
                              int src_fourcc, int src_width, int src_height,
                              unsigned char *src_Y, unsigned char *src_U, unsigned char *src_V)
{
    VAImage surface_image;
    unsigned char *surface_p = NULL, *Y_start = NULL, *U_start = NULL;
    int Y_pitch = 0, U_pitch = 0, row;
    VAStatus va_status;

    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    vaMapBuffer(va_dpy, surface_image.buf, (void **)&surface_p);
    assert(VA_STATUS_SUCCESS == va_status);

    Y_start = surface_p;
    Y_pitch = surface_image.pitches[0];
    switch (surface_image.format.fourcc) {
    case VA_FOURCC_NV12:
        U_start = (unsigned char *)surface_p + surface_image.offsets[1];
        U_pitch = surface_image.pitches[1];
        break;
    case VA_FOURCC_IYUV:
        U_start = (unsigned char *)surface_p + surface_image.offsets[1];
        U_pitch = surface_image.pitches[1];
        break;
    case VA_FOURCC_YV12:
        U_start = (unsigned char *)surface_p + surface_image.offsets[2];
        U_pitch = surface_image.pitches[2];
        break;
    case VA_FOURCC_YUY2:
        U_start = surface_p + 1;
        U_pitch = surface_image.pitches[0];
        break;
    default:
        assert(0);
    }

    /* copy Y plane */
    for (row = 0; row < src_height; row++) {
        unsigned char *Y_row = Y_start + row * Y_pitch;
        memcpy(Y_row, src_Y + row * src_width, src_width);
    }

    for (row = 0; row < src_height / 2; row++) {
        unsigned char *U_row = U_start + row * U_pitch;
        unsigned char *u_ptr = NULL, *v_ptr = NULL;
        int j;
        switch (surface_image.format.fourcc) {
        case VA_FOURCC_NV12:
            if (src_fourcc == VA_FOURCC_NV12) {
                memcpy(U_row, src_U + row * src_width, src_width);
                break;
            } else if (src_fourcc == VA_FOURCC_IYUV) {
                u_ptr = src_U + row * (src_width / 2);
                v_ptr = src_V + row * (src_width / 2);
            } else if (src_fourcc == VA_FOURCC_YV12) {
                v_ptr = src_U + row * (src_width / 2);
                u_ptr = src_V + row * (src_width / 2);
            }
            if ((src_fourcc == VA_FOURCC_IYUV) ||
                (src_fourcc == VA_FOURCC_YV12)) {
                for (j = 0; j < src_width / 2; j++) {
                    U_row[2 * j] = u_ptr[j];
                    U_row[2 * j + 1] = v_ptr[j];
                }
            }
            break;
        case VA_FOURCC_IYUV:
        case VA_FOURCC_YV12:
        case VA_FOURCC_YUY2:
        default:
            printf("unsupported fourcc in load_surface_yuv\n");
            assert(0);
        }
    }

    vaUnmapBuffer(va_dpy, surface_image.buf);

    vaDestroyImage(va_dpy, surface_image.image_id);

    return 0;
}

/*
 * Download YUV data from a surface into memory
 * Some hardward doesn't have a aperture for linear access of
 * tiled surface, thus use vaGetImage to expect the implemnetion
 * to do tile to linear convert
 *
 * if dst_fourcc == NV12, assume the buffer pointed by dst_U
 * is UV interleaved (src_V is ignored)
 */
static int download_surface_yuv(VADisplay va_dpy, VASurfaceID surface_id,
                                int dst_fourcc, int dst_width, int dst_height,
                                unsigned char *dst_Y, unsigned char *dst_U, unsigned char *dst_V)
{
    VAImage surface_image;
    unsigned char *surface_p = NULL, *Y_start = NULL, *U_start = NULL;
    int Y_pitch = 0, U_pitch = 0, row;
    VAStatus va_status;

    va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");

    vaMapBuffer(va_dpy, surface_image.buf, (void **)&surface_p);
    assert(VA_STATUS_SUCCESS == va_status);

    Y_start = surface_p;
    Y_pitch = surface_image.pitches[0];
    switch (surface_image.format.fourcc) {
    case VA_FOURCC_NV12:
        U_start = (unsigned char *)surface_p + surface_image.offsets[1];
        U_pitch = surface_image.pitches[1];
        break;
    case VA_FOURCC_IYUV:
        U_start = (unsigned char *)surface_p + surface_image.offsets[1];
        U_pitch = surface_image.pitches[1];
        break;
    case VA_FOURCC_YV12:
        U_start = (unsigned char *)surface_p + surface_image.offsets[2];
        U_pitch = surface_image.pitches[2];
        break;
    case VA_FOURCC_YUY2:
        U_start = surface_p + 1;
        U_pitch = surface_image.pitches[0];
        break;
    default:
        assert(0);
    }

    /* copy Y plane */
    for (row = 0; row < dst_height; row++) {
        unsigned char *Y_row = Y_start + row * Y_pitch;
        memcpy(dst_Y + row * dst_width, Y_row, dst_width);
    }

    for (row = 0; row < dst_height / 2; row++) {
        unsigned char *U_row = U_start + row * U_pitch;
        unsigned char *u_ptr = NULL, *v_ptr = NULL;
        int j;
        switch (surface_image.format.fourcc) {
        case VA_FOURCC_NV12:
            if (dst_fourcc == VA_FOURCC_NV12) {
                memcpy(dst_U + row * dst_width, U_row, dst_width);
                break;
            } else if (dst_fourcc == VA_FOURCC_IYUV) {
                u_ptr = dst_U + row * (dst_width / 2);
                v_ptr = dst_V + row * (dst_width / 2);
            } else if (dst_fourcc == VA_FOURCC_YV12) {
                v_ptr = dst_U + row * (dst_width / 2);
                u_ptr = dst_V + row * (dst_width / 2);
            }
            if ((dst_fourcc == VA_FOURCC_IYUV) ||
                (dst_fourcc == VA_FOURCC_YV12)) {
                for (j = 0; j < dst_width / 2; j++) {
                    u_ptr[j] = U_row[2 * j];
                    v_ptr[j] = U_row[2 * j + 1];
                }
            }
            break;
        case VA_FOURCC_IYUV:
        case VA_FOURCC_YV12:
        case VA_FOURCC_YUY2:
        default:
            printf("unsupported fourcc in load_surface_yuv\n");
            assert(0);
        }
    }

    vaUnmapBuffer(va_dpy, surface_image.buf);

    vaDestroyImage(va_dpy, surface_image.image_id);

    return 0;
}

#endif /* LIBVA_UTILS_UPLOAD_DOWNLOAD_YUV_SURFACE */
