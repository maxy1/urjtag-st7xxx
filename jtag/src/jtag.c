/*
 * $Id$
 *
 * Copyright (C) 2002, 2003 ETC s.r.o.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by Marcel Telka <marcel@telka.sk>, 2002, 2003.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "part.h"
#include "ctrl.h"
#include "tap.h"

#include "detect.h"
#include "bus.h"

bus_driver_t *bus_driver = NULL;

static char *
get_token( char *buf )
{
	return strtok( buf, " \f\n\r\t\v" );
}

void detectflash( parts *ps );
void readmem( parts *ps, FILE *f, uint32_t addr, uint32_t len );
void flashmem( parts *ps, FILE *f, uint32_t addr );
void flashmsbin( parts *ps, FILE *f );

void help( const char *cmd );

void discovery( const char *filename );

int
main( void )
{
	char *line = NULL;
	parts *ps = NULL;

	printf(
			"%s\n"
			"Copyright (C) 2002 ETC s.r.o.\n"
			"%s is free software, covered by the GNU General Public License, and you are\n"
			"welcome to change it and/or distribute copies of it under certain conditions.\n"
			"There is absolutely no warranty for %s.\n\n", PACKAGE_STRING, PACKAGE, PACKAGE
	);

	printf( "Warning: %s may damage your hardware! Type \"quit\" for exit!\n\n", PACKAGE );
	printf( "Type \"help\" for help.\n\n" );

	if (!tap_init()) {
		printf( "TAP initialization failed! Exiting.\n" );
		return 1;
	}

	tap_set_trst( 0 );
	tap_set_trst( 1 );

	for (;;) {
		char *t;

		free( line );
		line = readline( "jtag> " );

		if (!line || !*line)
			continue;
		add_history( line );

		t = get_token( line );
		if (!t)
			continue;

		if (strcmp( t, "quit" ) == 0)
			break;

		if (strcmp( t, "help" ) == 0) {
			t = get_token( NULL );
			if (get_token( NULL ))
				printf( "help: Syntax error!\n" );
			else
				help( t );
			continue;
		}

		if (strcmp( t, "discovery" ) == 0) {
			t = get_token( NULL );
			if (!t) {
				printf( "discovery: missing filename\n" );
				continue;
			}
			if (get_token( NULL )) {
				printf( "syntax error!\n" );
				continue;
			}
			discovery( t );
			continue;
		}

		if (strcmp( t, "detect" ) == 0) {
			t = get_token( NULL );
			if (ps)
				parts_free( ps );
			ps = detect_parts( JTAG_DATA_DIR );
			if (!ps->len)
				continue;
			parts_set_instruction( ps, "SAMPLE/PRELOAD" );
			parts_shift_instructions( ps );
			parts_shift_data_registers( ps );
			parts_set_instruction( ps, "BYPASS" );
			parts_shift_instructions( ps );
			if (strcmp( ps->parts[0]->part, "SA1110" ) == 0)
				bus_driver = &sa1110_bus_driver;
			if (strcmp( ps->parts[0]->part, "PXA250" ) == 0)
				bus_driver = &pxa250_bus_driver;
			continue;
		}

		if (strcmp( t, "flashmem" ) == 0) {
			FILE *f;
			int msbin = 0;
			uint32_t addr = 0;

			if (!ps) {
				printf( "Run \"detect\" first.\n" );
				continue;
			}

			t = get_token( NULL );
			if (!t) {
				printf( "flashmem: Missing argument(s)\n" );
				continue;
			}
			if (strcmp( t, "msbin" ) != 0) {
				if ((sscanf( t, "0x%x", &addr ) != 1) && (sscanf( t, "%d", &addr ) != 1)) {
					printf( "error\n" );
					continue;
				}
				printf( "0x%08X\n", addr );
			} else
				msbin = 1;
			/* filename */
			t = get_token( NULL );
			if (!t) {
				printf( "flashmem: missing filename\n" );
				continue;
			}
			f = fopen( t, "r" );
			if (!f) {
				printf( "Unable to open file `%s'!\n", t );
				continue;
			}
			t = get_token( NULL );
			if (t) {
				printf( "syntax error!\n" );
				fclose( f );
				continue;
			}
			if (msbin) 
				flashmsbin( ps, f );
			else
				flashmem( ps, f, addr );
			fclose( f );
			continue;
		}

		if (strcmp( t, "readmem" ) == 0) {
			FILE *f;
			uint32_t addr = 0;
			uint32_t len = 0;

			if (!ps) {
				printf( "Run \"detect\" first.\n" );
				continue;
			}

			t = get_token( NULL );
			if (!t) {
				printf( "flashmem: Missing argument(s)\n" );
				continue;
			}
			if ((sscanf( t, "0x%x", &addr ) != 1) && (sscanf( t, "%d", &addr ) != 1)) {
				printf( "syntax error\n" );
				continue;
			}

			t = get_token( NULL );
			if (!t) {
				printf( "flashmem: Missing argument(s)\n" );
				continue;
			}
			if ((sscanf( t, "0x%x", &len ) != 1) && (sscanf( t, "%d", &len ) != 1)) {
				printf( "syntax error\n" );
				continue;
			}

			/* filename */
			t = get_token( NULL );
			if (!t) {
				printf( "flashmem: missing filename\n" );
				continue;
			}
			f = fopen( t, "w" );
			if (!f) {
				printf( "Unable to create file `%s'!\n", t );
				continue;
			}
			t = get_token( NULL );
			if (t) {
				printf( "syntax error!\n" );
				fclose( f );
				continue;
			}

			readmem( ps, f, addr, len );

			fclose( f );
			continue;
		}

		if (strcmp( t, "detectflash" ) == 0) {
			if (!ps) {
				printf( "Run \"detect\" first.\n" );
				continue;
			}

			detectflash( ps );
			continue;
		}

		if (strcmp( t, "print" ) == 0) {
			parts_print( ps, 1 );
			continue;
		}

		if (strcmp( t, "instruction" ) == 0) {
			int n;

			if (!ps) {
				printf( "Run \"detect\" first.\n" );
				continue;
			}

			t = get_token( NULL );
			if (!t) {
				printf( "instruction: syntax error\n" );
				continue;
			}

			n = strtol( t, &t, 10 );
			if (t && *t) {
				printf( "instruction: syntax error\n" );
				continue;
			}
			
			if ((n < 0) || (n >= ps->len)) {
				printf( "instruction: invalid part number\n" );
				continue;
			}

			t = get_token( NULL );
			if (!t) {
				printf( "Missing instruction name\n" );
				continue;
			}

			part_set_instruction( ps->parts[n], t );

			continue;
		}

		if (strcmp( t, "shift" ) == 0) {
			t = get_token( NULL );

			if (t && (strcmp( t, "ir" ) == 0)) {
				parts_shift_instructions( ps );
				continue;
			}

			if (t && (strcmp( t, "dr" ) == 0)) {
				parts_shift_data_registers( ps );
				continue;
			}

			printf( "shift: syntax error\n" );
			continue;
		}

		if (strcmp( t, "dr" ) == 0) {
			int n;

			if (!ps) {
				printf( "Run \"detect\" first.\n" );
				continue;
			}

			t = get_token( NULL );
			if (!t) {
				printf( "dr: syntax error\n" );
				continue;
			}

			n = strtol( t, &t, 10 );
			if (t && *t) {
				printf( "dr: syntax error\n" );
				continue;
			}
			
			if ((n < 0) || (n >= ps->len)) {
				printf( "dr: invalid part number\n" );
				continue;
			}

			printf( "%s\n", register_get_string( ps->parts[n]->active_instruction->data_register->out ) );

			continue;
		}

		if (strcmp( t, "set" ) == 0) {
			t = get_token( NULL );
			if (!t) {
				printf( "set: syntax error\n" );
				continue;
			}
			
			continue;
		}

		printf( "%s: unknown command\n", t );
	}

	free( line );
	parts_free( ps );

	tap_reset();
	tap_done();

	return 0;
}
