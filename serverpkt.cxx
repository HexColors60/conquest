#include "c_defs.h"

/************************************************************************
 *
 * packet recEvent/pkt generation for the server
 *
 * Copyright Jon Trulson under the MIT License. (See LICENSE).
 ***********************************************************************/

#include "conqdef.h"
#include "cb.h"
#include "conf.h"
#include "server.h"
#include "context.h"
#include "serverpkt.h"
#include "conqlb.h"
#include "conqutil.h"
#include "rndlb.h"

/* Here, we maintain 2 copies of potential packets, privileged and
   unpriveleged.  We fill the packets, and then return a pointer to a static
   packet if it's different from the last one we processed, else NULL

   Note, seperate rec copies are only kept for those packets that
   can have restrictions (ship, torp, etc), since server recordings are
   not subject to those restrictions.

   This is pretty non-optimal and should be handled better via a per-item
   update counter instead of wholesale copy/compare.
*/

/* packet send */
static spShip_t *pktShip = NULL;
static spShipSml_t *pktShipSml = NULL;
static spShipLoc_t *pktShipLoc = NULL;
static spPlanet_t *pktPlanet = NULL;
static spPlanetSml_t *pktPlanetSml = NULL;
static spPlanetLoc_t *pktPlanetLoc = NULL;
static spPlanetLoc2_t *pktPlanetLoc2 = NULL;
static spUser_t *pktUser = NULL;
static spTorp_t **pktTorp = NULL;
static spTorpLoc_t **pktTorpLoc = NULL;
static spTorpEvent_t **pktTorpEvent = NULL;
static spTeam_t *pktTeam = NULL;
static spcbConqInfo_t *pktcbConqInfo = NULL;
static spHistory_t *pktHistory = NULL;
static spDoomsday_t *pktDoomsday = NULL;
static spPlanetInfo_t *pktPlanetInfo = NULL;

/* recording */
static spShip_t *recShip = NULL;
static spShipSml_t *recShipSml = NULL;
static spShipLoc_t *recShipLoc = NULL;
static spPlanet_t *recPlanet = NULL;
static spPlanetSml_t *recPlanetSml = NULL;
static spPlanetLoc_t *recPlanetLoc = NULL;
static spPlanetLoc2_t *recPlanetLoc2 = NULL;
static spTorp_t **recTorp = NULL;
static spTorpLoc_t **recTorpLoc = NULL;
static spTorpEvent_t **recTorpEvent = NULL;
static spTeam_t *recTeam = NULL;
static spDoomsday_t *recDoomsday = NULL;
static spPlanetInfo_t *recPlanetInfo = NULL;

/* allocate and memset everything to 0.  return false on success, true
 * on failure. */
bool spktInit(void)
{
    static bool firstTime = true;

    // can only run once...
    if (firstTime)
        firstTime = false;
    else
        return false; // should never call this twice anyway

    // if we fail one of the mallocs, we don't bother freeing what we
    // already allocated as the program will exit if this function
    // returns true.
    if (spktInitPkt())
        return true;
    if (spktInitRec())
        return true;

    return false;
}

#define MALLOC_ONED(_var, _type, _size)                                 \
    {                                                                   \
        if ( !(_var = (_type *)malloc(sizeof(_type) * _size )) )        \
            return true;                                                \
        memset((void *)(_var), 0, sizeof(_type) * _size);               \
    }

#define MALLOC_TWOD(_var, _type, _size1, _size2)                        \
    {                                                                   \
        if ( !(_var = (_type **)malloc(sizeof(_type *) * _size1 )) )    \
            return true;                                                \
        for (int i=0; i<_size1; i++)                                    \
        {                                                               \
            if ( !(_var[i] = (_type *)malloc(sizeof(_type) * _size2 )) ) \
                return true;                                            \
            memset((void *)_var[i], 0, sizeof(_type) * _size2);         \
        }                                                               \
    }

bool spktInitPkt(void)
{
    /* server pkt */
    MALLOC_ONED(pktShip, spShip_t, cbLimits.maxShips());
    MALLOC_ONED(pktShipSml, spShipSml_t, cbLimits.maxShips());
    MALLOC_ONED(pktShipLoc, spShipLoc_t, cbLimits.maxShips());
    MALLOC_ONED(pktPlanet, spPlanet_t, cbLimits.maxPlanets());
    MALLOC_ONED(pktPlanetSml, spPlanetSml_t, cbLimits.maxPlanets());
    MALLOC_ONED(pktPlanetLoc, spPlanetLoc_t, cbLimits.maxPlanets());
    MALLOC_ONED(pktPlanetLoc2, spPlanetLoc2_t, cbLimits.maxPlanets());
    MALLOC_ONED(pktUser, spUser_t, cbLimits.maxUsers());
    MALLOC_TWOD(pktTorp, spTorp_t, cbLimits.maxShips(), cbLimits.maxTorps());
    MALLOC_TWOD(pktTorpLoc, spTorpLoc_t, cbLimits.maxShips(),
                cbLimits.maxTorps());
    MALLOC_TWOD(pktTorpEvent, spTorpEvent_t, cbLimits.maxShips(),
                cbLimits.maxTorps());
    MALLOC_ONED(pktTeam, spTeam_t, NUMALLTEAMS);
    MALLOC_ONED(pktcbConqInfo, spcbConqInfo_t, 1);
    MALLOC_ONED(pktHistory, spHistory_t, cbLimits.maxHist());
    MALLOC_ONED(pktDoomsday, spDoomsday_t, 1);
    MALLOC_ONED(pktPlanetInfo, spPlanetInfo_t, cbLimits.maxPlanets());

    return false;
}

bool spktInitRec(void)
{
    /* recording */
    MALLOC_ONED(recShip, spShip_t, cbLimits.maxShips());
    MALLOC_ONED(recShipSml, spShipSml_t, cbLimits.maxShips());
    MALLOC_ONED(recShipLoc, spShipLoc_t, cbLimits.maxShips());
    MALLOC_ONED(recPlanet, spPlanet_t, cbLimits.maxPlanets());
    MALLOC_ONED(recPlanetSml, spPlanetSml_t, cbLimits.maxPlanets());
    MALLOC_ONED(recPlanetLoc, spPlanetLoc_t, cbLimits.maxPlanets());
    MALLOC_ONED(recPlanetLoc2, spPlanetLoc2_t, cbLimits.maxPlanets());
    MALLOC_TWOD(recTorp, spTorp_t, cbLimits.maxShips(),
                cbLimits.maxTorps());
    MALLOC_TWOD(recTorpLoc, spTorpLoc_t, cbLimits.maxShips(),
                cbLimits.maxTorps());
    MALLOC_TWOD(recTorpEvent, spTorpEvent_t, cbLimits.maxShips(),
                cbLimits.maxTorps());
    MALLOC_ONED(recTeam, spTeam_t, NUMALLTEAMS);
    MALLOC_ONED(recDoomsday, spDoomsday_t, 1);
    MALLOC_ONED(recPlanetInfo, spPlanetInfo_t, cbLimits.maxPlanets());

    return false;
}



/* non priv */
spUser_t *spktUser(uint16_t unum)
{
    static spUser_t suser;
    int i;

    memset((void *)&suser, 0, sizeof(spUser_t));

    suser.type = SP_USER;
    suser.team = (uint8_t)cbUsers[unum].team;
    suser.unum = htons(unum);
    suser.userType = (uint8_t)cbUsers[unum].type;
    suser.flags = htons(cbUsers[unum].flags);
    suser.opFlags = htons(cbUsers[unum].opFlags);

    for (i=0; i<NUMPLAYERTEAMS; i++)
        if (cbUsers[unum].war[i])
            suser.war |= (1 << i);

    suser.rating = (int16_t)htons((uint16_t)(cbUsers[unum].rating * 10.0));
    suser.lastentry = (uint32_t)htonl((uint32_t)cbUsers[unum].lastentry);

    for (i=0; i<USTAT_TOTALSTATS; i++)
        suser.stats[i] = (int32_t)htonl(cbUsers[unum].stats[i]);

    utStrncpy((char *)suser.username, cbUsers[unum].username, MAXUSERNAME);
    utStrncpy((char *)suser.alias, cbUsers[unum].alias, MAXUSERNAME);

    if (memcmp((void *)&suser, (void *)&pktUser[unum], sizeof(spUser_t)))
    {
        pktUser[unum] = suser;
        return &suser;
    }

    return NULL;
}

/* PRIV */
spShip_t *spktShip(uint8_t snum, int rec)
{
    int i;
    int mysnum = Context.snum;
    int myteam = cbShips[mysnum].team;
    static spShip_t sship;

    memset((void *)&sship, 0, sizeof(spShip_t));

    sship.type = SP_SHIP;
    sship.status = cbShips[snum].status;
    sship.snum = snum;
    sship.team = cbShips[snum].team;
    sship.unum = htons(cbShips[snum].unum);
    sship.shiptype = cbShips[snum].shiptype;

    /* RESTRICT */
    /* really only valid for own ship */
    if ((mysnum == snum) || rec)
    {
        for (i=0; i<NUMPLAYERTEAMS; i++)
	{
            if (cbShips[snum].war[i])
                sship.war |= (1 << i);
            if (cbShips[snum].rwar[i])
                sship.rwar |= (1 << i);
	}

        sship.killedBy = (uint8_t)cbShips[snum].killedBy;
        sship.killedByDetail = htons(cbShips[snum].killedByDetail);

        // encode the srpwar (Self Ruled Planet War) bits
        for (i=0; i<MAXPLANETS; i++)
        {
            if (cbShips[snum].srpwar[i])
            {
                int word = i / sizeof(uint32_t);
                int bit = i % sizeof(uint32_t);

                sship.srpwar[word] |= (1 << bit);
            }
        }

        // Now reformat for network transmission
        for (i=0; i<PROTO_SRPWAR_BIT_WORDS; i++)
            sship.srpwar[i] = htonl(sship.srpwar[i]);

    }
    else
    {
        /* RESTRICT */
        /* we only send the war stats relating to our team */
        if (cbShips[snum].war[myteam])
            sship.war |= (1 << myteam);

        if (cbShips[snum].rwar[myteam])
            sship.rwar |= (1 << myteam);
    }

    /* for robots, we need to account for strkills as well.
       we won't bother adding strkills to the packet, as the client doesn't
       really need to care whether these are 'real' kills vs. random ones. */
    if (SROBOT(snum))
        sship.kills =
            htonl((uint32_t)((cbShips[snum].kills + cbShips[snum].strkills) * 10.0));
    else
        sship.kills = htonl((uint32_t)(cbShips[snum].kills * 10.0));

    for (i=0; i<NUMPLAYERTEAMS; i++)
        sship.scanned[i] = (uint8_t)cbShips[snum].scanned[i];

    utStrncpy((char *)sship.alias, cbShips[snum].alias, MAXUSERNAME);

    if (rec)
    {
        if (memcmp((void *)&sship, (void *)&recShip[snum], sizeof(spShip_t)))
        {
            recShip[snum] = sship;
            return &sship;
        }
    }
    else
    {
        if (memcmp((void *)&sship, (void *)&pktShip[snum], sizeof(spShip_t)))
        {
            pktShip[snum] = sship;
            return &sship;
        }
    }

    return NULL;
}

/* PRIV */
spShipSml_t *spktShipSml(uint8_t snum, int rec)
{
    int mysnum = Context.snum;
    static spShipSml_t sshipsml;
    int canscan = false;
    real dis;
    uint16_t sflags = 0;		/* ship flags we are allowed to see */
    uint16_t scanflag = 0;         /* set to SHIP_F_SCANDIST if < ACCINFO_DIST */

    memset((void *)&sshipsml, 0, sizeof(spShipSml_t));

    sflags = SHIP_F_NONE;		/* can't see anything by default.  make sure
                                           SHIP_F_MAP never sneaks in */

    sshipsml.type = SP_SHIPSML;
    sshipsml.snum = snum;

    /* can always see these */
    sflags |= (SHIP_F_CLOAKED | SHIP_F_ROBOT | SHIP_F_VACANT | SHIP_F_SCANDIST);

    if ((snum == mysnum) || rec)
    {			     /* really only useful for our own ship */
        sflags |= (SHIP_F_REPAIR | SHIP_F_TALERT | SHIP_F_BOMBING
                   | SHIP_F_TOWEDBY | SHIP_F_TOWING);

        sshipsml.towing = cbShips[snum].towing;
        sshipsml.towedby = cbShips[snum].towedby;

        sshipsml.action = (uint8_t)cbShips[snum].action;
        sshipsml.lastblast = htons((uint16_t)(cbShips[snum].lastblast * 100.0));
        sshipsml.fuel = htons((uint16_t)cbShips[snum].fuel);
        sshipsml.lock = (uint8_t)cbShips[snum].lock;
        sshipsml.lockDetail = htons(cbShips[snum].lockDetail);
        sshipsml.sdfuse = (int16_t)htons((uint16_t)cbShips[snum].sdfuse);
        sshipsml.wfuse = (int8_t)cbShips[snum].wfuse;
        sshipsml.efuse = (int8_t)cbShips[snum].efuse;
        sshipsml.walloc = cbShips[snum].weapalloc;
        sshipsml.etemp = (uint8_t)cbShips[snum].etemp;
        sshipsml.wtemp = (uint8_t)cbShips[snum].wtemp;
        canscan = true;	      /* can always scan ourselves */
    }
    else
    {
        if (!SCLOAKED(snum))
            dis = (real) dist(cbShips[mysnum].x, cbShips[mysnum].y,
                              cbShips[snum].x,
                              cbShips[snum].y );
        else
            dis = (real) dist(cbShips[mysnum].x, cbShips[mysnum].y,
                              rndnor(cbShips[snum].x, CLOAK_SMEAR_DIST),
                              rndnor(cbShips[snum].y, CLOAK_SMEAR_DIST));

        /* if in accurate scanning distance (regardless of cloak) set the
           SCANDIST flag. */

        if (dis < ACCINFO_DIST)
            scanflag = SHIP_F_SCANDIST;

        /* help the driver, set scanned fuse */
        if ( (dis < ACCINFO_DIST && ! SCLOAKED(snum)) && ! selfwar(mysnum) )
            cbShips[snum].scanned[cbShips[mysnum].team] = SCANNED_FUSE;

        /* if within accurate dist and not cloaked, or
           if ship scanned by my team and not selfwar, or
           if not at war with ship. */

        canscan = ( (dis < ACCINFO_DIST && ! SCLOAKED(snum)) ||
                    ( (cbShips[snum].scanned[cbShips[mysnum].team] > 0) &&
                      ! selfwar(mysnum) ) ||
                    !satwar(snum, mysnum));
    }

    if (canscan)
    {				/* if we get all the stats */
        sflags |= SHIP_F_SHUP | SHIP_F_BOMBING | SHIP_F_REPAIR;

        sshipsml.shields = (uint8_t)cbShips[snum].shields;
        sshipsml.damage = (uint8_t)cbShips[snum].damage;
        sshipsml.armies = cbShips[snum].armies;

        /* so we can do bombing */
        sshipsml.lock = (uint8_t)cbShips[snum].lock;
        sshipsml.lockDetail = htons(cbShips[snum].lockDetail);

        /* so we can disp phasers in graphical client ;-) */
        sshipsml.lastphase = htons((uint16_t)(cbShips[snum].lastphase * 100.0));
        sshipsml.pfuse = (int8_t)cbShips[snum].pfuse;
    }

    /* only send those we are allowed to see */
    sshipsml.flags = htonl(((cbShips[snum].flags | scanflag) & sflags));

    if (rec)
    {
        if (memcmp((void *)&sshipsml, (void *)&recShipSml[snum],
                   sizeof(spShipSml_t)))
        {
            recShipSml[snum] = sshipsml;
            return &sshipsml;
        }
    }
    else
    {
        if (memcmp((void *)&sshipsml, (void *)&pktShipSml[snum],
                   sizeof(spShipSml_t)))
        {
            pktShipSml[snum] = sshipsml;
            return &sshipsml;
        }
    }

    return NULL;
}

/* PRIV */
spShipLoc_t *spktShipLoc(uint8_t snum, int rec)
{
    int mysnum = Context.snum;
    static spShipLoc_t sshiploc;
    real x, y;
    int canscan = false;
    real dis;
    static const uint32_t maxtime = 5000;  /* 5 seconds */
    static uint32_t lasttime = 0;
    uint32_t thetime = clbGetMillis();
    static int forceMyShip = true;

    memset((void *)&sshiploc, 0, sizeof(spShipLoc_t));

    sshiploc.type = SP_SHIPLOC;
    sshiploc.snum = snum;
    sshiploc.warp = (int8_t)(cbShips[snum].warp * 10.0);

    /* we need to ensure that if we are doing UDP, and we haven't
       updated a ship in awhile (causing UDP traffic), force an update
       of your ship even if it isn't neccessary.  This should help with
       those firewalls that disconnect a UDP connection when there has
       been no trafic on it for a while. */
    if (sInfo.doUDP && (snum == mysnum) && ((thetime - lasttime) > maxtime))
    {
        lasttime = thetime;
        forceMyShip = true;
    }

    /* RESTRICT */
    if ((snum == mysnum) || rec)
    {				/* we get everything */
        sshiploc.head = htons((uint16_t)(cbShips[snum].head * 10.0));
        x = cbShips[snum].x;
        y = cbShips[snum].y;
    }
    else
    {
        if (SCLOAKED(snum))
        {
            if (cbShips[snum].warp == 0.0)
            {
                x = 1e7;
                y = 1e7;
            }
            else
            { /* if your cloaked, and moving, get smeared x/y */
                x = rndnor( cbShips[snum].x, CLOAK_SMEAR_DIST );
                y = rndnor( cbShips[snum].y, CLOAK_SMEAR_DIST );
            }
        }
        else
	{			/* not cloaked */
            dis = (real) dist(cbShips[mysnum].x, cbShips[mysnum].y,
                              cbShips[snum].x,
                              cbShips[snum].y );

            canscan = ( (dis < ACCINFO_DIST && ! SCLOAKED(snum)) ||
                        ( (cbShips[snum].scanned[cbShips[mysnum].team] > 0) &&
                          ! selfwar(mysnum) ) ||
                        !satwar(snum, mysnum));

            if (canscan)		/* close or friendly */
                sshiploc.head = htons((uint16_t)(cbShips[snum].head * 10.0));

            x = cbShips[snum].x;
            y = cbShips[snum].y;

	}
    }

    sshiploc.x = (int32_t)htonl((int32_t)(x * 1000.0));
    sshiploc.y = (int32_t)htonl((int32_t)(y * 1000.0));

    if (rec)
    {
        if (memcmp((void *)&sshiploc, (void *)&recShipLoc[snum],
                   sizeof(spShipLoc_t)))
        {
            recShipLoc[snum] = sshiploc;
            return &sshiploc;
        }
    }
    else
    {
        if (forceMyShip || memcmp((void *)&sshiploc, (void *)&pktShipLoc[snum],
                                  sizeof(spShipLoc_t)))
        {
            pktShipLoc[snum] = sshiploc;
            /* if we are doing udp, and we are going to send a packet,
               reset the timer so we won't need a nother force (as we do
               not need one now :) */
            if (sInfo.doUDP)
                lasttime = thetime;
            forceMyShip = false;
            return &sshiploc;
        }
    }

    return NULL;
}

spPlanet_t *spktPlanet(uint8_t pnum, int rec)
{
    int snum = Context.snum;
    int team = cbShips[snum].team;
    static spPlanet_t splan;

    memset((void *)&splan, 0, sizeof(spPlanet_t));

#if defined(DEBUG_SERVERSEND)
    utLog("sendPlanet: pnum = %d",
          pnum);
#endif

    splan.type = SP_PLANET;
    splan.pnum = pnum;
    splan.ptype = cbPlanets[pnum].type;

    /* RESTRICT */
    if (cbPlanets[pnum].scanned[team] || rec)
        splan.team = cbPlanets[pnum].team;
    else
        splan.team = TEAM_SELFRULED; /* until we know for sure... */

    // who's homeworld is this (if a homeplanet)?
    splan.defendteam = cbPlanets[pnum].defendteam;

    // how big is it?
    splan.size = htons(cbPlanets[pnum].size);

    utStrncpy((char *)splan.name, cbPlanets[pnum].name, MAXPLANETNAME);

    if (rec)
    {
        if (memcmp((void *)&splan, (void *)&recPlanet[pnum],
                   sizeof(spPlanet_t)))
        {
            recPlanet[pnum] = splan;
            return &splan;
        }
    }
    else
    {
        if (memcmp((void *)&splan, (void *)&pktPlanet[pnum],
                   sizeof(spPlanet_t)))
        {
            pktPlanet[pnum] = splan;
            return &splan;
        }
    }

    return NULL;
}

spPlanetSml_t *spktPlanetSml(uint8_t pnum, int rec)
{
    int i;
    int snum = Context.snum;
    int team = cbShips[snum].team;
    static spPlanetSml_t splansml;

    memset((void *)&splansml, 0, sizeof(spPlanetSml_t));

    splansml.type = SP_PLANETSML;
    splansml.pnum = pnum;

    /* RESTRICT */
    if (rec)
    {
        for (i=0; i < NUMPLAYERTEAMS; i++)
            if (cbPlanets[pnum].scanned[i])
                splansml.scanned |= (1 << i);

        splansml.uninhabtime = (uint8_t)cbPlanets[pnum].uninhabtime;
    }
    else
    {
        if (cbPlanets[pnum].scanned[team])
        {
            splansml.scanned |= (1 << team);
            splansml.uninhabtime = (uint8_t)cbPlanets[pnum].uninhabtime;
        }
    }

    if (rec)
    {
        if (memcmp((void *)&splansml, (void *)&recPlanetSml[pnum],
                   sizeof(spPlanetSml_t)))
        {
            recPlanetSml[pnum] = splansml;
            return &splansml;
        }
    }
    else
    {
        if (memcmp((void *)&splansml, (void *)&pktPlanetSml[pnum],
                   sizeof(spPlanetSml_t)))
        {
            pktPlanetSml[pnum] = splansml;
            return &splansml;
        }
    }

    return NULL;
}

spPlanetLoc_t *spktPlanetLoc(uint8_t pnum, int rec, int force)
{
    int snum = Context.snum;
    int team = cbShips[snum].team;
    static spPlanetLoc_t splanloc;
    real dx, dy;
    static real px[MAXPLANETS] = {}; /* saved x/y */
    static real py[MAXPLANETS] = {};

    memset((void *)&splanloc, 0, sizeof(spPlanetLoc_t));

    splanloc.type = SP_PLANETLOC;
    splanloc.pnum = pnum;

    /* RESTRICT */
    if (cbPlanets[pnum].scanned[team] || rec)
        splanloc.armies = htons(cbPlanets[pnum].armies);

    dx = (real)fabs(cbPlanets[pnum].x - px[pnum]);
    dy = (real)fabs(cbPlanets[pnum].y - py[pnum]);


    /* we try to be clever here by reducing the pkt count.  If armies are
       the same, and an average delta of the planet's movement is below
       an empirically determined value, don't bother sending the packet.

       The idea is that fast moving planets will be updated more frequently
       than slower moving ones, hopefully reducing the packet count required
       for the appearence of smoother movement to the user.
    */
    if ((splanloc.armies == pktPlanetLoc[pnum].armies) &&
        ((dx + dy) / 2.0) < 3.0)
    {
#if 0
        utLog("REJECT: %s dx = %f dy = %f [%f]", cbPlanets[pnum].name,
              dx, dy,
              ((dx + dy) / 2.0));
#endif

        if (!rec && !force)
            return NULL;
    }

    if (!rec)
    {
        px[pnum] = cbPlanets[pnum].x;
        py[pnum] = cbPlanets[pnum].y;
    }

#if 0
    utLog("%s dx = %f dy = %f [%f]", cbPlanets[pnum].name,
          dx, dy,
          ((dx + dy) / 2.0));
#endif

    splanloc.x = (int32_t)htonl((int32_t)(cbPlanets[pnum].x * 1000.0));
    splanloc.y = (int32_t)htonl((int32_t)(cbPlanets[pnum].y * 1000.0));

    if (rec)
    {
        if (memcmp((void *)&splanloc, (void *)&recPlanetLoc[pnum],
                   sizeof(spPlanetLoc_t)))
        {
            recPlanetLoc[pnum] = splanloc;
            return &splanloc;
        }
    }
    else
    {
        if (memcmp((void *)&splanloc, (void *)&pktPlanetLoc[pnum],
                   sizeof(spPlanetLoc_t)))
        {
            pktPlanetLoc[pnum] = splanloc;
            return &splanloc;
        }
    }

    return NULL;
}

spPlanetLoc2_t *spktPlanetLoc2(uint8_t pnum, int rec, int force)
{
    int snum = Context.snum;
    int team = cbShips[snum].team;
    static spPlanetLoc2_t splanloc2;
    uint32_t iternow = clbGetMillis(); /* we send the loc2 packets only every 5 secs */
    const uint32_t iterwait = 5000.0; /* ms */
    static uint32_t tstart[MAXPLANETS] = {}; /* saved time deltas */
    int tooearly = false;

    /*
     * We have to handle the case where a planet has just been freshly
     * scanned (and therefore a real army count is available).  We want
     * the client to see the true army count as soon as possible.
     */

    if (!force && (tstart[pnum] != 0 && ((iternow - tstart[pnum]) < iterwait)))
        tooearly = true;

    memset((void *)&splanloc2, 0, sizeof(spPlanetLoc2_t));

    splanloc2.type = SP_PLANETLOC2;
    splanloc2.pnum = pnum;

    /* RESTRICT */
    if (cbPlanets[pnum].scanned[team] || rec)
        splanloc2.armies = htons(cbPlanets[pnum].armies);

    if (splanloc2.armies == pktPlanetLoc2[pnum].armies && tooearly)
        return NULL;

    tstart[pnum] = iternow;

    splanloc2.x = (int32_t)htonl((int32_t)(cbPlanets[pnum].x * 1000.0));
    splanloc2.y = (int32_t)htonl((int32_t)(cbPlanets[pnum].y * 1000.0));
    splanloc2.orbang = (uint16_t)htons((uint16_t)(cbPlanets[pnum].orbang * 100.0));

    if (rec)
    {
        if (memcmp((void *)&splanloc2, (void *)&recPlanetLoc2[pnum],
                   sizeof(spPlanetLoc2_t)))
        {
            recPlanetLoc2[pnum] = splanloc2;
            return &splanloc2;
        }
    }
    else
    {
        if (memcmp((void *)&splanloc2, (void *)&pktPlanetLoc2[pnum],
                   sizeof(spPlanetLoc2_t)))
        {
            pktPlanetLoc2[pnum] = splanloc2;
            return &splanloc2;
        }
    }

    return NULL;
}



/* non priv */
spTorp_t *spktTorp(uint8_t tsnum, uint8_t tnum, int rec)
{
    static spTorp_t storp;

    memset((void *)&storp, 0, sizeof(spTorp_t));

    storp.type = SP_TORP;
    storp.snum = tsnum;
    storp.tnum = tnum;
    storp.status = (uint8_t)cbShips[tsnum].torps[tnum].status;

    if (rec)
    {
        if (memcmp((void *)&storp, (void *)&(recTorp[tsnum][tnum]),
                   sizeof(spTorp_t)))
        {
            recTorp[tsnum][tnum] = storp;
            return &storp;
        }
    }
    else
    {
        if (memcmp((void *)&storp, (void *)&(pktTorp[tsnum][tnum]),
                   sizeof(spTorp_t)))
        {
            pktTorp[tsnum][tnum] = storp;
            return &storp;
        }
    }

    return NULL;
}

/* PRIV */
spTorpLoc_t *spktTorpLoc(uint8_t tsnum, uint8_t tnum, int rec)
{
    int i;
    int snum = Context.snum;
    int team = cbShips[snum].team;
    static spTorpLoc_t storploc;
    real dis;
    real x, y;

    memset((void *)&storploc, 0, sizeof(spTorpLoc_t));

    storploc.type = SP_TORPLOC;
    storploc.snum = tsnum;
    storploc.tnum = tnum;

    /* RESTRICT */
    /* we can always see friendly torps.  enemy torps can only be seen if
       within ACCINFO_DIST of your ship.  torp war stat only applies to
       your ship. */

    x = cbShips[tsnum].torps[tnum].x;
    y = cbShips[tsnum].torps[tnum].y;

    if (cbShips[tsnum].torps[tnum].war[team] && !rec)
    {				/* it's at war with us. bastards. */
        /* see if it's close enough to scan */
        dis = (real) dist(cbShips[snum].x, cbShips[snum].y,
                          cbShips[tsnum].torps[tnum].x,
                          cbShips[tsnum].torps[tnum].y );

        if (dis > ACCINFO_DIST)
        {                       /* in the bermuda triangle */
            x = 1e7;
            y = 1e7;
        }
    }

    storploc.x = (int32_t)htonl((int32_t)(x * 1000.0));
    storploc.y = (int32_t)htonl((int32_t)(y * 1000.0));

    if (rec)
    {
        for (i=0; i < NUMPLAYERTEAMS; i++)
            if (cbShips[tsnum].torps[tnum].war[i])
                storploc.war |= (1 << i);
    }
    else
    {
        /* RESTRICT */
        /* only send 'war' status as it relates to our team */
        if (cbShips[tsnum].torps[tnum].war[team])
            storploc.war |= (1 << team);
    }

    if (rec)
    {
        if (memcmp((void *)&storploc, (void *)&(recTorpLoc[tsnum][tnum]),
                   sizeof(spTorpLoc_t)))
        {
            recTorpLoc[tsnum][tnum] = storploc;
            return &storploc;
        }
    }
    else
    {
        if (memcmp((void *)&storploc, (void *)&(pktTorpLoc[tsnum][tnum]),
                   sizeof(spTorpLoc_t)))
        {
            pktTorpLoc[tsnum][tnum] = storploc;
            return &storploc;
        }
    }

    return NULL;
}

/* PRIV */
spTorpEvent_t *spktTorpEvent(uint8_t tsnum, uint8_t tnum, int rec)
{
    int i;
    int snum = Context.snum;
    int team = cbShips[snum].team;
    static spTorpEvent_t storpev;
    real x, y, dx, dy;

    memset((void *)&storpev, 0, sizeof(spTorpEvent_t));

    storpev.type = SP_TORPEVENT;
    storpev.snum = tsnum;
    storpev.tnum = tnum;
    storpev.status = (uint8_t)cbShips[tsnum].torps[tnum].status;

    /* RESTRICT */
    /* torp war stat only applies to your ship. */

    x = cbShips[tsnum].torps[tnum].x;
    y = cbShips[tsnum].torps[tnum].y;
    dx = cbShips[tsnum].torps[tnum].dx;
    dy = cbShips[tsnum].torps[tnum].dy;

    storpev.x = (int32_t)htonl((int32_t)(x * 1000.0));
    storpev.y = (int32_t)htonl((int32_t)(y * 1000.0));
    storpev.dx = (int32_t)htonl((int32_t)(dx * 1000.0));
    storpev.dy = (int32_t)htonl((int32_t)(dy * 1000.0));

    if (rec)
    {
        for (i=0; i < NUMPLAYERTEAMS; i++)
            if (cbShips[tsnum].torps[tnum].war[i])
                storpev.war |= (1 << i);
    }
    else
    {
        /* RESTRICT */
        /* only send 'war' status as it relates to our team */
        if (cbShips[tsnum].torps[tnum].war[team])
            storpev.war |= (1 << team);
    }

    /* we only do these if the torp status changed */
    if (rec)
    {
        if (storpev.status != recTorpEvent[tsnum][tnum].status)
        {
            recTorpEvent[tsnum][tnum] = storpev;
            return &storpev;
        }
    }
    else
    {
        if (storpev.status != pktTorpEvent[tsnum][tnum].status)
        {
            pktTorpEvent[tsnum][tnum] = storpev;
            return &storpev;
        }
    }

    return NULL;
}

/* PRIV */
spTeam_t *spktTeam(uint8_t team, int force, int rec)
{
    int snum = Context.snum;
    static spTeam_t steam;
    int i;

    memset((void *)&steam, 0, sizeof(spTeam_t));

    steam.type = SP_TEAM;
    steam.team = team;
    steam.homeplanet = (uint8_t)cbTeams[team].homeplanet;

    /* RESTRICT */
    if ((cbShips[snum].team == team) || rec)
    {				/* we only send this stuff for our team */
        if (cbTeams[team].coupinfo)
            steam.flags |= SPTEAM_FLAGS_COUPINFO;

        steam.couptime = (uint8_t)cbTeams[team].couptime;
    }

    for (i=0; i<MAXTSTATS; i++)
        steam.stats[i] = (uint32_t)htonl(cbTeams[team].stats[i]);

    utStrncpy((char *)steam.name, cbTeams[team].name, MAXTEAMNAME);

    if (rec)
    {
        if (memcmp((void *)&steam, (void *)&recTeam[team],
                   sizeof(spTeam_t)) || force)
        {
            recTeam[team] = steam;
            return &steam;
        }
    }
    else
    {
        if (memcmp((void *)&steam, (void *)&pktTeam[team],
                   sizeof(spTeam_t)) || force)
        {
            pktTeam[team] = steam;
            return &steam;
        }
    }

    return NULL;
}

spcbConqInfo_t *spktcbConqInfo(int force)
{
    static spcbConqInfo_t spci;

    memset((void *)&spci, 0, sizeof(spcbConqInfo_t));

    spci.type = SP_CONQINFO;

    utStrncpy((char *)spci.conqueror, cbConqInfo->conqueror, MAXUSERNAME);
    utStrncpy((char *)spci.conqteam, cbConqInfo->conqteam, MAXTEAMNAME);
    utStrncpy((char *)spci.conqtime, cbConqInfo->conqtime, MAXDATESIZE);
    utStrncpy((char *)spci.lastwords, cbConqInfo->lastwords, MAXLASTWORDS);

    if (memcmp((void *)&spci, (void *)pktcbConqInfo,
               sizeof(spcbConqInfo_t)) || force)
    {
        *pktcbConqInfo = spci;
        return &spci;
    }

    return NULL;
}

spHistory_t *spktHistory(int hnum)
{
    static spHistory_t hist;

    memset((void *)&hist, 0, sizeof(spHistory_t));

    hist.type = SP_HISTORY;
    hist.hnum = hnum;

    hist.histptr = cbConqInfo->histptr;

    hist.unum = (uint16_t)htons((uint16_t)cbHistory[hnum].unum);

    hist.elapsed = (uint32_t)htonl((uint32_t)cbHistory[hnum].elapsed);
    hist.enterTime = (uint32_t)htonl((uint32_t)cbHistory[hnum].enterTime);

    utStrncpy((char *)hist.username, cbHistory[hnum].username, MAXUSERNAME);

    if (memcmp((void *)&hist, (void *)&pktHistory[hnum], sizeof(spHistory_t)))
    {
        pktHistory[hnum] = hist;
        return &hist;
    }

    return NULL;
}

spDoomsday_t *spktDoomsday(int rec)
{
    static spDoomsday_t dd;

    memset((void *)&dd, 0, sizeof(spDoomsday_t));

    dd.type = SP_DOOMSDAY;
    dd.heading = htons((uint16_t)(cbDoomsday->heading * 10.0));
    dd.x = (int32_t)htonl((int32_t)(cbDoomsday->x * 1000.0));
    dd.y = (int32_t)htonl((int32_t)(cbDoomsday->y * 1000.0));

    dd.eaterType = static_cast<uint8_t>(cbDoomsday->eaterType);
    dd.flags = cbDoomsday->flags;

    if (rec)
    {
        if (memcmp((void *)&dd, (void *)recDoomsday,
                   sizeof(spDoomsday_t)))
        {
            *recDoomsday = dd;
            return &dd;
        }
    }
    else
    {
        if (memcmp((void *)&dd, (void *)pktDoomsday,
                   sizeof(spDoomsday_t)))
        {
            *pktDoomsday = dd;
            return &dd;
        }
    }

    return NULL;
}

spPlanetInfo_t *spktPlanetInfo(uint8_t pnum, int rec)
{
    static spPlanetInfo_t splaninfo;

    memset((void *)&splaninfo, 0, sizeof(spPlanetInfo_t));

    splaninfo.type = SP_PLANETINFO;
    splaninfo.pnum = pnum;
    splaninfo.flags = htonl(cbPlanets[pnum].flags);

    splaninfo.primary = (uint8_t)cbPlanets[pnum].primary;

    splaninfo.orbrad = (uint32_t)htonl((uint32_t)(cbPlanets[pnum].orbrad * 10.0));
    splaninfo.orbvel = (int32_t)htonl((int32_t)(cbPlanets[pnum].orbvel * 100.0));

    if (rec)
    {
        if (memcmp((void *)&splaninfo, (void *)&recPlanetInfo[pnum],
                   sizeof(spPlanetInfo_t)))
        {
            recPlanetInfo[pnum] = splaninfo;
            return &splaninfo;
        }
    }
    else
    {
        if (memcmp((void *)&splaninfo, (void *)&pktPlanetInfo[pnum],
                   sizeof(spPlanetInfo_t)))
        {
            pktPlanetInfo[pnum] = splaninfo;
            return &splaninfo;
        }
    }

    return NULL;
}
