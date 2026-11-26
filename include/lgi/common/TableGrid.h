#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

// Shared table grid model and CSS 2.1 column width solver.
//
// Engine independent: the grid stores opaque node pointers, and the per-cell
// content min/max measurements plus width preferences (Fixed/Pct) are filled by
// the calling engine's measure pass. The engine then gets resolved column widths
// from this module.
//
// Users:
//   lgi/trunk/src/common/Text/Html2.cpp            (HTML layout engine, nodes are LTag*)
//   lgi/trunk/src/common/Widgets/richText2         (TableBlock, nodes are Block*)
//
// Header only so it does not need to be added to platform makefiles/project files.

namespace TableGrid
{
    struct MinMax
    {
        int Min = 0, Max = 0;
        bool Has = false;
    };

    template <typename T>
    struct Cell
    {
        T *Node = nullptr;         // The engine node this cell represents
        int Row = 0, Col = 0, CS = 1, RS = 1;
        MinMax m;                  // Content min/max width, filled by the engine
        int Pct = -1;              // Preferred percentage column width, -1 if not set
        bool Fixed = false;        // Exact pixel column width preference

        Cell() {}
        Cell(T *node) : Node(node) {}
    };

    template <typename T>
    struct Grid
    {
        struct Row
        {
            T *Node = nullptr;
            T *Group = nullptr;
            std::vector<int> Cells;    // Indices into Grid::Cells

            Row() {}
            Row(T *node, T *group) : Node(node), Group(group) {}
        };

        std::vector<Row> Rows;
        std::vector<Cell<T>> Cells;    // Built in row order, so indices are stable
        int NCols = 0;
    };

    struct Columns
    {
        std::vector<int> Min, Max;
        std::vector<int> Pct;          // -1 if not set
        std::vector<bool> Fixed;
        int Spacing = 0;               // Border-spacing in px, 0 when collapsed
    };

    inline int Distribute(std::vector<int> &v, int from, int count, int extra)
    {
        if (extra <= 0 || count <= 0)
            return 0;
        int total = 0;
        for (int i = 0; i < count; i++)
            total += std::max(v[from + i], 1);
        int given = 0;
        for (int i = 0; i < count; i++)
        {
            int add = i == count - 1 ? extra - given
                : (int)((int64_t)extra * std::max(v[from + i], 1) / total);
            v[from + i] += add;
            given += add;
        }
        return given;
    }

    /// Assigns Col and NCols from the row structure. Cell Row/CS/RS must be set by
    /// the engine before calling, and Grid::Cells must be in row order.
    template <typename T>
    void PlaceCells(Grid<T> &g)
    {
        std::vector<std::vector<bool>> used(g.Rows.size() + 1);
        for (size_t r = 0; r < g.Rows.size(); r++)
        {
            int col = 0;
            for (int idx : g.Rows[r].Cells)
            {
                auto &tc = g.Cells[idx];
                tc.Row = (int)r;
                tc.RS = std::min(tc.RS, (int)(g.Rows.size() - r));

                while (col < (int)used[r].size() && used[r][col])
                    col++;
                tc.Col = col;
                for (int rr = 0; rr < tc.RS; rr++)
                {
                    auto &u = used[r + rr];
                    if ((int)u.size() < col + tc.CS)
                        u.resize(col + tc.CS, false);
                    for (int cc = 0; cc < tc.CS; cc++)
                        u[col + cc] = true;
                }
                col += tc.CS;
                g.NCols = std::max(g.NCols, col);
            }
        }
    }

    /// Folds per-cell measurements (already in Cell::m, Cell::Pct, Cell::Fixed) into
    /// column metrics, distributing spanned cells across the columns they cover.
    template <typename T>
    void ColumnMetrics(const Grid<T> &g, Columns &cols)
    {
        int n = g.NCols;
        cols.Min.assign(n, 0);
        cols.Max.assign(n, 0);
        cols.Pct.assign(n, -1);
        cols.Fixed.assign(n, false);

        for (const auto &tc : g.Cells)
            if (tc.CS == 1)
            {
                cols.Min[tc.Col] = std::max(cols.Min[tc.Col], tc.m.Min);
                cols.Max[tc.Col] = std::max(cols.Max[tc.Col], tc.m.Max);
                if (tc.Fixed)
                    cols.Fixed[tc.Col] = true;
                cols.Pct[tc.Col] = std::max(cols.Pct[tc.Col], tc.Pct);
            }

        for (const auto &tc : g.Cells)
        {
            if (tc.CS < 2)
                continue;
            int sMin = 0, sMax = 0;
            for (int c = 0; c < tc.CS; c++)
            {
                sMin += cols.Min[tc.Col + c];
                sMax += cols.Max[tc.Col + c];
            }
            int gaps = cols.Spacing * (tc.CS - 1);
            Distribute(cols.Min, tc.Col, tc.CS, tc.m.Min - gaps - sMin);
            Distribute(cols.Max, tc.Col, tc.CS, tc.m.Max - gaps - sMax);
        }

        for (int c = 0; c < n; c++)
            cols.Max[c] = std::max(cols.Max[c], cols.Min[c]);
    }

    /// Sum of preferred widths, honouring percentage columns that must fit their
    /// preferred width at that percentage.
    inline int PreferredWidth(const Columns &cols)
    {
        int width = 0;
        for (auto mx : cols.Max)
            width += mx;
        for (size_t c = 0; c < cols.Max.size(); c++)
        {
            if (cols.Pct[c] > 0)
                width = std::max(width,
                    (int)(((int64_t)cols.Max[c] * 100 + cols.Pct[c] - 1) / cols.Pct[c]));
        }
        return width;
    }

    /// CSS 2.1 column width distribution for a total content width of 'T'.
    /// Writes the resolved column widths into 'w'.
    inline void SolveColumns(const Columns &cols, int T, std::vector<int> &w)
    {
        int n = (int)cols.Min.size();
        std::vector<int> pref(n);
        int sumMin = 0, sumPref = 0;
        for (int i = 0; i < n; i++)
        {
            pref[i] = cols.Pct[i] >= 0 ? std::max(cols.Min[i], T * cols.Pct[i] / 100)
                : cols.Max[i];
            sumMin += cols.Min[i];
            sumPref += pref[i];
        }

        w = cols.Min;
        if (T <= sumMin)
            return;

        if (T >= sumPref)
        {
            w = pref;
            int extra = T - sumPref;
            if (extra > 0)
            {
                std::vector<int> idx;
                for (int i = 0; i < n; i++)
                    if (cols.Pct[i] < 0 && !cols.Fixed[i])
                        idx.push_back(i);
                if (idx.empty())
                    for (int i = 0; i < n; i++)
                        idx.push_back(i);
                int total = 0;
                for (int i : idx)
                    total += std::max(cols.Max[i], 1);
                int given = 0;
                for (size_t k = 0; k < idx.size(); k++)
                {
                    int i = idx[k];
                    int add = k + 1 == idx.size() ? extra - given
                        : (int)((int64_t)extra * std::max(cols.Max[i], 1) / total);
                    w[i] += add;
                    given += add;
                }
            }
            return;
        }

        int avail = T - sumMin;
        int range = sumPref - sumMin;
        int given = 0, lastGrow = -1;
        for (int i = 0; i < n; i++)
            if (pref[i] > cols.Min[i])
                lastGrow = i;
        for (int i = 0; i < n; i++)
        {
            int grow = pref[i] - cols.Min[i];
            if (grow <= 0)
                continue;
            int add = i == lastGrow ? avail - given
                : (int)((int64_t)avail * grow / range);
            w[i] = cols.Min[i] + add;
            given += add;
        }
    }
}
