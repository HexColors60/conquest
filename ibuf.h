/************************************************************************
 *
 * $Id$
 *
 * Copyright 1999-2004 Jon Trulson under the ARTISTIC LICENSE. (See LICENSE).
 ***********************************************************************/

/**********************************************************************/
/* Unix/C specific porting and supporting code Copyright (C)1994-1996 */
/* by Jon Trulson <jon@radscan.com> under the same terms and          */
/* conditions of the original copyright by Jef Poskanzer and Craig    */
/* Leres.                                                             */
/*                                                                    */
/**********************************************************************/

#ifndef IBUF_H
#define IBUF_H

#define    IBUFMAX 1024

void iBufInit(void);
int iBufCount(void);
void iBufPut(char *thestr);
void iBufPutc(unsigned int thechar);
unsigned int iBufGetCh(void);
int DoMacro(int fkey);

#endif
