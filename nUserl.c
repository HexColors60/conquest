/* 
 *
 * $Id$
 *
 * Copyright 1999-2004 Jon Trulson under the ARTISTIC LICENSE. (See LICENSE).
 */

#include "c_defs.h"
#include "context.h"
#include "global.h"
#include "datatypes.h"
#include "color.h"
#include "conf.h"
#include "conqcom.h"
#include "conqlb.h"
#include "gldisplay.h"
#include "node.h"
#include "client.h"
#include "packet.h"

#include "nCP.h"
#include "nMenu.h"
#include "nDead.h"
#include "nUserl.h"

/* from conquestgl */
extern Unsgn8 clientFlags; 
extern int lastServerError;

extern void processPacket(char *);

static int snum, godlike;
static int uvec[MAXUSERS];
static int nu;
static int fuser;
static int offset;

static int extrast;             /* normal, or extra stats? */

static int nUserlDisplay(dspConfig_t *);
static int nUserlIdle(void);
static int nUserlInput(int ch);

static scrNode_t nUserlNode = {
  nUserlDisplay,               /* display */
  nUserlIdle,                  /* idle */
  nUserlInput                  /* input */
};

static int retnode;             /* the node to return to */

void nUserlInit(int nodeid, int sn, int gl, int extra)
{
  int i, unum;

  retnode = nodeid;
  snum = sn;
  godlike = gl;
  extrast = extra;

  if (dConf.viewerwmapped)
    {
      /* unmap the viewer */
      glutSetWindow(dConf.viewerw);
      glutHideWindow();
      dConf.viewerwmapped = FALSE;

      glutSetWindow(dConf.mainw);
    }

  /* init the user vector */
  
  for (i=0; i<MAXUSERS; i++)
    uvec[i] = i;
  
  /* sort the (living) user list */
  nu = 0;
  for ( unum = 0; unum < MAXUSERS; unum++)
    if ( Users[unum].live)
      {
        uvec[nu++] = unum;
      }
  clbSortUsers(uvec, nu);

  fuser = 0;

  setNode(&nUserlNode);

  return;
}


static int nUserlDisplay(dspConfig_t *dsp)
{
  int j, fline, lline, lin;
  static char *hd1="U S E R   L I S T";
  static char *ehd1="M O R E   U S E R   S T A T S";
  static char *ehd2="name         cpu  conq coup geno  taken bombed/shot  shots  fired   last entry";
  static char *ehd3="planets  armies    phaser  torps";
  static char cbuf[BUFFER_SIZE];
  int color;

  /* Do some screen setup. */
  lin = 0;
  if (extrast)
    {
      cprintf(lin, 0, ALIGN_CENTER, "#%d#%s", LabelColor, ehd1);
      lin = lin + 2;
      cprintf(lin, 34, ALIGN_NONE, "#%d#%s", LabelColor, ehd3);

      c_strcpy( ehd2, cbuf );
      lin = lin + 1;
      cprintf(lin, 0, ALIGN_NONE, "#%d#%s", LabelColor, cbuf);
    }
  else
    {
      cprintf(lin, 0, ALIGN_CENTER, "#%d#%s", LabelColor, hd1);
      lin = lin + 3;        /* FIXME - hardcoded??? - dwp */
      clbUserline( -1, -1, cbuf, FALSE, FALSE );
      cprintf(lin, 0, ALIGN_NONE, "#%d#%s", LabelColor, cbuf);
    }
  
  for ( j = 0; cbuf[j] != EOS; j = j + 1 )
    if ( cbuf[j] != ' ' )
      cbuf[j] = '-';

  lin++;
  cprintf(lin, 0, ALIGN_NONE, "#%d#%s", LabelColor, cbuf);
  
  fline = lin + 1;				/* first line to use */
  lline = MSG_LIN1;				/* last line to use */
  
  offset = fuser;
  lin = fline;
  while ( offset < nu && lin <= lline )
    {
      if (extrast)
        clbStatline( uvec[offset], cbuf );
      else
        clbUserline( uvec[offset], -1, cbuf, godlike, FALSE );
      
      /* determine color */
      if ( snum > 0 && snum <= MAXSHIPS ) /* we're a valid ship */
        {
          if ( strcmp(Users[uvec[offset]].username,
                      Users[Ships[snum].unum].username) == 0 &&
               Users[uvec[offset]].type == Users[Ships[snum].unum].type)
            color = NoColor | CQC_A_BOLD; /* it's ours */
          else if (Ships[snum].war[Users[uvec[offset]].team]) 
            color = RedLevelColor; /* we're at war with it */
          else if (Ships[snum].team == Users[uvec[offset]].team && !selfwar(snum))
            color = GreenLevelColor; /* it's a team ship */
          else
            color = YellowLevelColor;
        }
      else if (godlike)/* we are running conqoper */
        color = YellowLevelColor; /* bland view */
      else			/* we don't have a ship yet */
        {
          if ( strcmp(Users[uvec[offset]].username,
                      Users[Context.unum].username) == 0 &&
               Users[uvec[offset]].type == Users[Context.unum].type)
            color = NoColor | CQC_A_BOLD;    /* it's ours */
          else if (Users[Context.unum].war[Users[uvec[offset]].team]) 
            color = RedLevelColor; /* we're war with them (might be selfwar) */
          else if (Users[Context.unum].team == Users[uvec[offset]].team) 
            color = GreenLevelColor; /* team ship */
          else
            color = YellowLevelColor;
        }
      
      cprintf(lin, 0, ALIGN_CENTER, "#%d#%s", color, cbuf);

      offset = offset + 1;
      lin = lin + 1;
    }

  if ( offset >= nu )           /* last page */
    cprintf(MSG_LIN2, 0, ALIGN_CENTER, "#%d#%s", NoColor, MTXT_DONE);
  else
    cprintf(MSG_LIN2, 0, ALIGN_CENTER, "#%d#%s", NoColor, MTXT_MORE);
  
  return NODE_OK;
}  

static int nUserlIdle(void)
{
  int pkttype;
  Unsgn8 buf[PKT_MAXSIZE];
  int sockl[2] = {cInfo.sock, cInfo.usock};

  while ((pkttype = waitForPacket(PKT_FROMSERVER, sockl, PKT_ANYPKT,
                                  buf, PKT_MAXSIZE, 0, NULL)) > 0)
    processPacket(buf);

  if (pkttype < 0)          /* some error */
    {
      clog("nUserlIdle: waiForPacket returned %d", pkttype);
      Ships[Context.snum].status = SS_OFF;
      return NODE_EXIT;
    }

  if (clientFlags & SPCLNTSTAT_FLAG_KILLED && retnode == DSP_NODE_CP)
    {
      /* time to die properly. */
      nDeadInit();
      return NODE_OK;
    }
      

  return NODE_OK;
}
  
static int nUserlInput(int ch)
{
  if (ch == TERM_EXTRA)
    {
      fuser = 0;                /* move to first page */
      return NODE_OK;
    }

  if (offset < nu)
    {
      if (ch == ' ')
        {
          fuser = offset;
          return NODE_OK;
        }
    }

  /* go back */
  switch (retnode)
    {
    case DSP_NODE_CP:
      nCPInit();
      break;
    case DSP_NODE_MENU:
      nMenuInit();
      break;

    default:
      clog("nUserlInput: invalid return node: %d, going to DSP_NODE_MENU",
           retnode);
      nMenuInit();
      break;
    }

  /* NOTREACHED */
  return NODE_OK;
}
