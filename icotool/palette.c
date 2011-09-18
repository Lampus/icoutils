/* palette.c - Palette hash management for icon/cursor creation
 *
 * Copyright (C) 1998 Oskar Liljeblad
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdint.h>		/* Gnulib/POSIX */
#include <stdlib.h>		/* C89 */
#include "xalloc.h"		/* Gnulib */
#include "icotool.h"
#include "common/hmap.h"
#include "common/common.h"

typedef struct {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	uint32_t index;
} PaletteColor;

struct _Palette {
	HMap *map;
	HMapIterator it;
	bool it_init;
};

static uint32_t
color_hash(PaletteColor *color)
{
	return (color->red << 16) | (color->green << 8) | color->blue;
}

static int32_t
color_compare(PaletteColor *c1, PaletteColor *c2)
{
	if (c1->red != c2->red)
		return c1->red - c2->red;
	if (c1->green != c2->green)
		return c1->green - c2->green;
	if (c1->blue != c2->blue)
		return c1->blue - c2->blue;
	return 0;
}

Palette *
palette_new(void)
{
	Palette *palette = xmalloc(sizeof(Palette));
	palette->map = hmap_new();
	palette->it_init = false;
	hmap_set_hash_fn(palette->map, (hash_fn_t) color_hash);
	hmap_set_compare_fn(palette->map, (comparison_fn_t) color_compare);
	return palette;
}

void
palette_free(Palette *palette)
{
	hmap_foreach_value(palette->map, free);
	hmap_free(palette->map);
	free(palette);
}

void
palette_add(Palette *palette, uint8_t r, uint8_t g, uint8_t b)
{
	PaletteColor color = { r, g, b, 0 };

	if (!hmap_contains_key(palette->map, &color)) {
		PaletteColor *new_color = xmalloc(sizeof(PaletteColor));
		new_color->red = r;
		new_color->green = g;
		new_color->blue = b;
		new_color->index = 0;
		hmap_put(palette->map, new_color, new_color);
	}
}

bool
palette_next(Palette *palette, uint8_t *r, uint8_t *g, uint8_t *b)
{
	if (!palette->it_init) {
		hmap_iterator(palette->map, &palette->it);
		palette->it_init = true;
	}
	if (palette->it.has_next(&palette->it)) {
		PaletteColor *color = palette->it.next(&palette->it);
		*r = color->red;
		*g = color->green;
		*b = color->blue;
		return true;
	}
	palette->it_init = false;
	return false;
}

void
palette_assign_indices(Palette *palette)
{
	HMapIterator it;
	uint32_t c;

	hmap_iterator(palette->map, &it);
	for (c = 0; it.has_next(&it); c++) {
		PaletteColor *color = it.next(&it);
		color->index = c;
	}
}

uint32_t
palette_lookup(Palette *palette, uint8_t r, uint8_t g, uint8_t b)
{
	PaletteColor color = { r, g, b, 0 };
	PaletteColor *real_color = hmap_get(palette->map, &color);
	return (real_color != NULL ? real_color->index : -1);
}

uint32_t
palette_count(Palette *palette)
{
	return hmap_size(palette->map);
}
