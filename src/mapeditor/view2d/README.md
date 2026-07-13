# mapeditor/view2d — 2D grid editor

Grid, vertices (as shader point sprites), linedefs (line lists), and sector fills
(earcut triangles in cached VBOs), all drawn through `src/render`. CPU hit-testing
follows Eureka's approach: zoom-scaled vertex slack, perpendicular linedef distance,
point-in-sector. Tools: draw/insert, drag, snap-to-grid, sector draw, error checks.
