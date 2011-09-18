/* wrestool.h - Common definitions for wrestool
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

#ifndef WRESTOOL_H
#define WRESTOOL_H

#include <unistd.h>		/* POSIX */
#include <stdarg.h>		/* C89 */
#include <stdio.h>		/* C89 */
#include <stdlib.h>		/* C89 */
#include <stdint.h>		/* POSIX/Gnulib */
#include <string.h>		/* C89 */
#include <errno.h>		/* C89 */
#include <getopt.h>		/* GNU Libc/Gnulib */
#include "common/common.h"
//#include "../common/win32.h"
//#include "../common/fileread.h"
//#include "../common/util.h"
//#include "../common/common.h"

/*
 * External Variables
 */

extern char *prgname;
extern bool arg_raw;

/*
 * Structures 
 */

typedef struct _WinLibrary {
	char *name;
	FILE *file;
	char *memory;
	uint8_t *first_resource;
	bool is_PE_binary;
	int total_size;
} WinLibrary;

typedef struct _WinResource {
	char id[256];
	void *this;
	void *children;
	int level;
	bool numeric_id;
	bool is_directory;
} WinResource;

#define WINRES_ID_MAXLEN (256)

/*
 * Definitions
 */

#define ACTION_LIST 				1	/* command: list resources */
#define ACTION_EXTRACT				2	/* command: extract resources */
#define CALLBACK_STOP				0	/* results of ResourceCallback */
#define CALLBACK_CONTINUE			1
#define CALLBACK_CONTINUE_RECURS	2

#define MZ_HEADER(x)	((DOSImageHeader *)(x))
#define NE_HEADER(x)	((OS2ImageHeader *)PE_HEADER(x))
#define NE_TYPEINFO_NEXT(x) ((Win16NETypeInfo *)((uint8_t *)(x) + sizeof(Win16NETypeInfo) + \
						    ((Win16NETypeInfo *)x)->count * sizeof(Win16NENameInfo)))
#define NE_RESOURCE_NAME_IS_NUMERIC (0x8000)

#define STRIP_RES_ID_FORMAT(x) (x != NULL && (x[0] == '-' || x[0] == '+') ? ++x : x)

typedef void (*DoResourceCallback) (WinLibrary *, WinResource *, WinResource *, WinResource *, WinResource *);

/*
 * Function Prototypes
 */

/* resource.c */
/* WinResource *list_resources (WinLibrary *, WinResource *, int *); */
bool read_library (WinLibrary *);
WinResource *find_resource (WinLibrary *, char *, char *, char *, int *);
void *get_resource_entry (WinLibrary *, WinResource *, int *);
void do_resources (WinLibrary *, char *, char *, char *, DoResourceCallback);
void print_resources_callback (WinLibrary *, WinResource *, WinResource *, WinResource *, WinResource *);
/* bool compare_resource_id (WinResource *, char *); */

/* main.c */
char *res_type_id_to_string (int);
char *get_destination_name (WinLibrary *, char *, char *, char *);

/* extract.c */
void *extract_resource (WinLibrary *, WinResource *, int *, bool *, char *, char *, bool);
void extract_resources_callback (WinLibrary *, WinResource *, WinResource *, WinResource *, WinResource *);

#endif
