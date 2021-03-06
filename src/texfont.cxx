
/* Copyright (c) Mark J. Kilgard, 1997. */

/* JET - I've modified it a little, but it works nicely. */
/* This program is freely distributable without licensing fees  and is
   provided without guarantee or warrantee expressed or  implied. This
   program is -not- in the public domain. */

#if defined(_WIN32)
#pragma warning (disable:4244)          /* disable bogus conversion warnings */
#endif

#include "c_defs.h"
#include "conqdef.h"
#include "color.h"
#include "ui.h"
#include "gldisplay.h"
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glu.h>
#include "TexFont.h"

#include "conqutil.h"

// Used to limit the various values read (width/height/etc) from the
// textfont (tainting)
#define MAX_EXTENT_VAL (2048)

#if 0
/* Uncomment to debug various scenarios. */
#undef GL_VERSION_1_1
#undef GL_EXT_texture_object
#undef GL_EXT_texture
#endif /* 0 */

#ifndef GL_VERSION_1_1

#  if defined(GL_EXT_texture_object)
#    define glGenTextures glGenTexturesEXT
#    define glBindTexture glBindTextureEXT
#  else
/* Without OpenGL 1.1 or the texture object extension, use display lists. */
#    define USE_DISPLAY_LISTS
#  endif

#  if defined(GL_EXT_texture)
#    define GL_INTENSITY4 GL_INTENSITY4_EXT
int useLuminanceAlpha = 0;
#  else
/* Intensity texture format not in OpenGL 1.0; added by the EXT_texture
   extension and now part of OpenGL 1.1. */
int useLuminanceAlpha = 1;
#  endif

#else
/* OpenGL 1.1 case. */
int useLuminanceAlpha = 0;
#endif

/* byte swap a 32-bit value */
#define SWAPL(x, n) {                           \
        n = ((char *) (x))[0];                  \
        ((char *) (x))[0] = ((char *) (x))[3];  \
        ((char *) (x))[3] = n;                  \
        n = ((char *) (x))[1];                  \
        ((char *) (x))[1] = ((char *) (x))[2];  \
        ((char *) (x))[2] = n; }

/* byte swap a short */
#define SWAPS(x, n) {                           \
        n = ((char *) (x))[0];                  \
        ((char *) (x))[0] = ((char *) (x))[1];  \
        ((char *) (x))[1] = n; }

static TexGlyphVertexInfo *
getTCVI(TexFont * txf, int c)
{
    TexGlyphVertexInfo *tgvi;

    /* Automatically substitute uppercase letters with lowercase if not
       uppercase available (and vice versa). */
    if ((c >= txf->min_glyph) && (c < txf->min_glyph + txf->range)) {
        tgvi = txf->lut[c - txf->min_glyph];
        if (tgvi) {
            return tgvi;
        }
        if (islower(c)) {
            c = toupper(c);
            if ((c >= txf->min_glyph) && (c < txf->min_glyph + txf->range)) {
                return txf->lut[c - txf->min_glyph];
            }
        }
        if (isupper(c)) {
            c = tolower(c);
            if ((c >= txf->min_glyph) && (c < txf->min_glyph + txf->range)) {
                return txf->lut[c - txf->min_glyph];
            }
        }
    }

#ifdef DEBUG_GL
    utLog("texfont: tried to access unavailable font character \"%c\" (%d)\n",
          isprint(c) ? c : '?', c);
    assert(0);
#endif

    return NULL;
}

static const char *lastError;

const char *
txfErrorString(void)
{
    return lastError;
}

TexFont *
txfLoadFont(char *filename)
{
    TexFont *txf;
    FILE *file;
    GLfloat w, h, xstep, ystep;
    char fileid[4], tmp;
    unsigned char *texbitmap = NULL;
    int min_glyph, max_glyph;
    int endianness, swap, format, stride, width, height;
    int i, j;
    unsigned long got;

    txf = NULL;
    file = fopen(filename, "rb");
    if (file == NULL) {
        lastError = "file open failed.";
        goto error;
    }
    txf = (TexFont *) malloc(sizeof(TexFont));
    if (txf == NULL) {
        lastError = "out of memory.";
        goto error;
    }
    /* For easy cleanup in error case. */
    txf->tgi = NULL;
    txf->tgvi = NULL;
    txf->lut = NULL;
    txf->teximage = NULL;

    /* JET - need to clear texobj... */
    txf->texobj = 0;

    got = fread(fileid, 1, 4, file);
    if (got != 4 || strncmp(fileid, "\377txf", 4)) {
        lastError = "not a texture font file.";
        goto error;
    }
    /*CONSTANTCONDITION*/
    assert(sizeof(int) == 4);  /* Ensure external file format size. */
    got = fread(&endianness, sizeof(int), 1, file);
    if (got == 1 && endianness == 0x12345678) {
        swap = 0;
    } else if (got == 1 && endianness == 0x78563412) {
        swap = 1;
    } else {
        lastError = "not a texture font file.";
        goto error;
    }
#define EXPECT(n) if (got != (unsigned long) n) { lastError = "premature end of file."; goto error; }
    got = fread(&format, sizeof(int), 1, file);
    EXPECT(1);
    format = CLAMP(0, MAX_EXTENT_VAL, format);
    got = fread(&txf->tex_width, sizeof(int), 1, file);
    EXPECT(1);
    txf->tex_width = CLAMP(0, MAX_EXTENT_VAL, txf->tex_width);
    got = fread(&txf->tex_height, sizeof(int), 1, file);
    EXPECT(1);
    txf->tex_height = CLAMP(0, MAX_EXTENT_VAL, txf->tex_height);
    got = fread(&txf->max_ascent, sizeof(int), 1, file);
    EXPECT(1);
    txf->max_ascent = CLAMP(0, MAX_EXTENT_VAL, txf->max_ascent);
    got = fread(&txf->max_descent, sizeof(int), 1, file);
    EXPECT(1);
    txf->max_descent = CLAMP(0, MAX_EXTENT_VAL, txf->max_descent);
    got = fread(&txf->num_glyphs, sizeof(int), 1, file);
    EXPECT(1);
    txf->num_glyphs = CLAMP(0, MAX_EXTENT_VAL, txf->num_glyphs);

    if (swap) {
        SWAPL(&format, tmp);
        SWAPL(&txf->tex_width, tmp);
        SWAPL(&txf->tex_height, tmp);
        SWAPL(&txf->max_ascent, tmp);
        SWAPL(&txf->max_descent, tmp);
        SWAPL(&txf->num_glyphs, tmp);
    }
    txf->tgi = (TexGlyphInfo *) malloc(txf->num_glyphs * sizeof(TexGlyphInfo));
    if (txf->tgi == NULL) {
        lastError = "out of memory.";
        goto error;
    }
    /*CONSTANTCONDITION*/
    assert(sizeof(TexGlyphInfo) == 12);  /* Ensure external file format size. */
    got = fread(txf->tgi, sizeof(TexGlyphInfo), txf->num_glyphs, file);
    EXPECT(txf->num_glyphs);
    txf->num_glyphs = CLAMP(0, MAX_EXTENT_VAL, txf->num_glyphs);

    if (swap) {
        for (i = 0; i < txf->num_glyphs; i++) {
            SWAPS(&txf->tgi[i].c, tmp);
            SWAPS(&txf->tgi[i].x, tmp);
            SWAPS(&txf->tgi[i].y, tmp);
        }
    }
    txf->tgvi = (TexGlyphVertexInfo *)
        malloc(txf->num_glyphs * sizeof(TexGlyphVertexInfo));
    if (txf->tgvi == NULL) {
        lastError = "out of memory.";
        goto error;
    }
    w = txf->tex_width;
    h = txf->tex_height;
#if 0
/* JET - this seems to clip off the glyph along the left and bottom
   edges.  Therefore, we will not do this. :) */
    xstep = 0.5 / w;
    ystep = 0.5 / h;
#else
    xstep = ystep = 0;
#endif

    for (i = 0; i < txf->num_glyphs; i++) {
        TexGlyphInfo *tgi;

        tgi = &txf->tgi[i];
        txf->tgvi[i].t0[0] = tgi->x / w + xstep;
        txf->tgvi[i].t0[1] = tgi->y / h + ystep;
        txf->tgvi[i].v0[0] = tgi->xoffset;
        txf->tgvi[i].v0[1] = tgi->yoffset;
        txf->tgvi[i].t1[0] = (tgi->x + tgi->width) / w + xstep;
        txf->tgvi[i].t1[1] = tgi->y / h + ystep;
        txf->tgvi[i].v1[0] = tgi->xoffset + tgi->width;
        txf->tgvi[i].v1[1] = tgi->yoffset;
        txf->tgvi[i].t2[0] = (tgi->x + tgi->width) / w + xstep;
        txf->tgvi[i].t2[1] = (tgi->y + tgi->height) / h + ystep;
        txf->tgvi[i].v2[0] = tgi->xoffset + tgi->width;
        txf->tgvi[i].v2[1] = tgi->yoffset + tgi->height;
        txf->tgvi[i].t3[0] = tgi->x / w + xstep;
        txf->tgvi[i].t3[1] = (tgi->y + tgi->height) / h + ystep;
        txf->tgvi[i].v3[0] = tgi->xoffset;
        txf->tgvi[i].v3[1] = tgi->yoffset + tgi->height;
        txf->tgvi[i].advance = tgi->advance;
    }

    min_glyph = txf->tgi[0].c;
    max_glyph = txf->tgi[0].c;
    for (i = 1; i < txf->num_glyphs; i++) {
        if (txf->tgi[i].c < min_glyph) {
            min_glyph = txf->tgi[i].c;
        }
        if (txf->tgi[i].c > max_glyph) {
            max_glyph = txf->tgi[i].c;
        }
    }
    txf->min_glyph = min_glyph;
    txf->range = max_glyph - min_glyph + 1;

    txf->lut = (TexGlyphVertexInfo **)
        calloc(txf->range, sizeof(TexGlyphVertexInfo *));
    if (txf->lut == NULL) {
        lastError = "out of memory.";
        goto error;
    }
    for (i = 0; i < txf->num_glyphs; i++) {
        unsigned short index = txf->tgi[i].c - txf->min_glyph;
        index = CLAMP(0, (txf->range * sizeof(TexGlyphVertexInfo *)) - 1,
                      index);
        txf->lut[index] = &txf->tgvi[i];
    }

    switch (format) {
    case TXF_FORMAT_BYTE:
        if (useLuminanceAlpha) {
            unsigned char *orig = NULL;

            orig = (unsigned char *) malloc(txf->tex_width * txf->tex_height);
            if (orig == NULL) {
                lastError = "out of memory.";
                goto error;
            }
            got = fread(orig, 1, txf->tex_width * txf->tex_height, file);
            // can't use EXPECT() here, or we leak orig
            if (got != (unsigned long)(txf->tex_width * txf->tex_height))
            {
                lastError = "invalid read or orig";
                free(orig);
                goto error;
            }
            txf->teximage = (unsigned char *)
                malloc(2 * txf->tex_width * txf->tex_height);
            if (txf->teximage == NULL) {
                lastError = "out of memory.";
                free(orig);
                goto error;
            }
            for (i = 0; i < txf->tex_width * txf->tex_height; i++) {
                txf->teximage[i * 2] = orig[i];
                txf->teximage[i * 2 + 1] = orig[i];
            }
            free(orig);
        } else {
            txf->teximage = (unsigned char *)
                malloc(txf->tex_width * txf->tex_height);
            if (txf->teximage == NULL) {
                lastError = "out of memory.";
                goto error;
            }
            got = fread(txf->teximage, 1, txf->tex_width * txf->tex_height, file);
            EXPECT(txf->tex_width * txf->tex_height);
        }
        break;
    case TXF_FORMAT_BITMAP:
        width = txf->tex_width;
        height = txf->tex_height;
        stride = (width + 7) >> 3;
        texbitmap = (unsigned char *) malloc(stride * height);
        if (texbitmap == NULL) {
            lastError = "out of memory.";
            goto error;
        }
        got = fread(texbitmap, 1, stride * height, file);
        EXPECT(stride * height);

        if (useLuminanceAlpha) {
            txf->teximage = (unsigned char *) calloc(width * height * 2, 1);
            if (txf->teximage == NULL) {
                lastError = "out of memory.";
                goto error;
            }
            for (i = 0; i < height; i++) {
                for (j = 0; j < width; j++) {
                    if (texbitmap[i * stride + (j >> 3)] & (1 << (j & 7))) {
                        txf->teximage[(i * width + j) * 2] = 255;
                        txf->teximage[(i * width + j) * 2 + 1] = 255;
                    }
                }
            }
        } else {
            txf->teximage = (unsigned char *) calloc(width * height, 1);
            if (txf->teximage == NULL) {
                lastError = "out of memory.";
                goto error;
            }
            for (i = 0; i < height; i++) {
                for (j = 0; j < width; j++) {
                    if (texbitmap[i * stride + (j >> 3)] & (1 << (j & 7))) {
                        txf->teximage[i * width + j] = 255;
                    }
                }
            }
        }
        free(texbitmap);
        break;
    }

    fclose(file);
    return txf;

error:

    if (txf) {
        if (txf->tgi)
            free(txf->tgi);
        if (txf->tgvi)
            free(txf->tgvi);
        if (txf->lut)
            free(txf->lut);
        if (txf->teximage)
            free(txf->teximage);
        free(txf);
    }
    if (file)
        fclose(file);

    free(texbitmap);

    return NULL;
}

GLuint
txfEstablishTexture(
    TexFont * txf,
    GLuint texobj,
    GLboolean setupMipmaps)
{
    if (txf->texobj == 0) {
        if (texobj == 0) {
#if !defined(USE_DISPLAY_LISTS)
            glGenTextures(1, &txf->texobj);
#else
            txf->texobj = glGenLists(1);
#endif
        } else {
            txf->texobj = texobj;
        }
    }
#if !defined(USE_DISPLAY_LISTS)
    glBindTexture(GL_TEXTURE_2D, txf->texobj);
    /* use linear MAG filtering */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    /* this is highest quality.  It should be configurable. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
#else
    glNewList(txf->texobj, GL_COMPILE);
#endif

#if 1
    /* XXX Indigo2 IMPACT in IRIX 5.3 and 6.2 does not support the GL_INTENSITY
       internal texture format. Sigh. Win32 non-GLX users should disable this
       code. */
    if (useLuminanceAlpha == 0) {
        char *vendor, *renderer, *version;

        renderer = (char *) glGetString(GL_RENDERER);
        vendor = (char *) glGetString(GL_VENDOR);
        if (!strcmp(vendor, "SGI") && !strncmp(renderer, "IMPACT", 6)) {
            version = (char *) glGetString(GL_VERSION);
            if (!strcmp(version, "1.0 Irix 6.2") ||
                !strcmp(version, "1.0 Irix 5.3")) {
                unsigned char *latex;
                int width = txf->tex_width;
                int height = txf->tex_height;
                int i;

                useLuminanceAlpha = 1;
                latex = (unsigned char *) calloc(width * height * 2, 1);
                /* XXX unprotected alloc. */
                for (i = 0; i < height * width; i++) {
                    latex[i * 2] = txf->teximage[i];
                    latex[i * 2 + 1] = txf->teximage[i];
                }
                free(txf->teximage);
                txf->teximage = latex;
            }
        }
    }
#endif

    if (useLuminanceAlpha) {
        if (setupMipmaps) {
            gluBuild2DMipmaps(GL_TEXTURE_2D, GL_LUMINANCE_ALPHA,
                              txf->tex_width, txf->tex_height,
                              GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, txf->teximage);
        } else {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA,
                         txf->tex_width, txf->tex_height, 0,
                         GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, txf->teximage);
        }
    } else {
#if defined(GL_VERSION_1_1) || defined(GL_EXT_texture)
        /* Use GL_INTENSITY4 as internal texture format since we want to use as
           little texture memory as possible. */
        if (setupMipmaps) {
            gluBuild2DMipmaps(GL_TEXTURE_2D, GL_INTENSITY4,
                              txf->tex_width, txf->tex_height,
                              GL_LUMINANCE, GL_UNSIGNED_BYTE, txf->teximage);
        } else {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_INTENSITY4,
                         txf->tex_width, txf->tex_height, 0,
                         GL_LUMINANCE, GL_UNSIGNED_BYTE, txf->teximage);
        }
#else
        abort();            /* Should not get here without EXT_texture or OpenGL
                               1.1. */
#endif
    }

#if defined(USE_DISPLAY_LISTS)
    glEndList();
    glCallList(txf->texobj);
#endif
    return txf->texobj;
}

void
txfBindFontTexture(
    TexFont * txf)
{
#if !defined(USE_DISPLAY_LISTS)
    glBindTexture(GL_TEXTURE_2D, txf->texobj);
#else
    glCallList(txf->texobj);
#endif
}

void
txfUnloadFont(
    TexFont * txf)
{
    if (txf->teximage) {
        free(txf->teximage);
    }
    free(txf->tgi);
    free(txf->tgvi);
    free(txf->lut);
    free(txf);
}

void
txfGetStringMetrics(
    TexFont * txf,
    const char *string,
    int len,
    int *width,
    int *max_ascent,
    int *max_descent)
{
    TexGlyphVertexInfo *tgvi;
    int w, i;

    w = 0;
    for (i = 0; i < len; i++)
    {
        if (string[i] == '#')
        {                       /* a color sequence */
            i++;
            while (isdigit(string[i]) && i < len)
                i++;

            if (string[i] == '#')
                continue;
        }

        tgvi = getTCVI(txf, string[i]);
        if (tgvi)
            w += tgvi->advance;
    }

    *width = w;
    *max_ascent = txf->max_ascent;
    *max_descent = txf->max_descent;
}

void
txfRenderGlyph(TexFont * txf, int c)
{
    TexGlyphVertexInfo *tgvi;

    tgvi = getTCVI(txf, c);
    if (!tgvi)
        return;

    glBegin(GL_QUADS);
    glTexCoord2fv(tgvi->t0);
    glVertex2sv(tgvi->v0);
    glTexCoord2fv(tgvi->t1);
    glVertex2sv(tgvi->v1);
    glTexCoord2fv(tgvi->t2);
    glVertex2sv(tgvi->v2);
    glTexCoord2fv(tgvi->t3);
    glVertex2sv(tgvi->v3);
    glEnd();
    glTranslatef(tgvi->advance, 0.0, 0.0);
}

void
txfRenderString(
    TexFont * txf,
    const char *string,
    int len)
{
    int i;

    for (i = 0; i < len; i++) {
        txfRenderGlyph(txf, string[i]);
    }
}

enum {
    MONO, TOP_BOTTOM, LEFT_RIGHT, FOUR
};

/* JET - for this one, all we want to be able to do is handle embedded
   cqColor info, represented as '#<int color>#' */
void
txfRenderFancyString(
    TexFont * txf,
    const char *string,
    int len)
{
    int i;
    char *ptr, buf256[256];
    int color;

    for (i = 0; i < len; i++)
    {
        if (string[i] == '#')
        {                       /* a color sequence */
            i++;
            ptr = buf256;
            while (isdigit(string[i]) && i < len)
            {
                *ptr = string[i];
                i++;
                ptr++;
            }

            if (string[i] == '#')
            {                   /* we have complete sequence, emit a
                                   color */
                *ptr = 0;
                color = atoi(buf256);

                uiPutColor(color);
                continue;
            }
        }
        else
            txfRenderGlyph(txf, string[i]);
    }

    return;
}


int
txfInFont(TexFont * txf, int c)
{
    /* NOTE: No uppercase/lowercase substitution. */
    if ((c >= txf->min_glyph) && (c < txf->min_glyph + txf->range)) {
        if (txf->lut[c - txf->min_glyph]) {
            return 1;
        }
    }
    return 0;
}
