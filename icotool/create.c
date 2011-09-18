/* create.c - Create icon and cursor files from PNG images
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
#include <stdint.h>		/* Gnulib/POSIX */
#include <stdio.h>		/* C89 */
#include <stdbool.h>		/* Gnulib/POSIX */
#include <stdlib.h>		/* C89 */
#include "gettext.h"		/* Gnulib */
#include "xalloc.h"		/* Gnulib */
#include "minmax.h"		/* Gnulib */
#define _(s) gettext(s)
#define N_(s) gettext_noop(s)
#include "common/io-utils.h"
#include "common/error.h"
#include "icotool.h"
#include "win32.h"
#include "win32-endian.h"

#define ROW_BYTES(bits) ((((bits) + 31) >> 5) << 2)

static void simple_setvec(uint8_t *data, uint32_t ofs, uint8_t size, uint32_t value);

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

bool
create_icon(int filec, char **filev, int raw_filec, char** raw_filev, CreateNameGen outfile_gen, bool icon_mode, int32_t hotspot_x, int32_t hotspot_y, int32_t alpha_threshold, int32_t bit_count)
{
	struct {
		FILE *in;
		png_structp png_ptr;
		png_infop info_ptr;
		uint32_t bit_count;
		uint32_t palette_count;
		uint32_t image_size;
		uint32_t mask_size;
		uint32_t width;
		uint32_t height;
		uint8_t *image_data;
		uint8_t **row_datas;
		Palette *palette;
		bool store_raw;
	} *img;

	Win32CursorIconFileDir dir;
	FILE *out;
	char *outname = NULL;
	uint32_t c, d, x;
	uint32_t dib_start;
	png_byte ct;
	int org_filec = filec;
	
	filec += raw_filec;

	img = xzalloc(filec * sizeof(*img));

	for (c = 0; c < filec; c++) {
		char header[8];
		uint32_t row_bytes;
		uint8_t transparency[256];
		uint16_t transparency_count;
		bool need_transparency;
		const char* real_filev = (c >= org_filec) ? raw_filev[c-org_filec] : filev[c];

		img[c].store_raw = (c >= org_filec);
		set_message_header(real_filev);

		img[c].in = fopen(real_filev, "rb");
    	if (img[c].in == NULL) {
        	warn_errno(_("cannot open file"));
			goto cleanup;
		}
    	if (!xfread(header, 8, img[c].in))
			goto cleanup;
    	if (png_sig_cmp(header, 0, 8)) {
	    
        	warn(_("not a png file"));
			goto cleanup;
		}

		img[c].png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL /*user_error_fn, user_warning_fn*/);
		if (img[c].png_ptr == NULL) {
			warn(_("cannot initialize PNG library"));
			goto cleanup;
		}
		img[c].info_ptr = png_create_info_struct(img[c].png_ptr);
		if (img[c].info_ptr == NULL) {
			warn(_("cannot create PNG info structure - out of memory"));
			goto cleanup;
		}

		png_init_io(img[c].png_ptr, img[c].in);
		png_set_sig_bytes(img[c].png_ptr, 8);
		png_set_strip_16(img[c].png_ptr);
		png_set_expand(img[c].png_ptr);
		png_set_gray_to_rgb(img[c].png_ptr);
		png_set_interlace_handling(img[c].png_ptr);
		png_set_filler(img[c].png_ptr, 0xFF, PNG_FILLER_AFTER);
		png_read_info(img[c].png_ptr, img[c].info_ptr);
		png_read_update_info(img[c].png_ptr, img[c].info_ptr);

		img[c].width = png_get_image_width(img[c].png_ptr, img[c].info_ptr);
		img[c].height = png_get_image_height(img[c].png_ptr, img[c].info_ptr);
		
		if (img[c].store_raw)
		{
			ct = png_get_color_type(img[c].png_ptr, img[c].info_ptr);
			if (ct & PNG_COLOR_MASK_PALETTE)
			{
				img[c].bit_count = png_get_bit_depth(img[c].png_ptr, img[c].info_ptr);
			}
			else
				img[c].bit_count = png_get_bit_depth(img[c].png_ptr, img[c].info_ptr)
					* png_get_channels(img[c].png_ptr, img[c].info_ptr);
			png_destroy_read_struct(&img[c].png_ptr, &img[c].info_ptr, NULL);
			
			fseek(img[c].in, 0, SEEK_END);
			img[c].image_size = ftell(img[c].in);
			fseek(img[c].in, 0, SEEK_SET);
			img[c].image_data = xmalloc(img[c].image_size);
		    	if (!xfread(img[c].image_data, img[c].image_size, img[c].in))
				goto cleanup;
		}
		else
		{
			row_bytes = png_get_rowbytes(img[c].png_ptr, img[c].info_ptr);
			img[c].row_datas = xmalloc(img[c].height * sizeof(png_bytep *));
			img[c].row_datas[0] = xmalloc(img[c].height * row_bytes);
			for (d = 1; d < img[c].height; d++)
				img[c].row_datas[d] = img[c].row_datas[d-1] + row_bytes;
			png_read_rows(img[c].png_ptr, img[c].row_datas, NULL, img[c].height);

			ct = png_get_color_type(img[c].png_ptr, img[c].info_ptr);
			img[c].palette = palette_new();


			/* Count number of necessary colors in palette and number of transparencies */
			memset(transparency, 0, 256);
			for (d = 0; d < img[c].height; d++) {
				png_bytep row = img[c].row_datas[d];
				for (x = 0; x < img[c].width; x++) {
					/* Set color of fully transparent pixels to black.
					    On Windows Mobile, and possibly on regular Windows OSes as well,
					    it seems that Windows does not completely ignore RGB-values of 
					    entirely transparent pixels as expected.
					 */
					if ((ct & PNG_COLOR_MASK_ALPHA) && (row[4*x+3] == 0))
					    row[4*x+0] = row[4*x+1] = row[4*x+2] = 0;
					if (palette_count(img[c].palette) <= (1 << 8))
					    palette_add(img[c].palette, row[4*x+0], row[4*x+1], row[4*x+2]);
					if (ct & PNG_COLOR_MASK_ALPHA)
					    transparency[row[4*x+3]] = 1;
				}
			}
			transparency_count = 0;
			for (d = 0; d < 256; d++)
			    transparency_count += transparency[d];

			/* If there are more than two steps of transparency, or if the
			 * two steps are NOT either entirely off (0) and entirely on (255),
			 * then we will lose transparency information if bit_count is not 32.
			 */
			need_transparency =
			    transparency_count > 2
			    ||
			    (transparency_count == 2 && (transparency[0] == 0 || transparency[255] == 0));

			/* Can we keep all colors in a palette? */
			if (need_transparency) {
				if (bit_count != -1) {
				    if (bit_count != 32)
					warn("decreasing bit depth will discard variable transparency", transparency_count);
				    /* Why 24 and not bit_count? Otherwise we might decrease below what's possible
					   * due to number of colors in image. The real decrease happens below. */
				    img[c].bit_count = 24;
				} else {
				    img[c].bit_count = 32;
				}
				img[c].palette_count = 0;
			}
			else if (palette_count(img[c].palette) <= 256) {
				for (d = 1; palette_count(img[c].palette) > 1 << d; d <<= 1);
				if (d == 2)	/* four colors (two bits) are not supported */
					d = 4;
				img[c].bit_count = d;
				img[c].palette_count = 1 << d;
			}
			else {
				img[c].bit_count = 24;
				img[c].palette_count = 0;
			}

			/* Does the user want to change number of bits per pixel? */
			if (bit_count != -1) {
				if (img[c].bit_count == bit_count) {
					/* No operation */
				} else if (img[c].bit_count < bit_count) {
					img[c].bit_count = bit_count;
					img[c].palette_count = (bit_count > 16 ? 0 : 1 << bit_count);
				} else {
					warn(_("cannot decrease bit depth from %d to %d, bit depth not changed"), img[c].bit_count, bit_count);
				}
			}
		
			img[c].image_size = img[c].height * ROW_BYTES(img[c].width * img[c].bit_count);
			img[c].mask_size = img[c].height * ROW_BYTES(img[c].width);
		}

		restore_message_header();
	}

	out = outfile_gen(&outname);
	restore_message_header();
	set_message_header(outname);
	if (out == NULL) {
		warn_errno(_("cannot create file"));
		goto cleanup;
	}

	dir.reserved = 0;
	dir.type = (icon_mode ? 1 : 2);
	dir.count = filec;
	fix_win32_cursor_icon_file_dir_endian(&dir);
	if (fwrite(&dir, sizeof(Win32CursorIconFileDir), 1, out) != 1) {
		warn_errno(_("cannot write to file"));
		goto cleanup;
	}

	dib_start = sizeof(Win32CursorIconFileDir) + filec * sizeof(Win32CursorIconFileDirEntry);
	for (c = 0; c < filec; c++) {
		Win32CursorIconFileDirEntry entry;

		/* If one of the dimensions is larger or equal to 256, both icon dimensions have
		   to be stored as 0 in the entry. */
		if ((img[c].width >= 256) || (img[c].height >= 256))
		{
			entry.width = 0;
			entry.height = 0;
		}
		else
		{
			entry.width = img[c].width;
			entry.height = img[c].height;
		}
		entry.reserved = 0;
		if (icon_mode) {
			entry.hotspot_x = 1;	            /* color planes for icons */
			entry.hotspot_y = img[c].bit_count; /* bit_count for icons */
		} else {
			entry.hotspot_x = hotspot_x;
			entry.hotspot_y = hotspot_y;
		}
		entry.dib_offset = dib_start;
		entry.color_count = (img[c].bit_count >= 8 ? 0 : 1 << img[c].bit_count);
		if (img[c].store_raw)
			entry.dib_size = img[c].image_size;
		else
			entry.dib_size = img[c].palette_count * sizeof(Win32RGBQuad)
					+ sizeof(Win32BitmapInfoHeader)
					+ img[c].image_size
					+ img[c].mask_size;

		dib_start += entry.dib_size;

		fix_win32_cursor_icon_file_dir_entry_endian(&entry);
		if (fwrite(&entry, sizeof(Win32CursorIconFileDirEntry), 1, out) != 1) {
			warn_errno(_("cannot write to file"));
			goto cleanup;
		}

	}

	for (c = 0; c < filec; c++) {
		if (img[c].store_raw)
		{
			if (fwrite(img[c].image_data, img[c].image_size, 1, out) != 1) {
				warn_errno(_("cannot write to file"));
				goto cleanup;
			}
		}
		else
		{
			Win32BitmapInfoHeader bitmap;

			bitmap.size = sizeof(Win32BitmapInfoHeader);
			bitmap.width = png_get_image_width(img[c].png_ptr, img[c].info_ptr);
			bitmap.height = png_get_image_height(img[c].png_ptr, img[c].info_ptr) * 2;
			bitmap.planes = 1;							// appears to be 1 always (XXX)
			bitmap.bit_count = img[c].bit_count;
			bitmap.compression = 0;
			bitmap.x_pels_per_meter = 0;				// should be 0 always
			bitmap.y_pels_per_meter = 0;				// should be 0 always
			bitmap.clr_important = 0;					// should be 0 always
			bitmap.clr_used = img[c].palette_count;
			bitmap.size_image = img[c].image_size;		// appears to be ok here (may be image_size+mask_size or 0, XXX)

			fix_win32_bitmap_info_header_endian(&bitmap);
			if (fwrite(&bitmap, sizeof(Win32BitmapInfoHeader), 1, out) != 1) {
				warn_errno("cannot write to file");
				goto cleanup;
			}

			if (img[c].bit_count <= 16) {
				Win32RGBQuad color;

				palette_assign_indices(img[c].palette);
				color.reserved = 0;
				while (palette_next(img[c].palette, &color.red, &color.green, &color.blue))
					fwrite(&color, sizeof(Win32RGBQuad), 1, out);

				/* Pad with empty colors. The reason we do this is because we
				 * specify bitmap.clr_used as a base of 2. The latter is probably
				 * not necessary according to the original specs, but many
				 * programs that read icons assume it. Especially gdk-pixbuf.
				 */
			    	memset(&color, 0, sizeof(Win32RGBQuad));
				for (d = palette_count(img[c].palette); d < 1 << img[c].bit_count; d++)
					fwrite(&color, sizeof(Win32RGBQuad), 1, out);
			}

			img[c].image_data = xzalloc(img[c].image_size);

			for (d = 0; d < img[c].height; d++) {
				png_bytep row = img[c].row_datas[img[c].height - d - 1];
				if (img[c].bit_count < 24) {
					uint32_t imod = d * (img[c].image_size/img[c].height) * 8 / img[c].bit_count;
					for (x = 0; x < img[c].width; x++) {
						uint32_t color;
						color = palette_lookup(img[c].palette, row[4*x+0], row[4*x+1], row[4*x+2]);
						simple_setvec(img[c].image_data, x+imod, img[c].bit_count, color);
					}
				} else if (img[c].bit_count == 24) {
					uint32_t irow = d * (img[c].image_size/img[c].height);
					for (x = 0; x < img[c].width; x++) {
						img[c].image_data[3*x+0 + irow] = row[4*x+2];
						img[c].image_data[3*x+1 + irow] = row[4*x+1];
						img[c].image_data[3*x+2 + irow] = row[4*x+0];
					}
				} else if (img[c].bit_count == 32) {
					uint32_t irow = d * (img[c].image_size/img[c].height);
					for (x = 0; x < img[c].width; x++) {
						img[c].image_data[4*x+0 + irow] = row[4*x+2];
						img[c].image_data[4*x+1 + irow] = row[4*x+1];
						img[c].image_data[4*x+2 + irow] = row[4*x+0];
						img[c].image_data[4*x+3 + irow] = row[4*x+3];
					}
				}
			}

			if (fwrite(img[c].image_data, img[c].image_size, 1, out) != 1) {
				warn_errno(_("cannot write to file"));
				goto cleanup;
			}

			for (d = 0; d < img[c].height; d++) {
				png_bytep row = img[c].row_datas[img[c].height - d - 1];

				for (x = 0; x < img[c].width; x += 8) {
					uint8_t mask = 0;
					mask |= (row[4*(x+0)+3] <= alpha_threshold ? 1 << 7 : 0);
					mask |= (row[4*(x+1)+3] <= alpha_threshold ? 1 << 6 : 0);
					mask |= (row[4*(x+2)+3] <= alpha_threshold ? 1 << 5 : 0);
					mask |= (row[4*(x+3)+3] <= alpha_threshold ? 1 << 4 : 0);
					mask |= (row[4*(x+4)+3] <= alpha_threshold ? 1 << 3 : 0);
					mask |= (row[4*(x+5)+3] <= alpha_threshold ? 1 << 2 : 0);
					mask |= (row[4*(x+6)+3] <= alpha_threshold ? 1 << 1 : 0);
					mask |= (row[4*(x+7)+3] <= alpha_threshold ? 1 << 0 : 0);
					fputc(mask, out);
				}

				fpad(out, 0, img[c].mask_size/img[c].height - x/8);
			}
		}

		free(img[c].image_data);
		if (img[c].palette) palette_free(img[c].palette);
		if (img[c].row_datas)
		{
			free(img[c].row_datas[0]);
			free(img[c].row_datas);
		}
		if (!img[c].store_raw)
		{
			png_read_end(img[c].png_ptr, img[c].info_ptr);
		}
		png_destroy_read_struct(&img[c].png_ptr, &img[c].info_ptr, NULL);
		fclose(img[c].in);
		memset(&img[c], 0, sizeof(*img));
	}

	free(outname);
	free(img);
	return true;

cleanup:

	restore_message_header();
	for (c = 0; c < filec; c++) {
		if (img[c].image_data != NULL)
			free(img[c].image_data);
		if (img[c].palette != NULL)
			palette_free(img[c].palette);
		if (img[c].row_datas != NULL && img[c].row_datas[0] != NULL) {
			free(img[c].row_datas[0]);
			free(img[c].row_datas);
		}
		if (img[c].png_ptr != NULL)
			png_destroy_read_struct(&img[c].png_ptr, &img[c].info_ptr, NULL);
		if (img[c].in != NULL)
			fclose(img[c].in);
	}
	if (outname != NULL)
		free(outname);

	free(img);
	return false;
}

static void
simple_setvec(uint8_t *data, uint32_t ofs, uint8_t size, uint32_t value)
{
	switch (size) {
	case 1:
		data[ofs/8] |= (value & 1) << (7 - ofs%8);
		break;
	case 2:
		data[ofs/4] |= (value & 3) << ((3 - ofs%4) << 1);
		break;
	case 4:
		data[ofs/2] |= (value & 15) << ((1 - ofs%2) << 2);
		break;
	case 8:
		data[ofs] = value;
		break;
	case 16:
		data[2*ofs] = value;
		data[2*ofs+1] = (value >> 8);
		break;
	case 24:
		data[3*ofs] = value;
		data[3*ofs+1] = (value >> 8);
		data[3*ofs+2] = (value >> 16);
		break;
	case 32:
		data[4*ofs] = value;
		data[4*ofs+1] = (value >> 8);
		data[4*ofs+2] = (value >> 16);
		data[4*ofs+3] = (value >> 24);
		break;
	}
}
