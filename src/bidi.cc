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


using namespace vte::base;

BidiRow::BidiRow()
{
        m_width = 0;

        /* These will be initialized / allocated on demand, when some RTL is encountered. */
        m_width_alloc = 0;
        m_log2vis = nullptr;
        m_vis2log = nullptr;
        m_vis_rtl = nullptr;
}

BidiRow::~BidiRow()
{
        g_free (m_log2vis);
        g_free (m_vis2log);
        g_free (m_vis_rtl);
}

void BidiRow::set_width(vte::grid::column_t width)
{
        if (G_UNLIKELY (width > m_width_alloc)) {
                if (m_width_alloc == 0) {
                        m_width_alloc = 128;
                }
                while (width > m_width_alloc) {
                        m_width_alloc *= 2;
                }
                m_log2vis = (vte::grid::column_t *) g_realloc (m_log2vis, sizeof (vte::grid::column_t) * m_width_alloc);
                m_vis2log = (vte::grid::column_t *) g_realloc (m_vis2log, sizeof (vte::grid::column_t) * m_width_alloc);
                m_vis_rtl = (guint8 *) g_realloc (m_vis_rtl, sizeof (guint8) * m_width_alloc);
        }

        m_width = width;
}

/* Converts from logical to visual column. Offscreen columns are mirrored
 * for RTL lines, e.g. (assuming 80 columns) -1 <=> 80, -2 <=> 81 etc. */
vte::grid::column_t BidiRow::log2vis(vte::grid::column_t col) const
{
        if (col >= 0 && col < m_width) {
                return m_log2vis[col];
        } else {
                return m_base_rtl ? m_width - 1 - col : col;
        }
}

/* Converts from visual to logical column. Offscreen columns are mirrored
 * for RTL lines, e.g. (assuming 80 columns) -1 <=> 80, -2 <=> 81 etc. */
vte::grid::column_t BidiRow::vis2log(vte::grid::column_t col) const
{
        if (col >= 0 && col < m_width) {
                return m_vis2log[col];
        } else {
                return m_base_rtl ? m_width - 1 - col : col;
        }
}

/* Whether the cell at the given visual position has RTL directionality.
 * For offscreen columns the line's base direction is returned. */
bool BidiRow::vis_is_rtl(vte::grid::column_t col) const
{
        if (col >= 0 && col < m_width) {
                return m_vis_rtl[col];
        } else {
                return m_base_rtl;
        }
}

/* Whether the cell at the given logical position has RTL directionality.
 * For offscreen columns the line's base direction is returned. */
bool BidiRow::log_is_rtl(vte::grid::column_t col) const
{
        if (col >= 0 && col < m_width) {
                col = m_log2vis[col];
                return m_vis_rtl[col];
        } else {
                return m_base_rtl;
        }
}

/* Whether the line's base direction is RTL. */
bool BidiRow::base_is_rtl() const
{
        return m_base_rtl;
}

/* Whether the paragraph contains a foreign directionality character.
 * This is used in the cursor, showing the character's directionality. */
bool BidiRow::has_foreign() const
{
        return m_has_foreign;
}


RingView::RingView()
{
        m_ring = nullptr;

        m_start = m_len = m_width = 0;
        m_height_alloc = 32;

        m_bidirows = (BidiRow **) g_malloc (sizeof (BidiRow *) * m_height_alloc);
        for (int i = 0; i < m_height_alloc; i++) {
                m_bidirows[i] = new BidiRow();
        }
}

RingView::~RingView()
{
        for (int i = 0; i < m_height_alloc; i++) {
                delete m_bidirows[i];
        }
        g_free (m_bidirows);
}

void RingView::set_ring(Ring *ring)
{
        m_ring = ring;
}

void RingView::set_width(vte::grid::column_t width)
{
        m_width = width;
}

void RingView::set_rows(vte::grid::row_t start, vte::grid::row_t len)
{
        if (G_UNLIKELY (len > m_height_alloc)) {
                int i = m_height_alloc;
                while (len > m_height_alloc) {
                        m_height_alloc *= 2;
                }
                m_bidirows = (BidiRow **) g_realloc (m_bidirows, sizeof (BidiRow *) * m_height_alloc);
                for (; i < m_height_alloc; i++) {
                        m_bidirows[i] = new BidiRow();
                }
        }

        m_start = start;
        m_len = len;
}

void RingView::update()
{
        vte::grid::row_t i = m_start;
        const VteRowData *row_data = m_ring->index(m_start);

        if (row_data->attr.bidi_flags & VTE_BIDI_IMPLICIT) {
                i = find_paragraph(m_start);
                if (i == -1) {
                        i = explicit_paragraph(m_start, row_data->attr.bidi_flags & VTE_BIDI_RTL);
                }
        }
        while (i < m_start + m_len) {
                i = paragraph(i);
        }
}

BidiRow const* RingView::get_row_map(vte::grid::row_t row) const
{
        g_assert_cmpint (row, >=, m_start);
        g_assert_cmpint (row, <, m_start + m_len);
        return m_bidirows[row - m_start];
}

BidiRow* RingView::get_row_map_writable(vte::grid::row_t row) const
{
        g_assert_cmpint (row, >=, m_start);
        g_assert_cmpint (row, <, m_start + m_len);
        return m_bidirows[row - m_start];
}

/* Set up the mapping according to explicit mode for a given line. */
void RingView::explicit_line(vte::grid::row_t row, bool rtl)
{
        int i;

        if (G_UNLIKELY (row < m_start || row >= m_start + m_len))
                return;

        BidiRow *bidirow = get_row_map_writable(row);
        bidirow->m_base_rtl = rtl;
        bidirow->m_has_foreign = false;

        if (G_UNLIKELY (rtl)) {
                bidirow->set_width(m_width);
                for (i = 0; i < m_width; i++) {
                        bidirow->m_log2vis[i] = bidirow->m_vis2log[i] = m_width - 1 - i;
                        bidirow->m_vis_rtl[i] = true;
                }
        } else {
                /* Shortcut: bidirow->m_width == 0 might denote a fully LTR line,
                 * m_width_alloc might even be 0 along with log2vis and friends being nullptr in this case. */
                bidirow->set_width(0);
        }
}

/* Set up the mapping according to explicit mode, for all the lines
 * of a paragraph beginning at the given line.
 * Returns the row number after the paragraph or viewport (whichever ends first). */
vte::grid::row_t RingView::explicit_paragraph(vte::grid::row_t row, bool rtl)
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

/* For the given row, find the first row of its paragraph.
 * Returns -1 if have to walk backwards too much. */
/* FIXME this could be much cheaper, we don't need to read the actual rows (text_stream),
 * we only need the soft_wrapped flag which is stored in row_stream. Needs method in ring. */
vte::grid::row_t RingView::find_paragraph(vte::grid::row_t row)
{
        vte::grid::row_t row_stop = row - VTE_BIDI_PARAGRAPH_LENGTH_MAX;
        const VteRowData *row_data;

        while (row-- > row_stop) {
                if (row < _vte_ring_delta(m_ring))
                        return row + 1;
                row_data = m_ring->index(row);
                if (row_data == nullptr || !row_data->attr.soft_wrapped)
                        return row + 1;
        }
        return -1;
}

/* Figure out the mapping for the paragraph starting at the given row.
 * Returns the row number after the paragraph or viewport (whichever ends first). */
vte::grid::row_t RingView::paragraph(vte::grid::row_t row)
{
        const VteRowData *row_data = m_ring->index(row);
        if (row_data == nullptr) {
                return explicit_paragraph(row, false);
        }

#ifndef WITH_FRIBIDI
        return explicit_paragraph(row, row_data->attr.bidi_flags & VTE_BIDI_RTL);
#else
        const VteCell *cell;
        bool rtl;
        bool autodir;
        FriBidiParType pbase_dir;
        FriBidiLevel level;
        BidiRow *bidirow;

        if (!(row_data->attr.bidi_flags & VTE_BIDI_IMPLICIT)) {
                return explicit_paragraph(row, row_data->attr.bidi_flags & VTE_BIDI_RTL);
        }

        rtl = row_data->attr.bidi_flags & VTE_BIDI_RTL;
        autodir = row_data->attr.bidi_flags & VTE_BIDI_AUTO;

        int lines[VTE_BIDI_PARAGRAPH_LENGTH_MAX + 1];
        lines[0] = 0;
        int line = 0;
        int count = 0;
        int row_orig = row;
        int tl, tv;  /* terminal logical and visual */
        int fl, fv;  /* fribidi logical and visual */
        unsigned int col;

        /* The buffer size assumes that combining chars are omitted. It's an overkill, but convenient solution. */
        // FIXME VLA is a gcc extension, use g_newa() instead
        FriBidiChar fribidi_chars[VTE_BIDI_PARAGRAPH_LENGTH_MAX * m_width];

        /* Extract the paragraph's contents, omitting unused and fragment cells. */
        while (row < _vte_ring_next(m_ring)) {
                row_data = m_ring->index(row);
                if (row_data == nullptr)
                        break;

                if (line == VTE_BIDI_PARAGRAPH_LENGTH_MAX) {
                        /* Overlong paragraph, bail out. */
                        return explicit_paragraph (row_orig, rtl);
                }

                /* A row_data might be longer, in case rewrapping is disabled and the window was narrowed.
                 * Truncate the logical data before applying BiDi. */
                // FIXME what the heck to do if this truncation cuts a TAB or CJK in half???
                for (tl = 0; tl < m_width && tl < row_data->len; tl++) {
                        cell = _vte_row_data_get (row_data, tl);
                        if (cell->attr.fragment())
                                continue;

                        // FIXME is it okay to run the BiDi algorithm without the combining accents?
                        // If we need to preserve them then we need to double check whether
                        // fribidi_reorder_line() requires a FRIBIDI_FLAG_REORDER_NSM or not.
                        fribidi_chars[count++] = _vte_unistr_get_base(cell->c);
                }

                lines[++line] = count;
                row++;

                if (!row_data->attr.soft_wrapped)
                        break;
        }

        if (lines == 0) {
                // huh?
                return explicit_paragraph (row_orig, rtl);
        }

        /* Run the BiDi algorithm on the paragraph to get the embedding levels. */
        // FIXME VLA is a gcc extension, use g_newa() instead
        FriBidiCharType fribidi_chartypes[count];
        FriBidiBracketType fribidi_brackettypes[count];
        FriBidiLevel fribidi_levels[count];
        FriBidiStrIndex fribidi_map[count];

        pbase_dir = autodir ? (rtl ? FRIBIDI_PAR_WRTL : FRIBIDI_PAR_WLTR)
                            : (rtl ? FRIBIDI_PAR_RTL  : FRIBIDI_PAR_LTR );

        fribidi_get_bidi_types (fribidi_chars, count, fribidi_chartypes);
        fribidi_get_bracket_types (fribidi_chars, count, fribidi_chartypes, fribidi_brackettypes);
        level = fribidi_get_par_embedding_levels_ex (fribidi_chartypes, fribidi_brackettypes, count, &pbase_dir, fribidi_levels);

        if (level == 0) {
                /* error */
                return explicit_paragraph (row_orig, rtl);
        }

        g_assert_cmpint (pbase_dir, !=, FRIBIDI_PAR_ON);
        /* For convenience, from now on this variable contains the resolved (i.e. possibly autodetected) value. */
        rtl = (pbase_dir == FRIBIDI_PAR_RTL || pbase_dir == FRIBIDI_PAR_WRTL);

        if ((!rtl && level == 1) || (rtl && level == 2)) {
                /* Fast shortcut for LTR-only and RTL-only paragraphs. */
                return explicit_paragraph (row_orig, rtl);
        }

        /* Silly FriBidi API of fribidi_reorder_line()... It reorders whatever values we give to it,
         * and it would be super convenient for us to pass the logical columns of the terminal.
         * However, we can't figure out the embedding levels then. So this feature is quite useless.
         * Set up the trivial mapping for fribidi, and to the mapping manually in fribidi_to_terminal below. */
        for (fl = 0; fl < count; fl++) {
                fribidi_map[fl] = fl;
        }

        /* Reshuffle line by line. */
        row = row_orig;
        line = 0;
        if (G_UNLIKELY (row < m_start)) {
                line = m_start - row;
                row = m_start;
        }
        while (row < m_start + m_len) {
                bidirow = get_row_map_writable(row);
                bidirow->m_base_rtl = rtl;
                bidirow->m_has_foreign = true;
                bidirow->set_width(m_width);

                row_data = m_ring->index(row);
                if (row_data == nullptr)
                        break;

                /* Map from FriBidi's to terminal's logical position, see the detailed explanation above. */
                // FIXME VLA is a gcc extension, use g_newa() instead
                FriBidiStrIndex fribidi_to_terminal[lines[line + 1] - lines[line]];
                fl = 0;
                for (tl = 0; tl < m_width && tl < row_data->len; tl++) {
                        cell = _vte_row_data_get (row_data, tl);
                        if (cell->attr.fragment())
                                continue;

                        fribidi_to_terminal[fl] = tl;
                        fl++;
                }

                g_assert_cmpint (fl, ==, lines[line + 1] - lines[line]);

                // FIXME is it okay to run the BiDi algorithm without the combining accents?
                // If we need to preserve them then we need to have a bigger fribidi_chars array,
                // and double check whether fribidi_reorder_line() requires a FRIBIDI_FLAG_REORDER_NSM or not.
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
                        bidirow->m_has_foreign = true;
                        goto next_line;
                }

                if ((!rtl && level == 1) || (rtl && level == 2)) {
                        /* Fast shortcut for LTR-only and RTL-only lines. */
                        explicit_line (row, rtl);
                        bidirow->m_has_foreign = true;
                        goto next_line;
                }

                /* Copy to our realm. Proceed in visual order.*/
                tv = 0;
                if (rtl) {
                        /* Unused cells on the left for RTL paragraphs */
                        int unused = MAX(m_width - row_data->len, 0);
                        for (; tv < unused; tv++) {
                                bidirow->m_vis2log[tv] = m_width - 1 - tv;
                                bidirow->m_vis_rtl[tv] = true;
                        }
                }
                for (fv = lines[line]; fv < lines[line + 1]; fv++) {
                        /* Inflate fribidi's result by inserting fragments. */
                        fl = fribidi_map[fv];
                        tl = fribidi_to_terminal[fl - lines[line]];
                        cell = _vte_row_data_get (row_data, tl);
                        g_assert (!cell->attr.fragment());
                        g_assert (cell->attr.columns() > 0);
                        if (FRIBIDI_LEVEL_IS_RTL(fribidi_levels[fl])) {
                                /* RTL character directionality. Map fragments in reverse order. */
                                for (col = 0; col < cell->attr.columns(); col++) {
                                        bidirow->m_vis2log[tv + col] = tl + cell->attr.columns() - 1 - col;
                                        bidirow->m_vis_rtl[tv + col] = true;
                                }
                                tv += cell->attr.columns();
                                tl += cell->attr.columns();
                        } else {
                                /* LTR character directionality. */
                                for (col = 0; col < cell->attr.columns(); col++) {
                                        bidirow->m_vis2log[tv] = tl;
                                        bidirow->m_vis_rtl[tv] = false;
                                        tv++;
                                        tl++;
                                }
                        }
                }
                if (!rtl) {
                        /* Unused cells on the right for LTR paragraphs */
                        g_assert_cmpint (tv, ==, MIN (row_data->len, m_width));
                        for (; tv < m_width; tv++) {
                                bidirow->m_vis2log[tv] = tv;
                                bidirow->m_vis_rtl[tv] = false;
                        }
                }
                g_assert_cmpint (tv, ==, m_width);

                /* From vis2log create the log2vis mapping too.
                 * In debug mode assert that we have a bijective mapping. */
                if (_vte_debug_on (VTE_DEBUG_BIDI)) {
                        for (tl = 0; tl < m_width; tl++) {
                                bidirow->m_log2vis[tl] = -1;
                        }
                }

                for (tv = 0; tv < m_width; tv++) {
                        bidirow->m_log2vis[bidirow->m_vis2log[tv]] = tv;
                }

                if (_vte_debug_on (VTE_DEBUG_BIDI)) {
                        for (tl = 0; tl < m_width; tl++) {
                                g_assert_cmpint (bidirow->m_log2vis[tl], !=, -1);
                        }
                }

next_line:
                line++;
                row++;

                if (!row_data->attr.soft_wrapped)
                        break;
        }

        return row;
#endif /* !WITH_FRIBIDI */
}


/* Find the mirrored counterpart of a codepoint, just like
 * fribidi_get_mirror_char() or g_unichar_get_mirror_char() does.
 * Two additions:
 * - works with vteunistr, that is, preserves combining accents;
 * - optionally mirrors box drawing characters.
 */
gboolean vte_bidi_get_mirror_char (vteunistr unistr, gboolean mirror_box_drawing, vteunistr *out)
{
        static const unsigned char mirrored_2500[0x80] = {
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x10, 0x11, 0x12, 0x13,
                0x0c, 0x0d, 0x0e, 0x0f, 0x18, 0x19, 0x1a, 0x1b, 0x14, 0x15, 0x16, 0x17, 0x24, 0x25, 0x26, 0x27,
                0x28, 0x29, 0x2a, 0x2b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x2c, 0x2e, 0x2d, 0x2f,
                0x30, 0x32, 0x31, 0x33, 0x34, 0x36, 0x35, 0x37, 0x38, 0x3a, 0x39, 0x3b, 0x3c, 0x3e, 0x3d, 0x3f,
                0x40, 0x41, 0x42, 0x44, 0x43, 0x46, 0x45, 0x47, 0x48, 0x4a, 0x49, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
                0x50, 0x51, 0x55, 0x56, 0x57, 0x52, 0x53, 0x54, 0x5b, 0x5c, 0x5d, 0x58, 0x59, 0x5a, 0x61, 0x62,
                0x63, 0x5e, 0x5f, 0x60, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6e, 0x6d, 0x70,
                0x6f, 0x72, 0x71, 0x73, 0x76, 0x75, 0x74, 0x77, 0x7a, 0x79, 0x78, 0x7b, 0x7e, 0x7d, 0x7c, 0x7f };

        gunichar base_ch = _vte_unistr_get_base (unistr);
        gunichar base_ch_mirrored = base_ch;

        if (G_UNLIKELY (base_ch >= 0x2500 && base_ch < 0x2580)) {
                if (G_UNLIKELY (mirror_box_drawing))
                        base_ch_mirrored = 0x2500 + mirrored_2500[base_ch - 0x2500];
        } else {
#ifdef WITH_FRIBIDI
                /* Prefer the FriBidi variant as that's more likely to be in sync with the rest of our BiDi stuff. */
                fribidi_get_mirror_char (base_ch, &base_ch_mirrored);
#else
                /* Fall back to glib, so that we still get mirrored characters in explicit RTL mode without BiDi support. */
                g_unichar_get_mirror_char (base_ch, &base_ch_mirrored);
#endif
        }

        vteunistr unistr_mirrored = _vte_unistr_replace_base (unistr, base_ch_mirrored);

        if (out)
                *out = unistr_mirrored;
        return unistr_mirrored == unistr;
}
