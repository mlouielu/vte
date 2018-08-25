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

#pragma once

#include <glib.h>

#include "ring.hh"
#include "vterowdata.hh"

// FIXME these names are ugly. Also, make these declarations private if possible.
struct _bidicellmap {
        int log2vis;
        int vis2log;
        guint8 vis_rtl: 1;
};

typedef struct _bidicellmap bidicellmap;

struct _bidirow {
        guint8 rtl: 1;
        bidicellmap *map;
};

typedef struct _bidirow bidirow;

namespace vte {

namespace base {  // FIXME ???

class RingView {
public:
        RingView();
        ~RingView();

        void set_ring(Ring *ring);
        void set_rows(long start, long len);
        void set_width(long width);

        void update();

        bidicellmap *get_row_map(long row);  // FIXME remove?

        long log2vis(long row, long col);
        long vis2log(long row, long col);
        bool log_is_rtl(long row, long col);
        bool vis_is_rtl(long row, long col);

private:
        Ring *m_ring;

        bidirow *m_bidirows;

        long m_start;
        long m_len;
        long m_width;

        long m_height_alloc;
        long m_width_alloc;

        void explicit_line(long row, bool rtl);
        long explicit_paragraph(long row, bool rtl);
        long find_paragraph(long row);
        long paragraph(long row);
};


}; /* namespace base */

}; /* namespace vte */

G_BEGIN_DECLS

gboolean vte_bidi_get_mirror_char (gunichar ch, gboolean mirror_box_drawing, gunichar *mirrored_ch);

G_END_DECLS
