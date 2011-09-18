/* extract.c - Extract icon and cursor files from PE and NE files
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
#include "gettext.h"			/* Gnulib */
#define _(s) gettext(s)
#define N_(s) gettext_noop(s)
#include "xalloc.h"			/* Gnulib */
#include "common/error.h"
#include "common/intutil.h"
#include "win32.h"
#include "win32-endian.h"
#include "fileread.h"
#include "wrestool.h"

static void *extract_group_icon_cursor_resource(WinLibrary *, WinResource *, char *, int *, bool);
static void *extract_bitmap_resource(WinLibrary *, WinResource *, int *);

void
extract_resources_callback (WinLibrary *fi, WinResource *wr,
                            WinResource *type_wr, WinResource *name_wr,
                            WinResource *lang_wr)
{
	int size;
	bool free_it;
	void *memory;
	char *outname;
	FILE *out;

	memory = extract_resource(fi, wr, &size, &free_it, type_wr->id, (lang_wr == NULL ? NULL : lang_wr->id), arg_raw);
	free_it = false;
	if (memory == NULL) {
		/* extract resource has printed error */
		return;
	}

	/* determine where to extract to */
	outname = get_destination_name(fi, type_wr->id, name_wr->id, (lang_wr == NULL ? NULL : lang_wr->id));
	if (outname == NULL) {
		out = stdout;
	} else {
		out = fopen(outname, "wb");
		if (out == NULL) {
			warn_errno("%s", outname);
			goto cleanup;
		}
	}

	/* write the actual data */
	fwrite(memory, size, 1, out);
	
	cleanup:
	if (free_it)
		free(memory);
	if (out != NULL && out != stdout)
		fclose(out);
}

/* extract_resource:
 *   Extract a resource, returning pointer to data.
 */
void *
extract_resource (WinLibrary *fi, WinResource *wr, int *size,
                  bool *free_it, char *type, char *lang, bool raw)
{
	char *str;
	int32_t intval;
	
	/* just return pointer to data if raw */
	if (raw) {
		*free_it = false;
		/* get_resource_entry will print possible error */
		return get_resource_entry(fi, wr, size);
	}

	/* find out how to extract */
	str = type;
	if (str != NULL && parse_int32(STRIP_RES_ID_FORMAT(str), &intval)) {
		if (intval == (int) RT_BITMAP) {
			*free_it = true;
			return extract_bitmap_resource(fi, wr, size);
		}
		if (intval == (int) RT_GROUP_ICON) {
			*free_it = true;
			return extract_group_icon_cursor_resource(fi, wr, lang, size, true);
		}
		if (intval == (int) RT_GROUP_CURSOR) {
			*free_it = true;
			return extract_group_icon_cursor_resource(fi, wr, lang, size, false);
		}
	}

	warn(_("%s: don't know how to extract resource, try `--raw'"), fi->name);
	return NULL;
}

/* extract_group_icon_resource:
 *   Create a complete RT_GROUP_ICON resource, that can be written to
 *   an `.ico' file without modifications. Returns an allocated
 *   memory block that should be freed with free() once used.
 *
 *   `root' is the offset in file that specifies the resource.
 *   `base' is the offset that string pointers are calculated from.
 *   `ressize' should point to an integer variable where the size of
 *   the returned memory block will be placed.
 *   `is_icon' indicates whether resource to be extracted is icon
 *   or cursor group.
 */
static void *
extract_group_icon_cursor_resource(WinLibrary *fi, WinResource *wr, char *lang,
                                   int *ressize, bool is_icon)
{
	Win32CursorIconDir *icondir;
	Win32CursorIconFileDir *fileicondir;
	char *memory;
	int c, size, offset, skipped;

	/* get resource data and size */
	icondir = (Win32CursorIconDir *) get_resource_entry(fi, wr, &size);
	if (icondir == NULL) {
		/* get_resource_entry will print error */
		return NULL;
	}

	/* calculate total size of output file */
	RETURN_IF_BAD_POINTER(NULL, icondir->count);
	skipped = 0;
	for (c = 0 ; c < icondir->count ; c++) {
		int level;
	    	int iconsize;
		char name[14];
		WinResource *fwr;

		RETURN_IF_BAD_POINTER(NULL, icondir->entries[c]);
		/*printf("%d. bytes_in_res=%d width=%d height=%d planes=%d bit_count=%d\n", c,
			icondir->entries[c].bytes_in_res,
			(is_icon ? icondir->entries[c].res_info.icon.width : icondir->entries[c].res_info.cursor.width),
			(is_icon ? icondir->entries[c].res_info.icon.height : icondir->entries[c].res_info.cursor.height),
			icondir->entries[c].plane_count,
			icondir->entries[c].bit_count);*/

		/* find the corresponding icon resource */
		snprintf(name, sizeof(name)/sizeof(char), "-%d", icondir->entries[c].res_id);
		fwr = find_resource(fi, (is_icon ? "-3" : "-1"), name, lang, &level);
		if (fwr == NULL) {
			warn(_("%s: could not find `%s' in `%s' resource."),
			 	fi->name, &name[1], (is_icon ? "group_icon" : "group_cursor"));
			return NULL;
		}

		if (get_resource_entry(fi, fwr, &iconsize) != NULL) {
		    if (iconsize == 0) {
			warn(_("%s: icon resource `%s' is empty, skipping"), fi->name, name);
			skipped++;
			continue;
		    }
		    if (iconsize != icondir->entries[c].bytes_in_res) {
			warn(_("%s: mismatch of size in icon resource `%s' and group (%d vs %d)"), fi->name, name, iconsize, icondir->entries[c].bytes_in_res);
		    }
		    size += iconsize; /* size += icondir->entries[c].bytes_in_res; */

		    /* cursor resources have two additional WORDs that contain
		     * hotspot info */
		    if (!is_icon)
			size -= sizeof(uint16_t)*2;
		}
	}
	offset = sizeof(Win32CursorIconFileDir) + (icondir->count-skipped) * sizeof(Win32CursorIconFileDirEntry);
	size += offset;
	*ressize = size;

	/* allocate that much memory */
	memory = xmalloc(size);
	fileicondir = (Win32CursorIconFileDir *) memory;

	/* transfer Win32CursorIconDir structure members */
	fileicondir->reserved = icondir->reserved;
	fileicondir->type = icondir->type;
	fileicondir->count = icondir->count - skipped;

	/* transfer each cursor/icon: Win32CursorIconDirEntry and data */
	skipped = 0;
	for (c = 0 ; c < icondir->count ; c++) {
		int level;
		char name[14];
		WinResource *fwr;
		char *data;
	
		/* find the corresponding icon resource */
		snprintf(name, sizeof(name)/sizeof(char), "-%d", icondir->entries[c].res_id);
		fwr = find_resource(fi, (is_icon ? "-3" : "-1"), name, lang, &level);
		if (fwr == NULL) {
			warn(_("%s: could not find `%s' in `%s' resource."),
			 	fi->name, &name[1], (is_icon ? "group_icon" : "group_cursor"));
			return NULL;
		}

		/* get data and size of that resource */
		data = get_resource_entry(fi, fwr, &size);
		if (data == NULL) {
			/* get_resource_entry has printed error */
			return NULL;
		}
    	    	if (size == 0) {
		    skipped++;
		    continue;
		}

		/* copy ICONDIRENTRY (not including last dwImageOffset) */
		memcpy(&fileicondir->entries[c-skipped], &icondir->entries[c],
			sizeof(Win32CursorIconFileDirEntry)-sizeof(uint32_t));

		/* special treatment for cursors */
		if (!is_icon) {
			fileicondir->entries[c-skipped].width = icondir->entries[c].res_info.cursor.width;
			fileicondir->entries[c-skipped].height = icondir->entries[c].res_info.cursor.height / 2;
			fileicondir->entries[c-skipped].color_count = 0;
			fileicondir->entries[c-skipped].reserved = 0;
		}

		/* set image offset and increase it */
		fileicondir->entries[c-skipped].dib_offset = offset;

		/* transfer resource into file memory */
		if (is_icon) {
			memcpy(&memory[offset], data, icondir->entries[c].bytes_in_res);
		} else {
			fileicondir->entries[c-skipped].hotspot_x = ((uint16_t *) data)[0];
			fileicondir->entries[c-skipped].hotspot_y = ((uint16_t *) data)[1];
			memcpy(&memory[offset], data+sizeof(uint16_t)*2,
				   icondir->entries[c].bytes_in_res-sizeof(uint16_t)*2);
			offset -= sizeof(uint16_t)*2;
		}

		/* increase the offset pointer */
		offset += icondir->entries[c].bytes_in_res;
	}

	return (void *) memory;
}

/* extract_bitmap_resource:
 *   Create a complete RT_BITMAP resource, that can be written to
 *   an `.bmp' file without modifications. Returns an allocated
 *   memory block that should be freed with free() once used.
 *
 *   `ressize' should point to an integer variable where the size of
 *   the returned memory block will be placed.
 */
static void *
extract_bitmap_resource(WinLibrary *fi, WinResource *wr, int *ressize)
{
    Win32BitmapInfoHeader info;
    uint8_t *result;
    uint8_t *resentry;
    uint32_t offbits;
    int size;

    resentry=(uint8_t *)(get_resource_entry(fi,wr,&size));

    /* Bitmap file consists of:
     * 1) File header (14 bytes)
     * 2) Bitmap header (40 bytes)
     * 3) Colormap (size depends on a few things)
     * 4) Pixels data
     *
     * parts 2-4 are present in the resource data, we need just
     * to add a file header, which contains file size and offset
     * from file beginning to pixels data.
     */

    /* Get the bitmap info */
    memcpy(&info,resentry,sizeof(info));
    fix_win32_bitmap_info_header_endian(&info);

    /* offbits - offset from file start to the beginning
     *           of the first pixel data */
    offbits = info.size+14;

    /* In 24-bit bitmaps there's no colormap
     * The size of an entry in colormap is 4
     */
    if (info.bit_count!=24) {

        /* 0 value of clr_used means that all possible color
        * entries are used */
       if (info.clr_used==0) {
           switch (info.bit_count) {
               case 1:    /* Monochrome bitmap */
                   offbits += 8;
                   break;
               case 4:    /* 16 colors bitmap */
                   offbits += 64;
                   break;
               case 8:    /* 256 colors bitmap */
                  offbits += 1024;
           }
       } else {
           offbits += 4 * info.clr_used;
       }
    }

    /* The file will consist of the resource data and
     * 14 bytes long file header */
    *ressize = 14+size;
    result = (uint8_t *)xmalloc(*ressize);

    /* Filling the file header with data */
    result[0] = 'B';   /* Magic char #1 */
    result[1] = 'M';   /* Magic char #2 */
    result[2] = (*ressize & 0x000000ff);      /* file size, little-endian */
    result[3] = (*ressize & 0x0000ff00)>>8;
    result[4] = (*ressize & 0x00ff0000)>>16;
    result[5] = (*ressize & 0xff000000)>>24;
    result[6] = 0; /* Reserved */
    result[7] = 0;
    result[8] = 0;
    result[9] = 0;
    result[10] = (offbits & 0x000000ff);  /* offset to pixels, little-endian */
    result[11] = (offbits & 0x0000ff00)>>8;
    result[12] = (offbits & 0x00ff0000)>>16;
    result[13] = (offbits & 0xff000000)>>24;

    /* The rest of the file is the resource entry */
    memcpy(result+14,resentry,size);

    return result;
}
