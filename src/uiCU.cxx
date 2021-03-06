//
// Author: Jon Trulson <jon@radscan.com>
// Copyright (c) 1994-2018 Jon Trulson
//
// The MIT License
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use, copy,
// modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
// BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// Curses specific UI function implementations

#include "c_defs.h"

#include <string>

#include "global.h"
#include "conqdef.h"
#include "cb.h"
#include "context.h"
#include "conf.h"
#include "color.h"
#include "hud.h"

#include "cd2lb.h"

#include "conqutil.h"
#include "cprintf.h"

void uiMoveCursor(int lin, int col)
{
    cdmove(lin, col);
}

void uiPutMsg(const std::string& buf, int lin)
{
    // FIXME - someday, correct the cd2lb stuff to not make 1 == 0

    // FIXME - maybe won't need this curses specific stuff if conqoper
    // draws the 3 msg lines based only on hud data...
    cdclrl( lin, 1 );
    if (buf.size())
        cdputs( buf.c_str(), lin, 1 );

    hudSetPrompt(lin, "", NoColor, buf, NoColor);
}
