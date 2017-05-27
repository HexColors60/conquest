/* 
 * anim.h - animation
 * 
 * Copyright Jon Trulson under the ARTISTIC LICENSE. (See LICENSE).
 */

#ifndef _ANIM_H
#define _ANIM_H 


#include "conqinit.h"

/* basic draw state used/modified by an animator */
typedef struct _anim_draw_state {
  /* texture id */
  GLint        id;

  /* texture coordinates */
  GLTexcoord_t tc;

  /* color */
  GLColor_t    col;

  /* geometry */
  real         x, y, z, angle, size;

  /* toggle (it's always either on (non-0) or off (0)) */
  int          armed;

  /* a private area for data storage */
  void        *private;

} animDrawStateRec_t, *animDrawStatePtr_t;

/* this structure contains all the state (public and private) needed by
   an animdef to run. */
typedef struct _anim_state {

  /* ##### public */

  /* flags (global for state) */
  uint32_t            flags;

  /* volatile (current) draw state */
  animDrawStateRec_t state;

  /* initial draw state - used to reset state on init */
  animDrawStateRec_t istate;

  uint32_t            expired; /* anim types that have expired (ANIM_TYPE_) */

  uint32_t            anims;     /* bitmask of animation types present
                                   ANIM_TYPE_* */

  /* ##### private - should only be accessed by animator and API */

  int                adIndex; /* index into GLAnimDefs that specifies the
                                 animdef responsible for running this
                                 state. */
  uint32_t            starttime; /* time of last reset */

  struct {
    uint32_t flags;
    uint32_t lasttime;
    uint32_t curstage;
    uint32_t curloop;
  } tex;

  struct {
    uint32_t flags;
    uint32_t lasttime;
    uint32_t curstage;
    uint32_t curloop;
  } col;

  struct {
    uint32_t flags;
    uint32_t lasttime;
    uint32_t curstage;
    uint32_t curloop;
  } geo;

  struct {
    uint32_t flags;
    uint32_t lasttime;
  } tog;
} animStateRec_t, *animStatePtr_t;

/* shortcut */
#define ANIM_EXPIRED(x)    (((x)->expired & CQI_ANIMS_MASK) == CQI_ANIMS_MASK)

/* GL representation of an animdef */

/* represents data generated for a texanim state */
struct _anim_texture_ent {
  GLint     id;                 /* texture id */
  GLColor_t col;                /* color components */
};

/* Alot of this is just copies of the relevant cqiAnimDefs data,
   duplicated here for speed. Other data (like texanim ents) are
   generated when the gl animdef is initialized in initGLAnimdefs(). */
typedef struct _gl_animdef {
  GLint      texid;             /* texid of a texture specified in the
                                   cqi animdef, and there was no
                                   texanim specified.  If there
                                   was a texanim specified, then this
                                   value is ignored.  this is only
                                   used to init the state.id at init
                                   time if non 0. */

  uint32_t    timelimit;         /* max run time in ms */
  uint32_t    anims;             /* animation types present (CQI_ANIMS_*) */

  /* istate */
  uint32_t    istates;           /* AD_ISTATE_* */
  GLint      itexid;
  GLColor_t  icolor;
  real       iangle;
  real       isize;             /* in CU's */

  struct {
    /* these are copies of the relevant cqi data */
    GLColor_t color;            /* starting color, if specified */
    uint32_t   stages;           /* number of stages (textures) */
    uint32_t   loops;            /* number of loops, 0 = inf */
    uint32_t   delayms;          /* delay per-stage in ms */
    uint32_t   looptype;  /* the type of loop (asc/dec/pingpong/etc) */

    GLfloat   deltas;
    GLfloat   deltat;
    /* list of texanim entries, generated at creation time */
    struct _anim_texture_ent *tex;
  } tex;

  struct {
    GLColor_t color;            /* starting color, if specified */
    
    uint32_t   stages;           /* number of stages (delta ops)) 0 = inf*/
    uint32_t   delayms;          /* delay per-stage in ms */
    uint32_t   loops;            /* number of loops, 0 = inf */
    uint32_t   looptype;         /* the type of loop (asc/dec//pingpong/etc) */
    
    real      deltaa;           /* deltas to appliy to ARGB components */
    real      deltar;
    real      deltag;
    real      deltab;
  } col;

  struct {
    uint32_t   stages;           /* number of stages (delta ops)) 0 = inf*/
    uint32_t   delayms;          /* delay per-stage in ms */
    uint32_t   loops;            /* number of loops, 0 = inf */
    uint32_t   looptype;         /* the type of loop (asc/dec//pingpong/etc) */

    real      deltax;           /* x y and z deltas */
    real      deltay;
    real      deltaz;
    real      deltar;           /* rotation (degrees) */
    real      deltas;           /* size in CU's */
  } geo;

  struct {
    uint32_t   delayms;          /* delay per-stage in ms */
  } tog;

} GLAnimDef_t;

/* a struct for the animation Que that nodes will use */
typedef struct _anim_que {
  int             maxentries;   /* max entries que can
                                   currently hold */
  int             numentries;   /* number of states in que */

  animStatePtr_t *que;          /* que of anim state ptrs to run */
} animQue_t;

/* cqiNumAnimDefs is identical to the number of GLAnimDefs, so
   we do not bother to export a seperate (but identical) numGLAnimDefs. */
#ifdef NOEXTERN_GLANIM
GLAnimDef_t *GLAnimDefs = NULL;
#else
extern GLAnimDef_t *GLAnimDefs;
#endif

int  findGLAnimDef(char *animname); /* GL.c */
int  animInitState(char *animname, animStatePtr_t astate, 
                  animDrawStatePtr_t istate);
void animResetState(animStatePtr_t astate, uint32_t lasttime);

int  animIterState(animStatePtr_t astate);

void animQueInit(animQue_t *aque);
void animQueAdd(animQue_t *aque, animStatePtr_t astate);
void animQueDelete(animQue_t *aque, animStatePtr_t astate);
void animQueRun(animQue_t *aque);


#endif /* _ANIM_H */
