/* main.c - Main routine for wrestool
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
#include "configmake.h"
#define _(s) gettext(s)
#define N_(s) gettext_noop(s)
#include "version-etc.h"        	/* Gnulib */
#include "progname.h"			/* Gnulib */
//#include "strcase.h"			/* Gnulib */
#include "dirname.h"			/* Gnulib */
#include "common/error.h"
#include "xalloc.h"			/* Gnulib */
#include "common/intutil.h"
#include "common/io-utils.h"
#include "common/string-utils.h"
#include "wrestool.h"

#define PROGRAM "wrestool"


enum {
    OPT_VERSION = 1000,
    OPT_HELP
};

const char version_etc_copyright[] = "Copyright (C) 1998 Oskar Liljeblad";
bool arg_raw;
static FILE *verbose_file;
static int arg_verbosity;
static char *arg_output;
static char *arg_type;
static char *arg_name;
static char *arg_language;
static int arg_action;
static char *res_types[] = {
    /* 0x01: */
    "cursor", "bitmap", "icon", "menu", "dialog", "string",
    "fontdir", "font", "accelerator", "rcdata", "messagelist",
    "group_cursor", NULL, "group_icon", NULL,
    /* the following are not defined in winbase.h, but found in wrc. */
    /* 0x10: */ 
    "version", "dlginclude", NULL, "plugplay", "vxd",
    "anicursor", "aniicon"
};
#define RES_TYPE_COUNT (sizeof(res_types)/sizeof(char *))

static char *res_type_string_to_id (char *);
static char *get_extract_extension (char *);

/* res_type_id_to_string:
 *   Translate a numeric resource type to it's corresponding string type.
 *   (For informative-ness.)
 */
char *
res_type_id_to_string (int id)
{
    if (id == 241)
	return "toolbar";
    if (id > 0 && id <= RES_TYPE_COUNT)
	return res_types[id-1];
    return NULL;
}

/* res_type_string_to_id:
 *   Translate a resource type string to integer.
 *   (Used to convert the --type option.)
 */
static char *
res_type_string_to_id (char *type)
{
    static char *res_type_ids[] = {
	"-1", "-2", "-3", "-4", "-5", "-6", "-7", "-8", "-9", "-10",
	"-11", "-12", NULL, "-14", NULL, "-16", "-17", NULL, "-19",
	"-20", "-21", "-22"
    };
    int c;

    if (type == NULL)
	return NULL;

    for (c = 0 ; c < RES_TYPE_COUNT ; c++) {
	if (res_types[c] != NULL && !strcasecmp(type, res_types[c]))
	    return res_type_ids[c];
    }

    return type;
}

/* get_extract_extension:
 *   Return extension for files of a certain resource type
 *
 */
static char *
get_extract_extension (char *type)
{
    uint16_t value;

    type = res_type_string_to_id(type);
    STRIP_RES_ID_FORMAT(type);
    if (parse_uint16(type, &value)) {
	if (value == 2)
	    return ".bmp";
	if (value == 14)
	    return ".ico";
	if (value == 12)
	    return ".cur";
    }

    return "";
}

#define SET_IF_NULL(x,def) ((x) = ((x) == NULL ? (def) : (x)))

/* get_destination_name:
 *   Make a filename for a resource that is to be extracted.
 */
char *
get_destination_name (WinLibrary *fi, char *type, char *name, char *lang)
{
    static char filename[1024];

    /* initialize --type, --name and --language options */
    SET_IF_NULL(type, "");
    SET_IF_NULL(name, "");
    if (!strcmp(lang, "1033"))
	lang = NULL;
    STRIP_RES_ID_FORMAT(type);
    STRIP_RES_ID_FORMAT(name);
    STRIP_RES_ID_FORMAT(lang);

    /* returning NULL means that output should be done to stdout */

    /* if --output not specified, write to STDOUT */
    if (arg_output == NULL)
	return NULL;

    /* if --output'ing to a directory, make filename */
    if (is_directory(arg_output) || ends_with(arg_output, "/")) {
	/* char *tmp = strdup(fi->name);
	if (tmp == NULL)
	    malloc_failure(); */

	snprintf (filename, 1024, "%s%s%s_%s_%s%s%s%s",
			  arg_output,
		      (ends_with(arg_output, "/") ? "" : "/"),
			  base_name(fi->name),
			  type,
			  name,
			  (lang != NULL && fi->is_PE_binary ? "_" : ""),
			  (lang != NULL && fi->is_PE_binary ? lang : ""),
			  get_extract_extension(type));
	/* free(tmp); */
	return filename;
    }

    /* otherwise, just return the --output argument */
    return arg_output;
}

static void
display_help(void)
{
    printf(_("Usage: %s [OPTION]... [FILE]...\n"), program_name);
    printf(_("Extract resources from Microsoft Windows(R) binaries.\n"));
    printf(_("\nCommands:\n"));
    printf(_("  -x, --extract           extract resources\n"));
    printf(_("  -l, --list              output list of resources (default)\n"));
    printf(_("\nFilters:\n"));
    printf(_("  -t, --type=[+|-]ID      resource type identifier\n"));
    printf(_("  -n, --name=[+|-]ID      resource name identifier\n"));
    printf(_("  -L, --language=[+|-]ID  resource language identifier\n"));
    printf(_("      --all               perform operation on all resource (default)\n"));
    printf(_("\nMiscellaneous:\n"));
    printf(_("  -o, --output=PATH       where to place extracted files\n"));
    printf(_("  -R, --raw               do not parse resource contents\n"));
    printf(_("  -v, --verbose           explain what is being done\n"));
    printf(_("      --help              display this help and exit\n"));
    printf(_("      --version           output version information and exit\n"));
    printf(_("\nA leading `+' in --type, name or language options indicates a true string\n"
             "identifier. Similarly, `-' indicates a true numeric identifier.\n\n"));
    printf(_("Report bugs to %s.\n"), PACKAGE_BUGREPORT);
}

int
main (int argc, char **argv)
{
    int c;

    arg_type = arg_name = arg_language = NULL;
    arg_verbosity = 0;
    arg_raw = false;
    arg_action = ACTION_LIST;

#ifdef ENABLE_NLS
    if (setlocale(LC_ALL, "") == NULL)
	warn(_("%s: cannot set locale: %s"), program_name, errstr);
    if (bindtextdomain(PACKAGE, LOCALEDIR) == NULL)
	warn(_("%s: bindtextdomain failed: %s"), program_name, errstr);
    if (textdomain(PACKAGE) == NULL)
	warn(_("%s: cannot set message domain: %s"), program_name, errstr);
#endif

    set_program_name(argv[0]);

    /* analyse arguments */
    while (true) {
	int option_index = 0;
	static struct option long_options[] = {
	    { "type",		required_argument,	NULL, 't' },
	    { "name", 		required_argument,	NULL, 'n' },
	    { "language",	required_argument,	NULL, 'L' },
	    { "output",     required_argument,  NULL, 'o' },
	    { "all",		no_argument,		NULL, 'a' },
	    { "raw",        no_argument,        NULL, 'R' },
	    { "extract",	no_argument,		NULL, 'x' },
	    { "list",		no_argument,		NULL, 'l' },
	    { "verbose",	no_argument,		NULL, 'v' },
	    { "version",	no_argument,		NULL, OPT_VERSION },
	    { "help",		no_argument,		NULL, OPT_HELP },
	    { 0, 0, 0, 0 }
	};
	c = getopt_long (argc, argv, "t:n:L:o:aRrxlv", long_options, &option_index);
	if (c == EOF)
	    break;

	    switch (c) {
	    case 't': arg_type = optarg; break;
	    case 'n': arg_name = optarg; break;
	    case 'L': arg_language = optarg; break;
	    case 'a': arg_type = arg_name = arg_language = NULL; break;
	    case 'R': arg_raw = true; break;
	    case 'x': arg_action = ACTION_EXTRACT; break;
	    case 'l': arg_action = ACTION_LIST; break;
	    case 'v': arg_verbosity++; break;
	    case 'o': arg_output = optarg; break;
	    case OPT_VERSION:
		version_etc(stdout, PROGRAM, PACKAGE, VERSION, "Oskar Liljeblad", NULL);
		return 0;
	    case OPT_HELP:
	    	display_help();
		return 0;
	    case '?':
	    default:
		return 1;
	    }
	}

	verbose_file = (arg_output == NULL) ? stderr : stdin;

	/* warn about unnecessary options */
	if (arg_action == ACTION_LIST) {
	    if (arg_language != NULL && (arg_name == NULL || arg_type == NULL))
		warn(_("--language has no effect without --name and --type"));
	    if (arg_name != NULL && arg_type == NULL)
		warn(_("--name has no effect without --type"));
	}

	/* translate --type option from resource type string to integer */
	arg_type = res_type_string_to_id(arg_type);

	/* make sure at least one file has been specified */
	if (optind >= argc) {
		warn(_("missing file argument\nTry `%s --help' for more information."), program_name);
		return 1;
	}

	/* for each file */
	for (c = optind ; c < argc ; c++) {
		WinLibrary fi;
		
		/* initiate stuff */
		fi.file = NULL;
		fi.memory = NULL;

		/* get file size */
		fi.name = argv[c];
		fi.total_size = file_size(fi.name);
		if (fi.total_size == -1) {
			die_errno("%s", fi.name);
			goto cleanup;
		}
		if (fi.total_size == 0) {
			warn(_("%s: file has a size of 0"), fi.name);
			goto cleanup;
		}

		/* open file */
		fi.file = fopen(fi.name, "rb");
		if (fi.file == NULL) {
			die_errno("%s", fi.name);
			goto cleanup;
		}
		
		/* read all of file */
		fi.memory = xmalloc(fi.total_size);
		if (fread(fi.memory, fi.total_size, 1, fi.file) != 1) {
			die_errno("%s", fi.name);
			goto cleanup;
		}

		/* identify file and find resource table */
		if (!read_library (&fi)) {
			/* error reported by read_library */
			goto cleanup;
		}

	//	verbose_printf("file is a %s\n",
	//		fi.is_PE_binary ? "Windows NT `PE' binary" : "Windows 3.1 `NE' binary");

		/* warn about more unnecessary options */
		if (!fi.is_PE_binary && arg_language != NULL)
			warn(_("%s: --language has no effect because file is 16-bit binary"), fi.name);

		/* do the specified command */
		if (arg_action == ACTION_LIST) {
			do_resources (&fi, arg_type, arg_name, arg_language, print_resources_callback);
			/* errors will be printed by the callback */
		} else if (arg_action == ACTION_EXTRACT) {
			do_resources (&fi, arg_type, arg_name, arg_language, extract_resources_callback);
			/* errors will be printed by the callback */
		}

		/* free stuff and close file */
		cleanup:
		if (fi.file != NULL)
			fclose(fi.file);
		if (fi.memory != NULL)
			free(fi.memory);
	}

	return 0;
}
