/*
__ __| |           |  /_) |     ___|             |           |
  |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
  |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
 _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
-----------------------------------------------------------------------------
TKGL_ANYCTRL MPC LD_PRELOAD library.
This "low-level" library allows you to set up any controller as a control
surface in addition to standard hardware to drive the standalone MPC app.
By a simple midi message mapping in your own controller, it is possible to
simulate "buttons" press, and get more shortut like those of the MPC X
(track mute, pad mixer, solo, mute, etc...) or to add more pads or qlinks.

You must create a connect script named "tkgl_anyctrl_cxscript.sh" and place
it in the same directory than the library. The script must contain at a minimum
the following command to enable standard MPC controller :

	aconnect 20:1 135:0

You can add any connection you may need after that line.

Preload syntax is :
	LD_PRELOAD=/full/path/to/tkgl_anyctrl.so /usr/bin/MPC

Compile with :
	arm-linux-gnueabihf-gcc tkgl_anyctrl.c -o tkgl_anyctrl.so -shared -fPIC

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

// connect script file name. place it in the same directory than the lib
#define CONNECT_SCRIPT "tkgl_anyctrl_cxscript.sh"

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
static typeof(&snd_seq_create_simple_port) orig_snd_seq_create_simple_port;
static typeof(&snd_midi_event_decode) orig_snd_midi_event_decode;

static void tkgl_init()
{
	fprintf(stdout,"------------------------------------\n");
	fprintf (stdout,"TKGL_ANYCTRL V1.0 by the KikGen Labs\n");
	fprintf(stdout,"------------------------------------\n");

	// Initialize card id for public and private
	if ( MPC_get_cardid( &mpc_cardid) < 0 ) {
			fprintf(stderr,"**** Error : MPC card id not found\n");
			exit(1);
	}

	sprintf(mpc_private_port,"hw:%c,0,1",mpc_cardid);
	sprintf(mpc_public_port,"hw:%c,0,0",mpc_cardid);
	fprintf(stdout,"(tkgl_anyctrl) MPC card id hw:%c found\n",mpc_cardid);
	fprintf(stdout,"(tkgl_anyctrl) MPC Private port is %s\n",mpc_private_port);
	fprintf(stdout,"(tkgl_anyctrl) MPC Public port is %s\n",mpc_public_port);

	// Virtual port
	if ( snd_rawmidi_open(&tkgl_inputp, NULL, "virtual", 2) ) {
		fprintf(stderr,"*** Error : can't create tkgl_anyctrl virtual port\n");
		exit(1);
	};
	fprintf(stdout,"(tkgl_anyctrl) Virtual port created.\n");
	fflush(0);
}

// Connect MPC controller to our virtual private port before SYSEX id string be sent
static void tkgl_connect()
{
	Dl_info dl_info;
	dladdr((void*)tkgl_init, &dl_info);

	char lib_path[128];
	char cmd[128];

	strcpy(lib_path,dl_info.dli_fname);
	sprintf(cmd,"sh %s/%s",dirname(lib_path),CONNECT_SCRIPT);
	if ( system(cmd) != 0 ) {
		fprintf(stdout,"(tkgl_anyctrl) Warning : some errors detected when lauching connect script cmd %s.\n",cmd);
	};
	sprintf(cmd,"cat %s/%s",lib_path,CONNECT_SCRIPT);
	system(cmd);
	fprintf(stdout,"(tkgl_anyctrl) MPC controller port connected to virtual client.\n");
	fprintf(stdout,"------------------------------------\n");
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

	// Substitute the hardware private input port by our input virtual port
	if ( strcmp(mpc_private_port,name) == 0  && inputp ) {
		*inputp = tkgl_inputp;
		return 0;
	}

	fprintf(stdout,"(tkgl_anyctrl) snd_rawmidi_open name %s mode %d\n",name,mode);

	// Else do open as usual
	return orig_snd_rawmidi_open(inputp, outputp, name, mode);
}

ssize_t snd_rawmidi_write(snd_rawmidi_t * 	rawmidi,const void * 	buffer,size_t 	size) {

	orig_snd_rawmidi_write = dlsym(RTLD_NEXT, "snd_rawmidi_write");

	static int aconnect = 1;

	// Connect MPC controller to our virtual private port before SYSEX id string be sent
	if ( aconnect && strcmp(mpc_public_port,snd_rawmidi_name(rawmidi)) == 0 ) {
			aconnect = 0;
			tkgl_connect();
	}

	return orig_snd_rawmidi_write(rawmidi,buffer,size);
}

// snd_seq --------------------------------------------------------------------

int snd_seq_create_simple_port	(	snd_seq_t * 	seq, const char * 	name, unsigned int 	caps, unsigned int 	type )
{
	orig_snd_seq_create_simple_port = dlsym(RTLD_NEXT, "snd_seq_create_simple_port");

	// Add subscriptions rights to created ports, so we can use aconnect without restrictions

	if (caps & SND_SEQ_PORT_CAP_READ) {
		caps |= SND_SEQ_PORT_CAP_SYNC_READ | SND_SEQ_PORT_CAP_SUBS_READ;
	}

	if (caps & SND_SEQ_PORT_CAP_WRITE) {
		caps |= SND_SEQ_PORT_CAP_SYNC_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
	}

	return orig_snd_seq_create_simple_port(seq,name,caps,type);
}

long snd_midi_event_decode	(	snd_midi_event_t * 	dev,unsigned char * 	buf,long 	count, const snd_seq_event_t * 	ev )
{
	orig_snd_midi_event_decode = dlsym(RTLD_NEXT, "snd_midi_event_decode");

	// Disable running status. Board effect : disabled for all ports...
	snd_midi_event_no_status(dev,1);

	return orig_snd_midi_event_decode(dev,buf,count,ev);

}
