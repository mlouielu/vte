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
#include "vtetypes.hh"
#include "vteunistr.h"

namespace vte {

namespace base {  // FIXME ???

/* BidiRow contains the BiDi transformation of a single row. */
class BidiRow {
        friend class RingView;

public:
        BidiRow();
        ~BidiRow();

        // prevent accidents
        BidiRow(BidiRow& o) = delete;
        BidiRow(BidiRow const& o) = delete;
        BidiRow(BidiRow&& o) = delete;
        BidiRow& operator= (BidiRow& o) = delete;
        BidiRow& operator= (BidiRow const& o) = delete;
        BidiRow& operator= (BidiRow&& o) = delete;

        vte::grid::column_t log2vis(vte::grid::column_t col) const;
        vte::grid::column_t vis2log(vte::grid::column_t col) const;
        bool log_is_rtl(vte::grid::column_t col) const;
        bool vis_is_rtl(vte::grid::column_t col) const;
        bool base_is_rtl() const;
        bool has_foreign() const;

private:
        void set_width(vte::grid::column_t width);

        vte::grid::column_t m_width;
        vte::grid::column_t m_width_alloc;

        vte::grid::column_t *m_log2vis;
        vte::grid::column_t *m_vis2log;
        guint8 *m_vis_rtl;

        guint8 m_base_rtl: 1;
        guint8 m_has_foreign: 1;
};


/* RingView contains the BiDi transformations for all the rows of the viewport. */
class RingView {
public:
        RingView();
        ~RingView();

        // prevent accidents
        RingView(RingView& o) = delete;
        RingView(RingView const& o) = delete;
        RingView(RingView&& o) = delete;
        RingView& operator= (RingView& o) = delete;
        RingView& operator= (RingView const& o) = delete;
        RingView& operator= (RingView&& o) = delete;

        void set_ring(Ring *ring);
        void set_rows(vte::grid::row_t start, vte::grid::row_t len);
        void set_width(vte::grid::column_t width);

        void update();

        BidiRow const* get_row_map(vte::grid::row_t row) const;

private:
        Ring *m_ring;

        BidiRow **m_bidirows;

        vte::grid::row_t m_start;
        vte::grid::row_t m_len;
        vte::grid::column_t m_width;

        vte::grid::row_t m_height_alloc;

        BidiRow* get_row_map_writable(vte::grid::row_t row) const;

        void explicit_line(vte::grid::row_t row, bool rtl);
        vte::grid::row_t explicit_paragraph(vte::grid::row_t row, bool rtl);
        vte::grid::row_t find_paragraph(vte::grid::row_t row);
        vte::grid::row_t paragraph(vte::grid::row_t row);
};

}; /* namespace base */

}; /* namespace vte */

G_BEGIN_DECLS

gboolean vte_bidi_get_mirror_char (vteunistr unistr, gboolean mirror_box_drawing, vteunistr *unistr_mirrored);

G_END_DECLS
