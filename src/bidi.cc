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
#include "vtedefines.hh"
#include "vteinternal.hh"



#ifdef WITH_FRIBIDI
FriBidiChar fribidi_chars[100000];
FriBidiCharType fribidi_chartypes[100000];
FriBidiBracketType fribidi_brackettypes[100000];
FriBidiLevel fribidi_levels[100000];
FriBidiStrIndex fribidi_map[100000];
#endif



using namespace vte::base;

RingView::RingView()
{
        m_ring = nullptr;

        m_start = m_len = m_width = 0;

        m_height_alloc = 32;
        m_width_alloc = 128;

        m_bidirows = (bidirow *) g_malloc (sizeof (bidirow) * m_height_alloc);
        for (int i = 0; i < m_height_alloc; i++) {
                m_bidirows[i].map = (bidicellmap *) g_malloc (sizeof (bidicellmap) * m_width_alloc);
        }
}

RingView::~RingView()
{
        for (int i = 0; i < m_height_alloc; i++)
                g_free (m_bidirows[i].map);
        g_free (m_bidirows);
}

void RingView::set_ring(Ring *r)
{
        m_ring = r;
}

void RingView::set_width(long w)
{
        if (G_UNLIKELY (w > m_width_alloc)) {
                while (w > m_width_alloc) {
                        m_width_alloc *= 2;
                }
                for (int i = 0; i < m_height_alloc; i++) {
                        m_bidirows[i].map = (bidicellmap *) g_realloc (m_bidirows[i].map, sizeof (bidicellmap) * m_width_alloc);
                }
        }

        m_width = w;
}

void RingView::set_rows(long s, long l)
{
        if (G_UNLIKELY (l > m_height_alloc)) {
                int i = m_height_alloc;
                while (l > m_height_alloc) {
                        m_height_alloc *= 2;
                }
                m_bidirows = (bidirow *) g_realloc (m_bidirows, sizeof (bidirow) * m_height_alloc);
                for (; i < m_height_alloc; i++) {
                        m_bidirows[i].map = (bidicellmap *) g_malloc (sizeof (bidicellmap) * m_width_alloc);
                }
        }

        m_start = s;
        m_len = l;
}

void RingView::update()
{
        long i = m_start;
        while (i < m_start + m_len) {
                i = paragraph (i);
        }
}

bidicellmap *RingView::get_row_map(long row)  // FIXME remove this?
{
        g_assert_cmpint (row, >=, m_start);
        g_assert_cmpint (row, <, m_start + m_len);
        return m_bidirows[row - m_start].map;
}

/* Converts from logical to visual column. Offscreen columns are mirrored
 * for RTL lines, e.g. (assuming 80 columns) -1 <=> 80, -2 <=> 81 etc. */
long RingView::log2vis(long row, long col)
{
        g_assert_cmpint (row, >=, m_start);
        g_assert_cmpint (row, <, m_start + m_len);
        bidirow *brow = &m_bidirows[row - m_start];
        if (G_LIKELY (col >= 0 && col < m_width)) {
                return brow->map[col].log2vis;
        } else {
                return brow->rtl ? m_width - 1 - col : col;
        }
}

/* Converts from visual to logical column. Offscreen columns are mirrored
 * for RTL lines, e.g. (assuming 80 columns) -1 <=> 80, -2 <=> 81 etc. */
long RingView::vis2log(long row, long col)
{
        g_assert_cmpint (row, >=, m_start);
        g_assert_cmpint (row, <, m_start + m_len);
        bidirow *brow = &m_bidirows[row - m_start];
        if (G_LIKELY (col >= 0 && col < m_width)) {
                return brow->map[col].vis2log;
        } else {
                return brow->rtl ? m_width - 1 - col : col;
        }
}

/* Whether the cell at the given visual position has RTL directionality.
 * For offscreen columns the line's direction is returned. */
bool RingView::vis_is_rtl(long row, long col)
{
        g_assert_cmpint (row, >=, m_start);
        g_assert_cmpint (row, <, m_start + m_len);
        bidirow *brow = &m_bidirows[row - m_start];
        if (G_LIKELY (col >= 0 && col < m_width)) {
                return brow->map[col].vis_rtl;
        } else {
                return brow->rtl;
        }
}

/* Whether the cell at the given logical position has RTL directionality.
 * For offscreen columns the line's direction is returned. */
bool RingView::log_is_rtl(long row, long col)
{
        g_assert_cmpint (row, >=, m_start);
        g_assert_cmpint (row, <, m_start + m_len);
        bidirow *brow = &m_bidirows[row - m_start];
        if (G_LIKELY (col >= 0 && col < m_width)) {
                col = brow->map[col].log2vis;
                return brow->map[col].vis_rtl;
        } else {
                return brow->rtl;
        }
}

/* Set up the mapping according to explicit mode for a given line. */
void RingView::explicit_line(long row, bool rtl)
{
        int i;
        bidicellmap *map;

        if (G_UNLIKELY (row < m_start || row >= m_start + m_len))
                return;

        m_bidirows[row - m_start].rtl = rtl;
        map = m_bidirows[row - m_start].map;

        if (G_UNLIKELY (rtl)) {
                for (i = 0; i < m_width; i++) {
                        map[i].log2vis = map[i].vis2log = m_width - 1 - i;
                        map[i].vis_rtl = TRUE;
                }
        } else {
                for (i = 0; i < m_width; i++) {
                        map[i].log2vis = map[i].vis2log = i;
                        map[i].vis_rtl = FALSE;
                }
        }
}

/* Set up the mapping according to explicit mode, for all the lines
 * of a paragraph beginning at the given line.
 * Returns the row number after the paragraph or viewport (whichever ends first). */
long RingView::explicit_paragraph(long row, bool rtl)
{
        const VteRowData *row_data;

        while (row < m_start + m_len) {
                explicit_line(row, rtl);

                row_data = m_ring->index(row++);
                if (row_data == nullptr || !row_data->attr.soft_wrapped)
                        break;
        }
        return row;
}

/* Figure out the mapping for the paragraph starting at the given row.
 * Returns the row number after the paragraph or viewport (whichever ends first). */
long RingView::paragraph(long row)
{
        const VteRowData *row_data;

#ifdef WITH_FRIBIDI
        const VteCell *cell;
        bool rtl;
        bool autodir;
        FriBidiParType pbase_dir;
        FriBidiLevel level;
        bidicellmap *map;
#endif /* WITH_FRIBIDI */

        row_data = m_ring->index(row);
        if (row_data == nullptr) {
                return explicit_paragraph(row, FALSE);
        }

#ifndef WITH_FRIBIDI
        return explicit_paragraph(row, !!(row_data->attr.bidi_flags & VTE_BIDI_RTL));
#else

        if (!(row_data->attr.bidi_flags & VTE_BIDI_IMPLICIT)) {
                return explicit_paragraph(row, !!(row_data->attr.bidi_flags & VTE_BIDI_RTL));
        }

        rtl = !!(row_data->attr.bidi_flags & VTE_BIDI_RTL);
        autodir = !!(row_data->attr.bidi_flags & VTE_BIDI_AUTO);

        int lines[VTE_BIDI_PARAGRAPH_LENGTH_MAX + 1];
        lines[0] = 0;
        int line = 0;
        int c = 0;
        int row_orig = row;
        int j = 0;
        int k, l, v;
        unsigned int col;

        /* Extract the paragraph's contents, omitting unused and fragment cells. */
        while (row < m_start + m_len) {
                row_data = m_ring->index(row++);
                if (row_data == nullptr)
                        break;

                if (line == VTE_BIDI_PARAGRAPH_LENGTH_MAX) {
                        /* Overlong paragraph, bail out. */
                        return explicit_paragraph (row_orig, rtl);
                }

                /* A row_data might be longer, in case rewrapping is disabled and the window was narrowed.
                 * Truncate the logical data before applying BiDi. */
                // FIXME what the heck to do if this truncation cuts a TAB or CJK in half???
                for (j = 0; j < m_width && j < row_data->len; j++) {
                        cell = _vte_row_data_get (row_data, j);
                        if (cell->attr.fragment())
                                continue;

                        // FIXME is it okay to run the BiDi algorithm without the combining accents?
                        // If we need to preserve them then we need to double check whether
                        // fribidi_reorder_line() requires a FRIBIDI_FLAG_REORDER_NSM or not.
                        fribidi_chars[c++] = _vte_unistr_get_base(cell->c);
                }

                lines[++line] = c;

                if (!row_data->attr.soft_wrapped)
                        break;
        }

        if (lines == 0) {
                // huh?
                return explicit_paragraph (row_orig, rtl);
        }

        /* Run the BiDi algorithm on the paragraph to get the embedding levels. */

        // FIXME are the WLTR / WRTL paragraph directions what I think they are?
        pbase_dir = autodir ? (rtl ? FRIBIDI_PAR_WRTL : FRIBIDI_PAR_WLTR)
                            : (rtl ? FRIBIDI_PAR_RTL  : FRIBIDI_PAR_LTR );

        fribidi_get_bidi_types (fribidi_chars, c, fribidi_chartypes);
        fribidi_get_bracket_types (fribidi_chars, c, fribidi_chartypes, fribidi_brackettypes);
        level = fribidi_get_par_embedding_levels_ex (fribidi_chartypes, fribidi_brackettypes, c, &pbase_dir, fribidi_levels);

        if (level == 0) {
                /* error */
                return explicit_paragraph (row_orig, rtl);
        }

        /* For convenience, from now on this variable contains the resolved (i.e. possibly autodetected) value. */
        g_assert_cmpint (pbase_dir, !=, FRIBIDI_PAR_ON);
        rtl = (pbase_dir == FRIBIDI_PAR_RTL || pbase_dir == FRIBIDI_PAR_WRTL);

        if (level == 1 || (rtl && level == 2)) {
                /* Fast shortcut for LTR-only and RTL-only paragraphs. */
                return explicit_paragraph (row_orig, rtl);
        }

        /* Reshuffle line by line. */
        row = row_orig;
        line = 0;
        while (row < m_start + m_len) {
                if (G_UNLIKELY (row < m_start)) {
                        row++;
                        line++;
                        continue;
                }

                m_bidirows[row - m_start].rtl = rtl;
                map = m_bidirows[row - m_start].map;

                row_data = m_ring->index(row++);
                if (row_data == nullptr)
                        break;

                /* fribidi_reorder_line() conveniently reorders arbitrary numbers we pass as the map.
                 * Use the logical position to save us from headaches when encountering fragments. */
                k = lines[line];
                for (j = 0; j < m_width && j < row_data->len; j++) {
                        cell = _vte_row_data_get (row_data, j);
                        if (cell->attr.fragment())
                                continue;

                        fribidi_map[k++] = j;
                }

                g_assert_cmpint (k, ==, lines[line + 1]);

                // FIXME is it okay to run the BiDi algorithm without the combining accents?
                // If we need to preserve them then we need to double check whether
                // fribidi_reorder_line() requires a FRIBIDI_FLAG_REORDER_NSM or not.
                level = fribidi_reorder_line (FRIBIDI_FLAGS_DEFAULT,
                                              fribidi_chartypes,
                                              lines[line + 1] - lines[line],
                                              lines[line],
                                              pbase_dir,
                                              fribidi_levels,
                                              NULL,
                                              fribidi_map);

                if (level == 0) {
                        /* error, what should we do? */
                        explicit_line (row, rtl);
                        goto next_line;
                }

                if (level == 1 || (rtl && level == 2)) {
                        /* Fast shortcut for LTR-only and RTL-only lines. */
                        explicit_line (row, rtl);
                        goto next_line;
                }

                /* Copy to our realm. Proceed in visual order.*/
                v = 0;
                if (rtl) {
                        /* Unused cell on the left for RTL paragraphs */
                        int unused = MAX(m_width - row_data->len, 0);
                        for (; v < unused; v++) {
                                map[v].vis2log = m_width - 1 - v;
                                map[v].vis_rtl = TRUE;
                        }
                }
                for (j = lines[line]; j < lines[line + 1]; j++) {
                        /* Inflate fribidi's result by inserting fragments. */
                        l = fribidi_map[j];
                        cell = _vte_row_data_get (row_data, l);
                        g_assert (!cell->attr.fragment());
                        g_assert (cell->attr.columns() > 0);
                        if (fribidi_levels[l] % 2 == 0) {
                                /* LTR character directionality. */
                                for (col = 0; col < cell->attr.columns(); col++) {
                                        map[v].vis2log = l;
                                        map[v].vis_rtl = FALSE;
                                        v++;
                                        l++;
                                }
                        } else {
                                /* RTL character directionality. Map fragments in reverse order. */
                                for (col = 0; col < cell->attr.columns(); col++) {
                                        map[v + col].vis2log = l + cell->attr.columns() - 1 - col;
                                        map[v + col].vis_rtl = TRUE;
                                }
                                v += cell->attr.columns();
                                l += cell->attr.columns();
                        }
                }
                if (!rtl) {
                        /* Unused cell on the right for LTR paragraphs */
                        g_assert_cmpint (v, ==, MIN (row_data->len, m_width));
                        for (; v < m_width; v++) {
                                map[v].vis2log = v;
                                map[v].vis_rtl = FALSE;
                        }
                }
                g_assert_cmpint (v, ==, m_width);

                /* From vis2log create the log2vis mapping too.
                 * In debug mode assert that we have a bijective mapping. */
                if (_vte_debug_on (VTE_DEBUG_BIDI)) {
                        for (l = 0; l < m_width; l++) {
                                map[l].log2vis = -1;
                        }
                }

                for (v = 0; v < m_width; v++) {
                        map[map[v].vis2log].log2vis = v;
                }

                if (_vte_debug_on (VTE_DEBUG_BIDI)) {
                        for (l = 0; l < m_width; l++) {
                                g_assert_cmpint (map[l].log2vis, !=, -1);
                        }
                }

next_line:
                line++;

                if (!row_data->attr.soft_wrapped)
                        break;
        }

        return row;

#endif /* !WITH_FRIBIDI */
}
