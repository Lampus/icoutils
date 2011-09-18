/* main.c - Main routine from icotool
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
#include <unistd.h>		/* POSIX */
#include <errno.h>		/* C89 */
#include <stdio.h>		/* C89 */
#include <getopt.h>		/* Gnulib/GNU Libc */
#include <string.h>		/* C89 */
#include <stdlib.h>		/* C89 */
#include "gettext.h"
#include "configmake.h"
#define _(s) gettext(s)
#define N_(s) gettext_noop(s)
#include "progname.h"		/* Gnulib */
#include "version-etc.h"	/* Gnulib */
#include "xalloc.h"		/* Gnulib */
#include "common/error.h"
#include "common/strbuf.h"
#include "common/string-utils.h"
#include "common/intutil.h"
#include "common/io-utils.h"
#include "icotool.h"

#define PROGRAM "icotool"

static int32_t image_index = -1;
static int32_t width = -1;
static int32_t height = -1;
static int32_t bitdepth = -1;
/*static uint32_t minbitdepth = 1;*/
static int32_t palettesize = -1;
static int32_t hotspot_x = 0;
static int32_t hotspot_y = 0;
static bool hotspot_x_set = false;
static bool hotspot_y_set = false;
static int32_t alpha_threshold = 127;
static bool icon_only = false;	
static bool cursor_only = false;
static char *output = NULL;

const char version_etc_copyright[] = "Copyright (C) 1998 Oskar Liljeblad";

enum {
    VERSION_OPT	= 1000,
    HELP_OPT,
    ICON_OPT,
    CURSOR_OPT,
};

static char *short_opts = "xlco:i:w:h:p:b:X:Y:t:r:";
static struct option long_opts[] = {
    { "extract",		no_argument,    	NULL, 'x' },
    { "list",			no_argument,		NULL, 'l' },
    { "create",			no_argument,       	NULL, 'c' },
    { "version",		no_argument, 	    	NULL, VERSION_OPT },
    { "help", 	    	 	no_argument,	    	NULL, HELP_OPT },
    { "output", 		required_argument, 	NULL, 'o' },
    { "index",	    	 	required_argument, 	NULL, 'i' },
    { "width",	    	 	required_argument, 	NULL, 'w' },
    { "height", 		required_argument, 	NULL, 'h' },
    { "palette-size",		required_argument, 	NULL, 'p' },
    { "bit-depth",		required_argument, 	NULL, 'b' },
    /*{ "min-bit-depth",          required_argument,      NULL, 'm' },*/
    { "hotspot-x",       	required_argument, 	NULL, 'X' },
    { "hotspot-y",       	required_argument, 	NULL, 'Y' },
    { "alpha-threshold", 	required_argument, 	NULL, 't' },
    { "icon",       	 	no_argument,       	NULL, ICON_OPT	},
    { "cursor",     	 	no_argument,       	NULL, CURSOR_OPT },
    { "raw", 			required_argument, 	NULL, 'r' },
    { 0, 0, 0, 0 }
};

static bool
filter(int i, int w, int h, int bd, int ps, bool icon, int hx, int hy)
{
    if (image_index != -1 && i != image_index)
	return false;
    if (width != -1 && w != width)
	return false;
    if (height != -1 && h != height)
	return false;
    if (bitdepth != -1 && bd != bitdepth)
	return false;
    /*if (bd < minbitdepth)
        return false;*/
    if (palettesize != -1 && ps != palettesize)
	return false;
    if ((icon_only && !icon) || (cursor_only && icon))
	return false;
    if (hotspot_x_set && hx != hotspot_x)
	return false;
    if (hotspot_y_set && hy != hotspot_y)	
	return false;
    return true;
}

static FILE *
create_outfile_gen(char **out)
{
    if (output != NULL) {
	*out = xstrdup(output);
	return fopen(output, "wb");
    }
    if (isatty(STDOUT_FILENO))
	die(_("refusing to write binary data to terminal"));
    *out = xstrdup(_("(standard out)"));
    return stdout;
}

static FILE *
extract_outfile_gen(char **outname_ptr, int w, int h, int bc, int i)
{
    char *inname = *outname_ptr;

    if (output == NULL || is_directory(output)) {
	StrBuf *outname;
	char *inbase;

	outname = strbuf_new();
	if (output != NULL) {
	    strbuf_append(outname, output);
	    if (!ends_with(output, "/"))
		strbuf_append(outname, "/");
	}
	inbase = strrchr(inname, '/');
	inbase = (inbase == NULL ? inname : inbase+1);
	if (ends_with_nocase(inbase, ".ico") || ends_with_nocase(inbase, ".cur")) {
	    strbuf_append_substring(outname, inbase, 0, strlen(inbase)-4);
	} else {
	    strbuf_append(outname, inbase);
	}
	strbuf_appendf(outname, "_%d_%dx%dx%d.png", i, w, h, bc);
	*outname_ptr = strbuf_free_to_string(outname);
	return fopen(*outname_ptr, "wb");
    }
    else if (strcmp(output, "-") == 0) {
	*outname_ptr = xstrdup(_("(standard out)"));
	return stdout;
    }

    *outname_ptr = xstrdup(output);
    return fopen(output, "wb");
}

static void
display_help(void)
{
    printf(_("Usage: %s [OPTION]... [FILE]...\n"), program_name);
    printf(_("Convert and create Win32 icon (.ico) and cursor (.cur) files.\n"));
    printf(_("\nCommands:\n"));
    printf(_("  -x, --extract                extract images from files\n"));
    printf(_("  -l, --list                   print a list of images in files\n"));
    printf(_("  -c, --create                 create an icon file from specified files\n"));
    printf(_("      --help                   display this help and exit\n"));
    printf(_("      --version                output version information and exit\n"));
    printf(_("\nOptions:\n"));
    printf(_("  -i, --index=NUMBER           match index of image (first is 1)\n"));
    printf(_("  -w, --width=PIXELS           match width of image\n"));
    printf(_("  -h, --height=PIXELS          match height of image\n"));
    printf(_("  -p, --palette-size=COUNT     match number of colors in palette (or 0)\n"));
    printf(_("  -b, --bit-depth=COUNT        match or set number of bits per pixel\n"));
    /*printf(_("  -m, --min-bit-depth=COUNT    match or set minimum number of bits per pixel\n"));*/
    printf(_("  -X, --hotspot-x=COORD        match or set cursor hotspot x-coordinate\n"));
    printf(_("  -Y, --hotspot-y=COORD        match or set cursor hotspot y-coordinate\n"));
    printf(_("  -t, --alpha-threshold=LEVEL  highest level in alpha channel indicating\n"
	     "                               transparent image portions (default is 127)\n"));
    printf(_("  -r, --raw=FILENAME           store input file as raw PNG (\"Vista icons\")\n"));
    printf(_("      --icon                   match icons only\n"));
    printf(_("      --cursor                 match cursors only\n"));
    printf(_("  -o, --output=PATH            where to place extracted files\n"));
    printf(_("\n"));
    printf(_("Report bugs to <%s>.\n"), PACKAGE_BUGREPORT);
}

static bool
open_file_or_stdin(char *name, FILE **outfile, char **outname)
{
    if (strcmp(name, "-") == 0) {
        *outfile = stdin;
	*outname = "(standard in)";
    } else {
        *outfile = fopen(name, "rb");
	*outname = name;
	if (*outfile == NULL) {
	    warn("%s: cannot open file", name);
	    return false;
	}
    }
    return true;
}

int
main(int argc, char **argv)
{
    int c;
    bool list_mode = false;
    bool extract_mode = false;
    bool create_mode = false;
    FILE *in;
    char *inname;
    int raw_filec = 0;
    char** raw_filev = 0;

    set_program_name(argv[0]);

#ifdef ENABLE_NLS
    if (setlocale(LC_ALL, "") == NULL)
	warn(_("%s: cannot set locale: %s"), program_name, errstr);
    if (bindtextdomain(PACKAGE, LOCALEDIR) == NULL)
	warn(_("%s: bindtextdomain failed: %s"), program_name, errstr);
    if (textdomain(PACKAGE) == NULL)
	warn(_("%s: cannot set message domain: %s"), program_name, errstr);
#endif

    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
	switch (c) {
	case 'x':
	    extract_mode = true;
	    break;
	case 'l':
	    list_mode = true;
	    break;
	case 'c':
	    create_mode = true;
	    break;
	case VERSION_OPT:
	    version_etc(stdout, PROGRAM, PACKAGE, VERSION, "Oskar Liljeblad", NULL);
	    exit(0);
	case HELP_OPT:
	    display_help();
	    exit(0);
	case 'o':
	    output = optarg;
	    break;
	case 'i':
	    if (!parse_int32(optarg, &image_index) || image_index < 0)
		die(_("invalid index value: %s"), optarg);
	    break;
	case 'w':
	    if (!parse_int32(optarg, &width) || width < 0)
		die(_("invalid width value: %s"), optarg);
	    break;
	case 'h':
	    if (!parse_int32(optarg, &height) || height < 0)
		die(_("invalid height value: %s"), optarg);
	    break;
	case 'p':
	    if (!parse_int32(optarg, &palettesize) || palettesize < 0)
		die(_("invalid palette-size value: %s"), optarg);
	    break;
	case 'b':
	    if (!parse_int32(optarg, &bitdepth) || bitdepth < 0)
		die(_("invalid bit-depth value: %s"), optarg);
	    break;
        /*case 'm':
            if (!parse_uint32(optarg, &minbitdepth))
                die(_("invalid minimum bit-depth value: %s"), optarg);
            break;*/
	case 'X':
	    if (!parse_int32(optarg, &hotspot_x) || hotspot_x < 0)
		die(_("invalid hotspot-x value: %s"), optarg);
	    hotspot_x_set = true;
	    break;
	case 'Y':
	    if (!parse_int32(optarg, &hotspot_y) || hotspot_y < 0)
		die(_("invalid hotspot-y value: %s"), optarg);
	    hotspot_y_set = true;
	    break;
	case 't':
	    if (!parse_int32(optarg, &alpha_threshold) || alpha_threshold < 0)
		die(_("invalid alpha-threshold value: %s"), optarg);
	    break;
	case 'r':
	    raw_filev = realloc (raw_filev, (raw_filec+1)*sizeof (char*));
	    raw_filev[raw_filec] = optarg;
	    raw_filec++;
	    break;
	case ICON_OPT:
	    icon_only = true;
	    break;
	case CURSOR_OPT:
	    cursor_only = true;
	    break;
	case '?':
	    exit(1);
	}
    }

    if (extract_mode + create_mode + list_mode > 1)
	die(_("multiple commands specified"));
    if (extract_mode + create_mode + list_mode == 0) {
	warn(_("missing argument"));
	display_help();
	exit (1);
    }
    if (icon_only && cursor_only)
	die(_("only one of --icon and --cursor may be specified"));

    if (list_mode) {
	if (argc-optind <= 0)
	    die(_("missing file argument"));
	for (c = optind ; c < argc ; c++) {
	    if (open_file_or_stdin(argv[c], &in, &inname)) {
		if (!extract_icons(in, inname, true, NULL, filter))
		    exit(1);
		if (in != stdin)
		    fclose(in);
	    }
	}
    }

    if (extract_mode) {
	if (argc-optind <= 0)
	    die(_("missing arguments"));

        for (c = optind ; c < argc ; c++) {
            int matched;

	    if (open_file_or_stdin(argv[c], &in, &inname)) {
	        matched = extract_icons(in, inname, false, extract_outfile_gen, filter);
	        if (matched == -1)
	            exit(1);
                if (matched == 0)
                    fprintf(stderr, _("%s: no images matched\n"), inname);
                if (in != stdin)
                    fclose(in);
            }
        }
    }

    if (create_mode) {
        if (argc-optind+raw_filec <= 0)
	    die(_("missing arguments"));
        if (!create_icon(argc-optind, argv+optind, raw_filec, raw_filev, create_outfile_gen, (icon_only ? true : !cursor_only), hotspot_x, hotspot_y, alpha_threshold, bitdepth))
            exit(1);
    }

    exit(0);
}
