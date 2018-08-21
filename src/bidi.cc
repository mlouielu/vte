/*
 * Copyright © 2018 Egmont Koblinger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>

#ifdef WITH_FRIBIDI
#include <fribidi.h>
#endif

#include "bidi.hh"
#include "debug.h"
#include "vteinternal.hh"

#ifdef WITH_FRIBIDI

FriBidiChar str[1000];
FriBidiStrIndex L_to_V[1000];
FriBidiStrIndex V_to_L[1000];

static void bidi_shuffle_explicit (int width, gboolean rtl)
{
        int i;

        if (rtl) {
                for (i = 0; i < width; i++) {
                        L_to_V[i] = V_to_L[i] = width - 1 - i;
                }		
        } else {
                for (i = 0; i < width; i++) {
                        L_to_V[i] = V_to_L[i] = i;
                }		
        }
}

void bidi_shuffle (const VteRowData *rowdata, int width)
{
        int i;
        FriBidiParType pbase_dir;

        if (rowdata == NULL) {  // FIXME make sure it doesn't happen
                bidi_shuffle_explicit (width, FALSE);
                return;
        }

        if (!(rowdata->attr.bidi_flags & VTE_BIDI_IMPLICIT)) {
                bidi_shuffle_explicit (width, rowdata->attr.bidi_flags & VTE_BIDI_RTL);
                return;
        }

        for (i = 0; i < rowdata->len && i < width; i++) {
                if (rowdata->cells[i].c == 0) break;
                // FIXME is it okay to run the BiDi algorithm without the combining accents?
                str[i] = _vte_unistr_get_base(rowdata->cells[i].c);
        }

        pbase_dir = (rowdata->attr.bidi_flags & VTE_BIDI_AUTO) 
                    ? FRIBIDI_PAR_ON
                    : (rowdata->attr.bidi_flags & VTE_BIDI_RTL) ? FRIBIDI_PAR_RTL : FRIBIDI_PAR_LTR;

        fribidi_log2vis (str, i, &pbase_dir, NULL, L_to_V, V_to_L, NULL);

        if (pbase_dir == FRIBIDI_PAR_ON) {
                pbase_dir = (rowdata->attr.bidi_flags & VTE_BIDI_RTL) ? FRIBIDI_PAR_RTL : FRIBIDI_PAR_LTR;
        }

        if (pbase_dir == FRIBIDI_PAR_RTL || pbase_dir == FRIBIDI_PAR_WRTL) {
                if (i < width) {
                        /* shift to the right */
                        int shift = width - i;
                        for (i--; i >= 0; i--) {
                                L_to_V[i] += shift;
                                V_to_L[i + shift] = V_to_L[i];
                        }
                        for (i = 0; i < shift; i++) {
                                L_to_V[width - 1 - i] = i;
                                V_to_L[i] = width - 1 - i;
                        }
                }
        } else {
                for (; i < width; i++) {
                        L_to_V[i] = V_to_L[i] = i;
                }
        }
}

#else /* WITH_FRIBIDI */

void bidi_shuffle (const VteRowData *rowdata, int width) {
        if (rowdata == NULL) {  // FIXME make sure it doesn't happen
                bidi_shuffle_explicit (width, FALSE);
        } else {
                bidi_shuffle_explicit (width, rowdata->attr.bidi_flags & VTE_BIDI_RTL);
        }
}

#endif /* WITH_FRIBIDI */

int log2vis (int log) {
        return L_to_V[log];
}

int vis2log (int vis) {
        return V_to_L[vis];
}
