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

struct _bidicellmap {
        int log2vis;
        int vis2log;
        guint8 vis_rtl: 1;
};

typedef struct _bidicellmap bidicellmap;

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

        bidicellmap *get_row_map(long row);

private:
        Ring *m_ring;

        bidicellmap **m_bidimaps;

        long m_start;
        long m_len;
        long m_width;

        long m_height_alloc;
        long m_width_alloc;

        void explicit_line(long row, gboolean rtl);
        long explicit_paragraph(long row, gboolean rtl);
        long paragraph(long row);
};


}; /* namespace base */

}; /* namespace vte */

G_BEGIN_DECLS

G_END_DECLS
