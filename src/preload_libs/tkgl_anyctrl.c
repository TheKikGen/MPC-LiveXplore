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

// Globals
static int mpc_cardid = -1;
static char mpc_private_port[12];
static char mpc_public_port[12];
static snd_rawmidi_t *tkgl_inputp ;
static snd_rawmidi_t *tkgl_outputp ;
static int first_seqvirtual_addr=-1;
static int seqvirtual_addr_index=-1;
static int anyctrl_seqport=-1;


// Alsa API
static typeof(&snd_rawmidi_open) orig_snd_rawmidi_open;
static typeof(&snd_rawmidi_write) orig_snd_rawmidi_write;
static typeof(&snd_seq_create_simple_port) orig_snd_seq_create_simple_port;
static typeof(&snd_midi_event_decode) orig_snd_midi_event_decode;


// Get a one line positive int value from a system command
static int getcmd_int(char* cmd)
{
    FILE *fp;
    char result[128];

    // Open the command for reading
	fflush(0);
	fp = popen(cmd, "r");

	if (fp == NULL) {
	  fprintf(stderr,"**** Error : failed to run command %s\n",cmd );
	  exit(1);
	}

	while ( fgets(result, sizeof(result), fp) ) ;

	pclose(fp);

	// Read the last line of output
    char * endPtr;
    long value = strtol( result, &endPtr, 10 );

    if ( endPtr == result ) return -1 ;

	return value;
}

// Get Private and public card #
// MPC Alsa Card # can change, depending on the number of controller connected
// amidi system call is the "lazy" but simple way to do it
// Do not remove unset LD_PRELOAD to avoid recursive calls !
static int MPC_get_cardid( )
{
	return getcmd_int("unset LD_PRELOAD ; amidi -l | grep Private | cut -d' ' -f3 | cut -d':' -f2");
}

// Get Last seq client number to guess virtual ports
// Do not remove unset LD_PRELOAD to avoid recursive calls !
static int MPC_get_last_seqclient( )
{
	return getcmd_int("unset LD_PRELOAD ; aconnect -l | grep 'client .*:.*' | cut -d':' -f1 | cut -d' ' -f2");
}

// Get client port by name
// Do not remove unset LD_PRELOAD to avoid recursive calls !
static int MPC_get_seqport(char *name)
{
	char cmd[128];

	sprintf(cmd,"unset LD_PRELOAD ; aconnect -l | grep \"client .*: '%s' .*card=.*\" | cut -d' ' -f2 | cut -d':' -f1",name);
	return getcmd_int(cmd);
}

// Initialize almost everything
static void tkgl_init()
{
	fprintf(stdout,"------------------------------------\n");
	fprintf (stdout,"TKGL_ANYCTRL V1.0 by the KikGen Labs\n");
	fprintf(stdout,"------------------------------------\n");

	// Initialize card id for public and private
	if ( (mpc_cardid = MPC_get_cardid()) < 0 ) {
			fprintf(stderr,"**** Error : MPC card not found\n");
			exit(1);
	}

	sprintf(mpc_private_port,"hw:%d,0,1",mpc_cardid);
	sprintf(mpc_public_port,"hw:%d,0,0",mpc_cardid);
	fprintf(stdout,"(tkgl_anyctrl) MPC card id hw:%d found\n",mpc_cardid);
	fprintf(stdout,"(tkgl_anyctrl) MPC Private port is %s\n",mpc_private_port);
	fprintf(stdout,"(tkgl_anyctrl) MPC Public port is %s\n",mpc_public_port);

	if ( (first_seqvirtual_addr = MPC_get_last_seqclient()) < 0 ) {
			fprintf(stderr,"**** Error : Last seq client not found\n");
			exit(1);
	}
	seqvirtual_addr_index = first_seqvirtual_addr ;
	first_seqvirtual_addr++;

	fprintf(stdout,"(tkgl_anyctrl) First virtual seq client available address is %d\n",first_seqvirtual_addr);


	char * port_name = getenv("ANYCTRL_NAME") ;
	if ( port_name == NULL) {
		fprintf(stdout,"(tkgl_anyctrl) ANYCTRL_NAME environment variable not set.\n") ;
	}
	else {
		// Initialize card id for public and private
		anyctrl_seqport = MPC_get_seqport(port_name);
		if ( anyctrl_seqport  < 0 ) {
			fprintf(stderr,"**** Error : %s seq client not found\n",port_name);
			exit(1);
		}
		fprintf(stdout,"(tkgl_anyctrl) %s connect port is %d:0\n",port_name,anyctrl_seqport);
		fprintf(stdout,"(tkgl_anyctrl) Port creation was already disabled for : %s\n",port_name);
	}


	fflush(stdout);
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

	//fprintf(stdout,"(tkgl_anyctrl) snd_rawmidi_open name %s mode %d\n",name,mode);

	// Substitute the hardware private input/ouput port by our input virtual port
	// and connect it to the hardware port again
	int r = orig_snd_rawmidi_open(inputp, outputp, "virtual", mode);

	if ( r != 0 ) return r;
    seqvirtual_addr_index++;

	char cmd[128];
	// Substitute the hardware private input port by our input virtual port
	// Do not remove unset LD_PRELOAD to avoid recursive calls !

	if ( strcmp(mpc_private_port,name) == 0   ) {
		if (inputp) {
			sprintf(cmd,"unset LD_PRELOAD ; aconnect 20:1 %d:0",seqvirtual_addr_index);
			system(cmd);
			// Connect our midi device  port 0 is used.
			if (anyctrl_seqport >= 0) {
				sprintf(cmd,"unset LD_PRELOAD ; aconnect %d:0 %d:0",anyctrl_seqport,seqvirtual_addr_index);
				system(cmd);
			}

			return 0;
		}

		if (outputp) {
			sprintf(cmd,"unset LD_PRELOAD ; aconnect %d:0 20:1",seqvirtual_addr_index);
			system(cmd);
			return 0;
		}
	}

	if ( strcmp(mpc_public_port,name) == 0   ) {
		if (inputp) {
			sprintf(cmd,"unset LD_PRELOAD ; aconnect 20:0 %d:0",seqvirtual_addr_index);
			system(cmd);
			return 0;
		}

		if (outputp) {
			sprintf(cmd,"unset LD_PRELOAD ; aconnect %d:0 20:0",seqvirtual_addr_index);
			system(cmd);
			return 0;
		}
	}


	return 0;

}

// ssize_t snd_rawmidi_write(snd_rawmidi_t * 	rawmidi,const void * 	buffer,size_t 	size) {

// 	orig_snd_rawmidi_write = dlsym(RTLD_NEXT, "snd_rawmidi_write");


// 	return orig_snd_rawmidi_write(rawmidi,buffer,size);
// }

// // snd_seq --------------------------------------------------------------------

int snd_seq_create_simple_port	(	snd_seq_t * 	seq, const char * 	name, unsigned int 	caps, unsigned int 	type )
{
	orig_snd_seq_create_simple_port = dlsym(RTLD_NEXT, "snd_seq_create_simple_port");


	// Do not allow port creation for our virtual ports
	char port_name[128];
	for ( int i = first_seqvirtual_addr ; i <= seqvirtual_addr_index ; i++  ) {
		sprintf(port_name,"Client-%d Virtual RawMIDI",i);
		if ( strcmp(port_name,name) == 0) {
			printf("(tkgl_anyctrl) Port creation disabled for : %s\n",port_name);
			return -1;
		}
	}

	// Do not allow ports creation for our device
	if ( strstr(name, getenv("ANYCTRL_NAME") ) ) return -1;

	// Add subscriptions rights to created ports, so we can use aconnect without restrictions

	//printf("snd_seq_create_simple_port %s type %u\n",name,type);
	// if (caps & SND_SEQ_PORT_CAP_READ) {
	// 	caps |= SND_SEQ_PORT_CAP_SYNC_READ | SND_SEQ_PORT_CAP_SUBS_READ;
	// }

	// if (caps & SND_SEQ_PORT_CAP_WRITE) {
	// 	caps |= SND_SEQ_PORT_CAP_SYNC_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
	// }

	return orig_snd_seq_create_simple_port(seq,name,caps,type);
}

long snd_midi_event_decode	(	snd_midi_event_t * 	dev,unsigned char * 	buf,long 	count, const snd_seq_event_t * 	ev )
{
	orig_snd_midi_event_decode = dlsym(RTLD_NEXT, "snd_midi_event_decode");

	// Disable running status. Board effect : disabled for all ports...
	snd_midi_event_no_status(dev,1);

	return orig_snd_midi_event_decode(dev,buf,count,ev);

}
