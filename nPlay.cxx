/*
 * play node.  Sets up for the cockpit (CP).
 *
 * Copyright Jon Trulson under the MIT License. (See LICENSE).
 */

#include "c_defs.h"
#include "context.h"
#include "global.h"

#include "color.h"
#include "conf.h"
#include "cb.h"
#include "gldisplay.h"
#include "glmisc.h"
#include "node.h"
#include "client.h"
#include "record.h"
#include "conqutil.h"

#include "nMenu.h"
#include "nCP.h"
#include "nPlay.h"
#include "cqkeys.h"

#define S_DONE       0          /* ready to play */
#define S_NSERR      1          /* _newship error */
#define S_SELSYS     2          /* select system */
#define S_MENU       3          /* go back to menu */

static int state;
static int fatal = FALSE;
static int shipinited = FALSE;   /* whether we've done _newship() yet */
static int owned[NUMPLAYERTEAMS];

static int nPlayDisplay(dspConfig_t *);
static int nPlayIdle(void);
static int nPlayInput(int ch);

static scrNode_t nPlayNode = {
    nPlayDisplay,               /* display */
    nPlayIdle,                  /* idle */
    nPlayInput,                  /* input */
    NULL,                         /* minput */
    NULL                          /* animQue */
};

/* select a system to enter */
static void selectentry( uint8_t esystem )
{
    int i;
    char cbuf[BUFFER_SIZE_256];

    /* First figure out which systems we can enter from. */
    for ( i = 0; i < NUMPLAYERTEAMS; i++ )
        if (esystem & (1 << i))
        {
            owned[i] = TRUE;
        }
        else
            owned[i] = FALSE;

    /* Prompt for a decision. */
    strcpy(cbuf , "Enter which system") ;
    for ( i = 0; i < NUMPLAYERTEAMS; i++ )
        if ( owned[i] )
        {
            strcat(cbuf, ", ");
            strcat(cbuf , cbTeams[i].name) ;
        }

    /* Change first comma to a colon. */
    char *sptr = strchr(cbuf, ',');
    if (sptr)
        *sptr = ':';

    cprintf(12, 0, ALIGN_CENTER, cbuf);

    return;
}



/*  _newship - here we will await a ClientStat from the server (indicating
    our possibly new ship), or a NAK indicating a problem.
*/
static int _newship( int unum, int *snum )
{
    int pkttype;
    char buf[PKT_MAXSIZE];

    /* here we will wait for ack's or a clientstat pkt. Acks indicate an
       error.  If the clientstat pkt's esystem is !0, we need to prompt
       for the system to enter and send it in a CP_COMMAND:CPCMD_ENTER
       pkt. */


    while (TRUE)
    {
        if ((pkttype = pktWaitForPacket(PKT_ANYPKT,
                                        buf, PKT_MAXSIZE, 60, NULL)) < 0)
	{
            utLog("nPlay: _newship: waitforpacket returned %d", pkttype);
            fatal = TRUE;
            return FALSE;
	}

        switch (pkttype)
	{
	case 0:			/* timeout */
            return FALSE;
            break;

	case SP_ACK:		/* bummer */
            PKT_PROCSP(buf);
            utLog("%s: got an ack, _newship failed.", __FUNCTION__);
            state = S_NSERR;

            return FALSE;		/* always a failure */
            break;

	case SP_CLIENTSTAT:
        {
            if (PKT_PROCSP(buf))
            {
                return TRUE;
            }
            else
            {
                utLog("nPlay: _newship: invalid CLIENTSTAT");
                return FALSE;
            }
        }
        break;

        /* we might get other packets too */
	default:
            processPacket(buf);
            break;
	}
    }

    /* if we are here, something unexpected happened */
    return FALSE;			/* NOTREACHED */


}

void nPlayInit(void)
{
    state = S_SELSYS;               /* default */
    shipinited = FALSE;
    /* let the server know our intentions */
    if (!sendCommand(CPCMD_ENTER, 0))
        fatal = TRUE;

    setNode(&nPlayNode);

    return;
}


static int nPlayDisplay(dspConfig_t *dsp)
{
    char cbuf[BUFFER_SIZE_256];
    int i, j;

    if (fatal)
        return NODE_EXIT;

    if (state == S_SELSYS)
    {
        if (!shipinited)
        {                       /* need to call _newship */
            shipinited = TRUE;
            if (!_newship( Context.unum, &Context.snum ))
            {
                utLog("%s: _newship failed.", __FUNCTION__);
                state = S_NSERR;
                return NODE_OK;
            }
        }

        if (!sClientStat.esystem)
        {                       /* we are ready  */
            state = S_DONE;
            return NODE_OK;
        }
        else
        {                       /* need to display/get a selection */
            selectentry(sClientStat.esystem);
        }
    }
    else if (state == S_NSERR)
    {
        // FIXME - this needs to be fail tested! inf loop once when
        // trying to re-enter the game, but got a ping resp (ack code
        // 17) instead.  Just ran in loop dumping "nPlay: unexpected
        // server ack, code 17" to console
        switch (sAckMsg.code)
        {
        case PERR_FLYING:
            sprintf(cbuf, "You're already playing on another ship.");
            cprintf(5,0,ALIGN_CENTER,"#%d#%s",InfoColor, cbuf);
            cbShips[Context.snum].status = SS_RESERVED;
            break;

        default:
            cprintf(5,0,ALIGN_CENTER,
                    "#%d#nPlay: _newship: unexpected server ack, code %d",
                    InfoColor, sAckMsg.code);
            utLog("nPlay: unexpected server ack, code %d",
                  sAckMsg.code);
            break;
        }
        /* Press any key... */
        cprintf(MSG_LIN2, 0, ALIGN_CENTER, MTXT_DONE);
    }


    return NODE_OK;
}

static int nPlayIdle(void)
{
    if (state == S_DONE)
    {
        Context.entship = TRUE;
        cbShips[Context.snum].sdfuse = 0;       /* zero self destruct fuse */
        utGrand( &Context.msgrand );            /* initialize message timer */
        Context.leave = FALSE;                /* assume we won't want to bail */
        Context.redraw = TRUE;                /* want redraw first time */
        Context.msgok = TRUE;                 /* ok to get messages */

        Context.display = TRUE;               /* ok to display */

        /* start recording if neccessary */
        if (Context.recmode == RECMODE_STARTING)
        {
            if (recInitOutput(Context.unum, time(0), Context.snum,
                              FALSE))
            {
                Context.recmode = RECMODE_ON;
            }
            else
                Context.recmode = RECMODE_OFF;
        }

        /* need to tell the server to resend all the crap it already
           sent in menu - our ship may have chenged */
        sendCommand(CPCMD_RELOAD, 0);

        nCPInit(TRUE);            /* play */
    }
    else if (state == S_MENU)
        nMenuInit();

    return NODE_OK;
}

static int nPlayInput(int ch)
{
    int i;
    unsigned char c = CQ_CHAR(ch);

    switch (state)
    {
    case S_SELSYS:              /* we are selecting our system */
    {
        switch  ( ch )
        {
        case TERM_NORMAL:
        case TERM_ABORT:	/* doesn't like the choices ;-) */
            sendCommand(CPCMD_ENTER, 0);
            state = S_MENU;
            return NODE_OK;
            break;
        case TERM_EXTRA:
            /* Enter the home system. */
            sendCommand(CPCMD_ENTER, (uint16_t)(1 << cbShips[Context.snum].team));
            state = S_DONE;
            return NODE_OK;
            break;
        default:
            for ( i = 0; i < NUMPLAYERTEAMS; i++ )
                if ( cbTeams[i].teamchar == (char)toupper(c) && owned[i] )
                {
                    /* Found a good one. */
                    sendCommand(CPCMD_ENTER, (uint16_t)(1 << i));
                    state = S_DONE;
                    return NODE_OK;
                }

            /* Didn't get a good one; complain and try again. */
            mglBeep(MGL_BEEP_ERR);
            break;
        }
    }

    break;

    case S_NSERR:               /* any key to return */
        nMenuInit();

        return NODE_OK;
        break;
    }

    return NODE_OK;
}