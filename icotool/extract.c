/* extract.c - Extract PNG images from icon and cursor files
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
#include <assert.h>		/* C89 */
#include <stdint.h>		/* POSIX/Gnulib */
#include <stdlib.h>		/* C89 */
#include <stdio.h>		/* C89 */
#if HAVE_PNG_H
# include <png.h>
#else
# if HAVE_LIBPNG_PNG_H
#  include <libpng/png.h>
# else
#  if HAVE_LIBPNG10_PNG_H
#   include <libpng10/png.h>
#  else
#   if HAVE_LIBPNG12_PNG_H
#    include <libpng12/png.h>
#   endif
#  endif
# endif
#endif
#include "gettext.h"		/* Gnulib */
#include "minmax.h"		/* Gnulib */
#define _(s) gettext(s)
#define N_(s) gettext_noop(s)
#include "xalloc.h"		/* Gnulib */
#include "common/strbuf.h"
#include "common/string-utils.h"
#include "common/io-utils.h"
#include "common/error.h"
#include "icotool.h"
#include "win32-endian.h"

#define ICO_PNG_MAGIC       0x474e5089

#define ROW_BYTES(bits) ((((bits) + 31) >> 5) << 2)

#define FALSE	0
#define TRUE	1

static uint32_t simple_vec(uint8_t *data, uint32_t ofs, uint8_t size);
static int read_png(uint8_t *image_data, uint32_t image_size, uint32_t *bit_count, uint32_t *width, uint32_t *height);

static bool
xfread(void *ptr, size_t size, FILE *stream)
{
	if (fread(ptr, size, 1, stream) < 1) {
		if (ferror(stream))
			warn_errno(_("cannot read file"));
		else
			warn(_("premature end"));
		return false;
	}
	return true;
}

struct png_mem_in
{
	uint8_t* ptr;
	uint32_t size;
};

static void png_read_mem (png_structp png, png_bytep data, png_size_t size)
{
	struct png_mem_in* io = (struct png_mem_in*)png_get_io_ptr (png);

	if (io->size < size)
		png_error (png, _("read error"));
	else
  	{
		memcpy (data, io->ptr, size);
		io->size -= size;
		io->ptr += size;
	}
}



int
extract_icons(FILE *in, char *inname, bool listmode, ExtractNameGen outfile_gen, ExtractFilter filter)
{
	Win32CursorIconFileDir dir;
	Win32CursorIconFileDirEntry *entries = NULL;
	uint32_t offset;
	uint32_t c, d;
	int completed = 0;
	int matched = 0;

	set_message_header(inname);

	if (!xfread(&dir, sizeof(Win32CursorIconFileDir), in))
		goto cleanup;
	fix_win32_cursor_icon_file_dir_endian(&dir);

	if (dir.reserved != 0) {
		warn(_("not an icon or cursor file (reserved non-zero)"));
		goto cleanup;
	}
	if (dir.type != 1 && dir.type != 2) {
		warn(_("not an icon or cursor file (wrong type)"));
		goto cleanup;
	}

	entries = xmalloc(dir.count * sizeof(Win32CursorIconFileDirEntry));
	for (c = 0; c < dir.count; c++) {
		if (!xfread(&entries[c], sizeof(Win32CursorIconFileDirEntry), in))
			goto cleanup;
		fix_win32_cursor_icon_file_dir_entry_endian(&entries[c]);
		if (entries[c].reserved != 0)
			warn(_("reserved is not zero"));
	}
	offset = sizeof(Win32CursorIconFileDir) + dir.count * sizeof(Win32CursorIconFileDirEntry);

	while(completed < dir.count) {
		uint32_t min_offset = UINT32_MAX;
		int previous = completed;
		
		for (c = 0; c < dir.count; c++) {
			if (entries[c].dib_offset == offset) {
				Win32BitmapInfoHeader bitmap;
				Win32RGBQuad *palette = NULL;
				uint32_t palette_count = 0;
				uint32_t image_size, mask_size;
				uint32_t width, height, bit_count;
				uint8_t *image_data = NULL, *mask_data = NULL;
				png_structp png_ptr = NULL;
				png_infop info_ptr = NULL;
				png_byte *row = NULL;
				char *outname = NULL;
				FILE *out = NULL;
				int do_next = FALSE;

				if (!xfread(&bitmap, sizeof(Win32BitmapInfoHeader), in))
					goto done;

				fix_win32_bitmap_info_header_endian(&bitmap);
				/* Vista icon: it's just a raw PNG */
				if (bitmap.size == ICO_PNG_MAGIC)
				{
					fseek(in, offset, SEEK_SET);
				
					image_size = entries[c].dib_size;
					image_data = xmalloc(image_size);
					if (!xfread(image_data, image_size, in))
						goto done;
					
					if (!read_png (image_data, image_size, &bit_count, &width, &height))
						goto done;
					
					completed++;
					
					if (!filter(completed, width, height, bitmap.bit_count, palette_count, dir.type == 1,
							(dir.type == 1 ? 0 : entries[c].hotspot_x),
								(dir.type == 1 ? 0 : entries[c].hotspot_y))) {
						do_next = TRUE;
						goto done;
					}
					matched++;

					if (listmode) {
						printf(_("--%s --index=%d --width=%d --height=%d --bit-depth=%d --palette-size=%d"),
								(dir.type == 1 ? "icon" : "cursor"), completed, width, height,
								bit_count, palette_count);
						if (dir.type == 2)
							printf(_(" --hotspot-x=%d --hotspot-y=%d"), entries[c].hotspot_x, entries[c].hotspot_y);
						printf("\n");
					} else {
						outname = inname;
						out = outfile_gen(&outname, width, height, bit_count, completed);
						restore_message_header();
						set_message_header(outname);

						if (out == NULL) {
							warn_errno(_("cannot create file"));
							goto done;
						}

						restore_message_header();
						set_message_header(inname);

						if (fwrite(image_data, image_size, 1, out) != 1) {
							warn_errno(_("cannot write to file"));
							goto cleanup;
						}
					}
					offset += image_size;
				}
				else
				{
					if (bitmap.size < sizeof(Win32BitmapInfoHeader)) {
						warn(_("bitmap header is too short"));
						goto done;
					}
					if (bitmap.compression != 0) {
						warn(_("compressed image data not supported"));
						goto done;
					}
					if (bitmap.x_pels_per_meter != 0)
						warn(_("x_pels_per_meter field in bitmap should be zero"));
					if (bitmap.y_pels_per_meter != 0)
						warn(_("y_pels_per_meter field in bitmap should be zero"));
					if (bitmap.clr_important != 0)
						warn(_("clr_important field in bitmap should be zero"));
					if (bitmap.planes != 1)
						warn(_("planes field in bitmap should be one"));
					if (bitmap.size != sizeof(Win32BitmapInfoHeader)) {
						uint32_t skip = bitmap.size - sizeof(Win32BitmapInfoHeader);
						warn(_("skipping %d bytes of extended bitmap header"), skip);
						fskip(in, skip);
					}
					offset += bitmap.size;

					if (bitmap.clr_used != 0 || bitmap.bit_count < 24) {
						palette_count = (bitmap.clr_used != 0 ? bitmap.clr_used : 1 << bitmap.bit_count);
						palette = xmalloc(sizeof(Win32RGBQuad) * palette_count);
						if (!xfread(palette, sizeof(Win32RGBQuad) * palette_count, in))
							goto done;
						offset += sizeof(Win32RGBQuad) * palette_count;
					}

					width = bitmap.width;
					height = abs(bitmap.height)/2;
				
					image_size = height * ROW_BYTES(width * bitmap.bit_count);
					mask_size = height * ROW_BYTES(width);

					if (entries[c].dib_size	!= bitmap.size + image_size + mask_size + palette_count * sizeof(Win32RGBQuad))
						warn(_("incorrect total size of bitmap (%d specified; %d real)"),
						    entries[c].dib_size,
						    bitmap.size + image_size + mask_size + palette_count * sizeof(Win32RGBQuad)
						);

					image_data = xmalloc(image_size);
					if (!xfread(image_data, image_size, in))
						goto done;

					mask_data = xmalloc(mask_size);
					if (!xfread(mask_data, mask_size, in))
						goto done;

					offset += image_size;
					offset += mask_size;
					completed++;

					if (!filter(completed, width, height, bitmap.bit_count, palette_count, dir.type == 1,
							(dir.type == 1 ? 0 : entries[c].hotspot_x),
								(dir.type == 1 ? 0 : entries[c].hotspot_y))) {
						do_next = TRUE;
						goto done;
					}
					matched++;

					if (!listmode) {
						png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL /*user_error_fn, user_warning_fn*/);
						if (!png_ptr) {
							warn(_("cannot initialize PNG library"));
							goto done;
						}
						info_ptr = png_create_info_struct(png_ptr);
						if (!info_ptr) {
							warn(_("cannot create PNG info structure - out of memory"));
							goto done;
						}

						outname = inname;
						out = outfile_gen(&outname, width, height, bitmap.bit_count, completed);
						restore_message_header();
						set_message_header(outname);

						if (out == NULL) {
							warn_errno(_("cannot create file"));
							goto done;
						}
						png_init_io(png_ptr, out);

						restore_message_header();
						set_message_header(inname);

						png_set_IHDR(png_ptr, info_ptr,	width, height, 8,
								PNG_COLOR_TYPE_RGB_ALPHA,
								PNG_INTERLACE_NONE,
								PNG_COMPRESSION_TYPE_DEFAULT,
								PNG_FILTER_TYPE_DEFAULT);
						png_write_info(png_ptr, info_ptr);
					}

					row = xmalloc(width * 4);

					for (d = 0; d < height; d++) {
						uint32_t x;
						uint32_t y = (bitmap.height < 0 ? d : height - d - 1);
						uint32_t imod = y * (image_size / height) * 8 / bitmap.bit_count;
						uint32_t mmod = y * (mask_size / height) * 8;

						for (x = 0; x < width; x++) {
							uint32_t color = simple_vec(image_data, x + imod, bitmap.bit_count);

							if (bitmap.bit_count <= 16) {
								if (color >= palette_count) {
									warn("color out of range in image data");
									goto done;
								}
								row[4*x+0] = palette[color].red;
								row[4*x+1] = palette[color].green;
								row[4*x+2] = palette[color].blue;
							} else {
								row[4*x+0] = (color >> 16) & 0xFF;
								row[4*x+1] = (color >>  8) & 0xFF;
								row[4*x+2] = (color >>  0) & 0xFF;
							}
							if (bitmap.bit_count == 32)
							    row[4*x+3] = (color >> 24) & 0xFF;
							else
							    row[4*x+3] = simple_vec(mask_data, x + mmod, 1) ? 0 : 0xFF;
						}

						if (!listmode)
							png_write_row(png_ptr, row);
					}

					if (listmode) {
						printf(_("--%s --index=%d --width=%d --height=%d --bit-depth=%d --palette-size=%d"),
								(dir.type == 1 ? "icon" : "cursor"), completed, width, height,
								bitmap.bit_count, palette_count);
						if (dir.type == 2)
							printf(_(" --hotspot-x=%d --hotspot-y=%d"), entries[c].hotspot_x, entries[c].hotspot_y);
						printf("\n");
					} else {
						png_write_end(png_ptr, info_ptr);
						png_destroy_write_struct(&png_ptr, &info_ptr);
						/*restore_message_header();*/
					}
				}
				
			do_next = TRUE;
			done:

				if (row != NULL) {
					free(row);
					row = NULL;
				}
				if (palette != NULL) {
					free(palette);
					palette = NULL;
				}
				if (image_data != NULL) {
					free(image_data);
					image_data = NULL;
				}
				if(mask_data != NULL) {
					free(mask_data);
					mask_data = NULL;
				}
				if (out != NULL) {
					fclose(out);
					out = NULL;
				}
				if (outname != NULL) {
					free(outname);
					outname = NULL;
				}
				if (do_next == TRUE) {
					continue;
				} else {
					goto cleanup;
				}
			} else {
				if (entries[c].dib_offset > offset)
					min_offset = MIN(min_offset, entries[c].dib_offset);
			}
		}

		if (previous == completed) {
			if (min_offset < offset) {
				warn(_("offset of bitmap header incorrect (too low)"));
				goto cleanup;
			}
			if ((min_offset-offset) == 0) {
				warn(_("invalid data at expected offset (unrecoverable)"));
				goto cleanup;
			}
			warn(_("skipping %u bytes of garbage at %u"), min_offset-offset, offset);
			fskip(in, min_offset - offset);
			offset = min_offset;
		}
	}

	restore_message_header();
	free(entries);
	return matched;

cleanup:

	restore_message_header();
	free(entries);
	return -1;
}

static uint32_t
simple_vec(uint8_t *data, uint32_t ofs, uint8_t size)
{
	switch (size) {
	case 1:
		return (data[ofs/8] >> (7 - ofs%8)) & 1;
	case 2:
		return (data[ofs/4] >> ((3 - ofs%4) << 1)) & 3;
	case 4:
		return (data[ofs/2] >> ((1 - ofs%2) << 2)) & 15;
	case 8:
		return data[ofs];
	case 16:
		return data[2*ofs] | data[2*ofs+1] << 8;
	case 24:
		return data[3*ofs] | data[3*ofs+1] << 8 | data[3*ofs+2] << 16;
	case 32:
		return data[4*ofs] | data[4*ofs+1] << 8 | data[4*ofs+2] << 16 | data[4*ofs+3] << 24;
	}

	return 0;
}

static int
read_png(uint8_t *image_data, uint32_t image_size, uint32_t *bit_count, uint32_t *width, uint32_t *height)
{
	png_structp png_ptr;
	png_infop info_ptr;
	struct png_mem_in png_in;
	png_byte ct;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL /*user_error_fn, user_warning_fn*/);
	if (png_ptr == NULL) {
		warn(_("cannot initialize PNG library"));
		return FALSE;
	}
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		warn(_("cannot create PNG info structure - out of memory"));
		return FALSE;
	}
	
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return FALSE;
	}

	png_in.ptr = image_data;
	png_in.size = image_size;
	
	png_set_read_fn(png_ptr, &png_in, &png_read_mem);
	png_read_info(png_ptr, info_ptr);
	png_read_update_info(png_ptr, info_ptr);
	
	*width = png_get_image_width(png_ptr, info_ptr);
	*height = png_get_image_height(png_ptr, info_ptr);
	ct = png_get_color_type(png_ptr, info_ptr);
	if (ct & PNG_COLOR_MASK_PALETTE)
	{
		*bit_count = png_get_bit_depth(png_ptr, info_ptr);
	}
	else
		*bit_count = png_get_bit_depth(png_ptr, info_ptr)
			* png_get_channels(png_ptr, info_ptr);

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	
	return TRUE;
}

