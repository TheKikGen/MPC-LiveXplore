/*
__ __| |           |  /_) |     ___|             |           |
  |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
  |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
 _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
-----------------------------------------------------------------------------
TKGL_CTRLDUMP : MPC controller dumper.
This "low-level" library allows you todump all in/ou message from the MPC private
and public port of the Akai controller.

Preload syntax is :
	LD_PRELOAD=/full/path/to/tkgl_ctrldump.so /usr/bin/MPC

Compile with :
	arm-linux-gnueabihf-gcc tkgl_ctrldump.c -o tkgl_ctrldump.so -shared -fPIC

-----------------------------------------------------------------------------

Disclaimer.
This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
NON COMMERCIAL - PERSONAL USE ONLY : You may not use the material for pure
commercial closed code solution without the licensor permission.
You are free to copy and redistribute the material in any medium or format,
adapt, transform, and build upon the material.
You must give appropriate credit, a link to the github site
https://github.com/TheKikGen/USBMidiKliK4x4 , provide a link to the license,
and indicate if changes were made. You may do so in any reasonable manner,
but not in any way that suggests the licensor endorses you or your use.
You may not apply legal terms or technological measures that legally restrict
others from doing anything the license permits.
You do not have to comply with the license for elements of the material
in the public domain or where your use is permitted by an applicable exception
or limitation.
No warranties are given. The license may not give you all of the permissions
necessary for your intended use.  This program is distributed in the hope that
it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <alsa/asoundlib.h>
#include <dlfcn.h>
#include <libgen.h>


// Function prototypes
static int MPC_get_cardid( char *cardid );

// Globals
static char mpc_cardid = '-';
static char mpc_private_port[12];
static char mpc_public_port[12];
static snd_rawmidi_t *tkgl_inputp;

// Alsa API
static typeof(&snd_rawmidi_open) orig_snd_rawmidi_open;
static typeof(&snd_rawmidi_write) orig_snd_rawmidi_write;
static typeof(&snd_rawmidi_read) orig_snd_rawmidi_read;

static void tkgl_init()
{
	fprintf(stdout,"--------------------------------------\n");
	fprintf (stdout,"TKGL_CTRLDUMP V1.0 by the KikGen Labs\n");
	fprintf(stdout,"--------------------------------------\n");

	// Initialize card id for public and private
	if ( MPC_get_cardid( &mpc_cardid) < 0 ) {
			fprintf(stderr,"**** Error : MPC card id not found\n");
			exit(1);
	}

	sprintf(mpc_private_port,"hw:%c,0,1",mpc_cardid);
	sprintf(mpc_public_port,"hw:%c,0,0",mpc_cardid);
	fprintf(stdout,"(tkgl_ctrldump) MPC card id hw:%c found\n",mpc_cardid);
	fprintf(stdout,"(tkgl_ctrldump) MPC Private port is %s\n",mpc_private_port);
	fprintf(stdout,"(tkgl_ctrldump) MPC Public port is %s\n",mpc_public_port);

}

////////////////////////////////////////////////////////////////////////////////
// Clean DUMP of a buffer to screen
////////////////////////////////////////////////////////////////////////////////
static void ShowBufferHexDump(const uint8_t* data, uint16_t sz, uint8_t nl)
{
    uint8_t b;
    char asciiBuff[33];
    uint8_t c=0;
    for (uint16_t idx = 0 ; idx < sz; idx++) {
				if ( c == 0 && idx > 0) fprintf(stdout,"                        ");
        b = (*data++);
  			fprintf(stdout,"%02X ",b);
        asciiBuff[c++] = ( b >= 0x20 && b< 127? b : '.' ) ;
        if ( c == nl || idx == sz -1 ) {
          asciiBuff[c] = 0;
					for (  ; c < nl; c++  ) fprintf(stdout,"   ");
          c = 0;
          fprintf(stdout," | %s\n", asciiBuff);
        }
    }
}

// Get Private and public card #
// MPC Alsa Card # can change, depending on the number of controller connected
// amidi system call is the "lazy" but simple way to do it
static int MPC_get_cardid( char *cardid )
{

  FILE *fp;
  char amidi_result[128], cmd[128];

  // Open the command for reading
	fflush(0);
  fp = popen("amidi -l | grep Private | cut -d' ' -f3 | cut -d':' -f2", "r");

	if (fp == NULL) {
    fprintf(stderr,"**** Error : failed to run command %s\n",cmd );
    exit(1);
  }

	while ( fgets(amidi_result, sizeof(amidi_result), fp) );
	pclose(fp);

	// Read the last line of output

	if ( amidi_result[0] < '0' || amidi_result[0] > '9') return -1;

	// The first character is the card id
  *cardid =  amidi_result[0];
	return 0;
}


// API hooks

int snd_rawmidi_open(snd_rawmidi_t **inputp, snd_rawmidi_t **outputp, const char *name, int mode)
{
	orig_snd_rawmidi_open = dlsym(RTLD_NEXT, "snd_rawmidi_open");
	static int do_init = 1;

	if ( do_init) {
		do_init = 0 ;
		tkgl_init();
	}

	fprintf(stdout,"(tkgl_ctrldump) snd_rawmidi_open name %s mode %d\n",name,mode);

	// do open as usual
	return orig_snd_rawmidi_open(inputp, outputp, name, mode);
}

ssize_t snd_rawmidi_write(snd_rawmidi_t * 	rawmidi,const void * 	buffer,size_t 	size) {

	orig_snd_rawmidi_write = dlsym(RTLD_NEXT, "snd_rawmidi_write");

	const char *name = snd_rawmidi_name(rawmidi);
	// Dump if it is the controller card id
	if ( name[3]  == mpc_cardid ) {
			fprintf(stdout,"MPC      --> %s = ",name);
			ShowBufferHexDump(buffer, size, 16);
			fprintf(stdout,"\n");
			fflush(stdout);
	}

	return orig_snd_rawmidi_write(rawmidi,buffer,size);
}

ssize_t snd_rawmidi_read(snd_rawmidi_t *rawmidi, void *buffer, size_t size) {

	orig_snd_rawmidi_read = dlsym(RTLD_NEXT,"snd_rawmidi_read");
	ssize_t r = orig_snd_rawmidi_read(rawmidi, buffer, size);
	const char *name = snd_rawmidi_name(rawmidi);

	// Dump if it is the controller card id
	if ( r && name[3]  == mpc_cardid ) {
		fprintf(stdout,"%s --> MPC      = ",name);
		ShowBufferHexDump(buffer, r, 16);
		fprintf(stdout,"\n");
		fflush(stdout);

	}

	return r;
}
