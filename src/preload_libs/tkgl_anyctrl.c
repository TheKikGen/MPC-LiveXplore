/*
__ __| |           |  /_) |     ___|             |           |
  |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
  |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
 _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
  -----------------------------------------------------------------------------
	TKGL_ANYCTRL MPC LD_PRELOAD library.
	This "low-level" library used allows you to set up any controller as a control
	surface in addition to standard hardware to drive the standalone MPC app.
	By a simple midi message mapping in your own controller, 	it is possible to
	simulate "buttons" press, and get more shortut like those of the MPC X
	(track mute, pad mixer, solo, mute, etc...) or to add more pads.

  You must create a connect script named "tkgl_anyctrl_cxscript.sh" and place
	it in the same directory than the library. The script must contain at a minimum
	the	following command to enable standard MPC controller :
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

#define MPC_PUBLIC "hw:1,0,0"
#define MPC_PRIVATE "hw:1,0,1"

// connect script file name. place it in the same directory than the lib
#define CONNECT_SCRIPT "tkgl_anyctrl_cxscript.sh"

// snd_rawmidi API

typedef int (*orig_snd_rawmidi_open_type)(snd_rawmidi_t **inputp, snd_rawmidi_t **outputp, const char *name, int mode);
static orig_snd_rawmidi_open_type orig_snd_rawmidi_open;

typedef ssize_t (*orig_snd_rawmidi_write_type) (	snd_rawmidi_t * 	rawmidi, const void * 	buffer, size_t 	size);
static orig_snd_rawmidi_write_type orig_snd_rawmidi_write;

// snd_seq API

typedef int (*orig_snd_seq_create_simple_port_type)	(	snd_seq_t * 	seq, const char * 	name, unsigned int 	caps, unsigned int 	type );
orig_snd_seq_create_simple_port_type orig_snd_seq_create_simple_port;

typedef long (*orig_snd_midi_event_decode_type)	(	snd_midi_event_t * 	dev,unsigned char * 	buf,long 	count, const snd_seq_event_t * 	ev );
orig_snd_midi_event_decode_type orig_snd_midi_event_decode;

void tkgl_banner() {
	printf( "------------------------------------\n");
	printf ("TKGL_ANYCTRL V1.0 by the KikGen Labs\n");
	printf( "------------------------------------\n");
}

int snd_rawmidi_open(snd_rawmidi_t **inputp, snd_rawmidi_t **outputp, const char *name, int mode)
{

	orig_snd_rawmidi_open = (orig_snd_rawmidi_open_type)dlsym(RTLD_NEXT,"snd_rawmidi_open");

	// Substitute the hardware private port by our virtual port
	if ( strcmp(MPC_PRIVATE,name) == 0  && inputp ) {
			 	return orig_snd_rawmidi_open(inputp, NULL, "virtual", mode);
	}

	return orig_snd_rawmidi_open(inputp, outputp, name, mode);
}

ssize_t snd_rawmidi_write(snd_rawmidi_t * 	rawmidi,const void * 	buffer,size_t 	size) {

	orig_snd_rawmidi_write = (orig_snd_rawmidi_write_type)dlsym(RTLD_NEXT,"snd_rawmidi_write");
	static int connect_ctrl = 0;

	// Connect MPC controller to our virtual private port before SYSEX id string be sent
	if (! connect_ctrl && strcmp(MPC_PUBLIC,snd_rawmidi_name(rawmidi)) == 0 ) {

			tkgl_banner();
			Dl_info dl_info;
	    dladdr((void*)tkgl_banner, &dl_info);

			char lib_path[128];
			char connect_cmd[128];

			strcpy(lib_path,dl_info.dli_fname);
			sprintf(connect_cmd,"sh %s/%s",dirname(lib_path),CONNECT_SCRIPT);
			if ( system(connect_cmd) != 0 ) {
				printf ("**** Error when lauching connect script cmd %s. Abort.\n",connect_cmd);
				exit(1);
			};
			sprintf(connect_cmd,"cat %s/%s",lib_path,CONNECT_SCRIPT);
			system(connect_cmd);
			connect_ctrl = 1;
			printf ("MPC controller port connected to virtual client.\n");
			printf( "------------------------------------\n");
	}

	return orig_snd_rawmidi_write(rawmidi,buffer,size);
}

// snd_seq --------------------------------------------------------------------

int snd_seq_create_simple_port	(	snd_seq_t * 	seq, const char * 	name, unsigned int 	caps, unsigned int 	type )
{
	// Add subscriptions rights to created ports, so we can use aconnect without restrictions
	orig_snd_seq_create_simple_port = (orig_snd_seq_create_simple_port_type)dlsym(RTLD_NEXT,"snd_seq_create_simple_port");
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
	orig_snd_midi_event_decode = (orig_snd_midi_event_decode_type)dlsym(RTLD_NEXT,"snd_midi_event_decode");
	// Disable running status. Board effect : disabled for all ports...
	snd_midi_event_no_status(dev,1);

	return orig_snd_midi_event_decode(dev,buf,count,ev);

}
