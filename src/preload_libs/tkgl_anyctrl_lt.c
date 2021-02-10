/*
__ __| |           |  /_) |     ___|             |           |
  |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
  |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
 _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
-----------------------------------------------------------------------------
TKGL_ANYCTRL   MPC LD_PRELOAD library.
This "low-level" library allows you to set up any controller as a control
surface in addition to standard hardware to drive the standalone MPC app.
By a simple midi message mapping in your own controller, it is possible to
simulate "buttons" press, and get more shortut like those of the MPC X
(track mute, pad mixer, solo, mute, etc...) or to add more pads or qlinks.

The first port of your controller will be masked in the application
if you use it with anyctrl.

NB : some specific midi messages crash the MPC application. This is not
linked with this library, but probably likely due to a bug in the application
that does not correctly filter messages according to the context.

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
#include <regex.h>

// MPC Controller names
#define CTRL_FORCE "Akai Pro Force"
#define CTRL_MPC_X "MPC X Controller"
#define CTRL_MPC_LIVE "MPC Live Controller"
#define CTRL_MPC_ALL ".*(MPC.*Controller|Akai Pro Force).*"


// Function prototypes

// Globals

// MPC alsa informations
static int  mpc_midi_card = -1;
static int  mpc_seq_client = -1;
static char mpc_midi_private_alsa_name[20];
static char mpc_midi_public_alsa_name[20];

// Midi controller seq client
static int seqanyctrl_client=-1;
static int seqanyctrl_firstport = 1;

// Virtual rawmidi pointers
static snd_rawmidi_t *rawvirt_inpriv  = NULL;

// Sequencers virtual client addresses
static int seqvirt_client_inpriv  = -1;

// Alsa API
static typeof(&snd_rawmidi_open) orig_snd_rawmidi_open;
static typeof(&snd_rawmidi_read) orig_snd_rawmidi_read;
static typeof(&snd_rawmidi_write) orig_snd_rawmidi_write;
static typeof(&snd_seq_create_simple_port) orig_snd_seq_create_simple_port;
static typeof(&snd_midi_event_decode) orig_snd_midi_event_decode;

///////////////////////////////////////////////////////////////////////////////
// Match string against a regular expression
///////////////////////////////////////////////////////////////////////////////
int match(const char *string, char *pattern)
{
    int    status;
    regex_t    re;

    if (regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB) != 0) {
        return(0);      /* Report error. */
    }
    status = regexec(&re, string, (size_t) 0, NULL, 0);
    regfree(&re);
    if (status != 0) {
        return(0);      /* Report error. */
    }
    return(1);
}

///////////////////////////////////////////////////////////////////////////////
// Get an ALSA sequencer client containing a name
///////////////////////////////////////////////////////////////////////////////
int GetSeqClientFromPortName(const char * name) {

	if ( name == NULL) return -1;
	char port_name[128];

	snd_seq_t *seq;
	if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		fprintf(stderr,"(tkgl_anyctrl) impossible to open default seq\n");
		return -1;
	}

	snd_seq_client_info_t *cinfo;
	snd_seq_port_info_t *pinfo;

	snd_seq_client_info_alloca(&cinfo);
	snd_seq_port_info_alloca(&pinfo);
	snd_seq_client_info_set_client(cinfo, -1);
	while (snd_seq_query_next_client(seq, cinfo) >= 0) {
		/* reset query info */
		snd_seq_port_info_set_client(pinfo, snd_seq_client_info_get_client(cinfo));
		snd_seq_port_info_set_port(pinfo, -1);
		while (snd_seq_query_next_port(seq, pinfo) >= 0) {
			sprintf(port_name,"%s %s",snd_seq_client_info_get_name(cinfo),snd_seq_port_info_get_name(pinfo));
			if (strstr(port_name,name)) {
				snd_seq_close(seq);
				return snd_seq_client_info_get_client(cinfo) ;
			}
		}
	}

	snd_seq_close(seq);
	return  -1;
}

///////////////////////////////////////////////////////////////////////////////
// Get the last ALSA sequencer client
///////////////////////////////////////////////////////////////////////////////
int GetLastSeqClient() {

	int r = -1;

	snd_seq_t *seq;
	if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		fprintf(stderr,"(tkgl_anyctrl) impossible to open default seq\n");
		return -1;
	}

	snd_seq_client_info_t *cinfo;
	snd_seq_port_info_t *pinfo;

	snd_seq_client_info_alloca(&cinfo);
	snd_seq_port_info_alloca(&pinfo);
	snd_seq_client_info_set_client(cinfo, -1);
	while (snd_seq_query_next_client(seq, cinfo) >= 0) {
		/* reset query info */
		snd_seq_port_info_set_client(pinfo, snd_seq_client_info_get_client(cinfo));
		snd_seq_port_info_set_port(pinfo, -1);
		while (snd_seq_query_next_port(seq, pinfo) >= 0) {
				r = snd_seq_client_info_get_client(cinfo) ;
		}
	}

	snd_seq_close(seq);
	return  r;
}

///////////////////////////////////////////////////////////////////////////////
// Get an ALSA card from a matching regular expression pattern
///////////////////////////////////////////////////////////////////////////////
int GetCardFromShortName(char *pattern) {
   int card = -1;
   char* shortname = NULL;

   if ( snd_card_next(&card) < 0) return -1;
   while (card >= 0) {
   	  if ( snd_card_get_name(card, &shortname) == 0 && match(shortname,pattern) ) return card;
   	  if ( snd_card_next(&card) < 0) break;
   }
   return -1;
}

///////////////////////////////////////////////////////////////////////////////
// ALSA aconnect utility API equivalent
///////////////////////////////////////////////////////////////////////////////
int aconnect(int src_client, int src_port, int dest_client, int dest_port) {
	int queue = 0, convert_time = 0, convert_real = 0, exclusive = 0;
	snd_seq_port_subscribe_t *subs;
	snd_seq_addr_t sender, dest;
	int client;
	char addr[10];

	snd_seq_t *seq;
	if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		fprintf(stderr,"(tkgl_anyctrl) impossible to open default seq\n");
		return -1;
	}

	if ((client = snd_seq_client_id(seq)) < 0) {
		fprintf(stderr,"(tkgl_anyctrl) impossible to get seq client id\n");
		snd_seq_close(seq);
		return - 1;
	}

	/* set client info */
	if (snd_seq_set_client_name(seq, "ALSA Connector") < 0) {
		fprintf(stderr,"(tkgl_anyctrl) set client name failed\n");
		snd_seq_close(seq);
		return -1;
	}

	/* set subscription */
	sprintf(addr,"%d:%d",src_client,src_port);
	if (snd_seq_parse_address(seq, &sender, addr) < 0) {
		snd_seq_close(seq);
		fprintf(stderr,"(tkgl_anyctrl) invalid source address %s\n", addr);
		return -1;
	}

	sprintf(addr,"%d:%d",dest_client,dest_port);
	if (snd_seq_parse_address(seq, &dest, addr) < 0) {
		snd_seq_close(seq);
		fprintf(stderr,"(tkgl_anyctrl) invalid destination address %s\n", addr);
		return -1;
	}

	snd_seq_port_subscribe_alloca(&subs);
	snd_seq_port_subscribe_set_sender(subs, &sender);
	snd_seq_port_subscribe_set_dest(subs, &dest);
	snd_seq_port_subscribe_set_queue(subs, queue);
	snd_seq_port_subscribe_set_exclusive(subs, exclusive);
	snd_seq_port_subscribe_set_time_update(subs, convert_time);
	snd_seq_port_subscribe_set_time_real(subs, convert_real);

	fprintf(stdout,"(tkgl_anyctrl) connection %d:%d to %d:%d",src_client,src_port,dest_client,dest_port);
	if (snd_seq_get_port_subscription(seq, subs) == 0) {
		snd_seq_close(seq);
		fprintf(stdout," already subscribed\n");
		return 0;
	}
	if (snd_seq_subscribe_port(seq, subs) < 0) {
		snd_seq_close(seq);
		fprintf(stderr," failed !\n");
		return 1;
	}

	fprintf(stdout," successfull\n");

	snd_seq_close(seq);
}


// void print_binary(unsigned int number)
// {
//     if (number >> 1) {
//         print_binary(number >> 1);
//     }
//     putc((number & 1) ? '1' : '0', stdout);
// }

///////////////////////////////////////////////////////////////////////////////
// Setup tkgl anyctrl
///////////////////////////////////////////////////////////////////////////////
static void tkgl_init()
{
	fprintf(stdout,"------------------------------------------\n");
	fprintf(stdout,"TKGL_ANYCTRL LITE  V1.0 by the KikGen Labs\n");
	fprintf(stdout,"------------------------------------------\n");

	// Alsa hooks
	orig_snd_rawmidi_open              = dlsym(RTLD_NEXT, "snd_rawmidi_open");
	orig_snd_rawmidi_read              = dlsym(RTLD_NEXT,"snd_rawmidi_read");
	orig_snd_rawmidi_write             = dlsym(RTLD_NEXT, "snd_rawmidi_write");
	orig_snd_seq_create_simple_port    = dlsym(RTLD_NEXT, "snd_seq_create_simple_port");
	orig_snd_midi_event_decode         = dlsym(RTLD_NEXT, "snd_midi_event_decode");

	// Initialize card id for public and private
	mpc_midi_card = GetCardFromShortName(CTRL_MPC_ALL);
	if ( mpc_midi_card < 0 ) {
			fprintf(stderr,"(tkgl_anyctrl) **** Error : MPC card not found\n");
			exit(1);
	}

	// Get MPC seq
	// Public is port 0, Private is port 1
	mpc_seq_client = GetSeqClientFromPortName("Private");

	if ( mpc_seq_client  < 0 ) {
		fprintf(stderr,"(tkgl_anyctrl) **** Error : MPC seq client not found\n");
		exit(1);
	}

	sprintf(mpc_midi_private_alsa_name,"hw:%d,0,1",mpc_midi_card);
	sprintf(mpc_midi_public_alsa_name,"hw:%d,0,0",mpc_midi_card);
	fprintf(stdout,"(tkgl_anyctrl) MPC card id hw:%d found\n",mpc_midi_card);
	fprintf(stdout,"(tkgl_anyctrl) MPC Private port is %s\n",mpc_midi_private_alsa_name);
	fprintf(stdout,"(tkgl_anyctrl) MPC Public port is %s\n",mpc_midi_public_alsa_name);
	fprintf(stdout,"(tkgl_anyctrl) MPC seq client is %d\n",mpc_seq_client);


  // Get our controller seq  port client
	const char * port_name = getenv("ANYCTRL_NAME") ;
	if ( port_name == NULL) {
		fprintf(stdout,"(tkgl_anyctrl) ANYCTRL_NAME environment variable not set.\n") ;
	}
	else {
		// Initialize card id for public and private
		seqanyctrl_client = GetSeqClientFromPortName(port_name);
		if ( seqanyctrl_client  < 0 )
			fprintf(stderr,"(tkgl_anyctrl) **** Warning : %s seq client not found\n",port_name);
    else
      fprintf(stdout,"(tkgl_anyctrl) %s connect port is %d:0\n",port_name,seqanyctrl_client);
	}

	// Create 1 IN virtual port for Private IN
	// This will trig snd_seq_create_simple_port for the first time outside MPC app

	// Private In
	if ( orig_snd_rawmidi_open(&rawvirt_inpriv, NULL, "virtual", 2) == 0 ) {
		seqvirt_client_inpriv = GetLastSeqClient();
		fprintf(stdout,"(tkgl_anyctrl) Virtual private input port %d created.\n",seqvirt_client_inpriv);
	}

	// Make connections of our virtuals ports

	// Private MPC controller port out 1 to virtual In 0
	aconnect(	mpc_seq_client, 1, seqvirt_client_inpriv, 0);

  // Connect our controller if used
	if (seqanyctrl_client >= 0) {
		// port 0 to virtual In 0,
		aconnect(	seqanyctrl_client, 0, seqvirt_client_inpriv, 0);
	}

	fflush(stdout);
}

///////////////////////////////////////////////////////////////////////////////
// MPC Main hook
///////////////////////////////////////////////////////////////////////////////
int __libc_start_main(
    int (*main)(int, char **, char **),
    int argc,
    char **argv,
    int (*init)(int, char **, char **),
    void (*fini)(void),
    void (*rtld_fini)(void),
    void *stack_end)
{

    /* Find the real __libc_start_main()... */
    typeof(&__libc_start_main) orig = dlsym(RTLD_NEXT, "__libc_start_main");

  	tkgl_init();

    /* ... and call it with our custom main function */
    return orig(main, argc, argv, init, fini, rtld_fini, stack_end);
}

///////////////////// ALSA API HOOKS //////////////////////////////////////////

int snd_rawmidi_open(snd_rawmidi_t **inputp, snd_rawmidi_t **outputp, const char *name, int mode)
{

	//fprintf(stdout,"(tkgl_anyctrl) snd_rawmidi_open name %s mode %d\n",name,mode);

	// Substitute the hardware private input port by our input virtual ports

	if ( strcmp(mpc_midi_private_alsa_name,name) == 0 && inputp   ) {

		// Substitue Private In by our virtual in
		*inputp = *inputp = rawvirt_inpriv;
		fprintf(stdout,"(tkgl_anyctrl) %s substitution by virtual rawmidi successfull\n",name);
		return 0;
	}

	return orig_snd_rawmidi_open(inputp, outputp, name, mode);
}

int snd_seq_create_simple_port	(	snd_seq_t * 	seq, const char * 	name, unsigned int 	caps, unsigned int 	type )
{
  // fprintf(stdout,"(tkgl_anyctrl) create simple port %s  type ",name);
  // print_binary(type);
  // fprintf(stdout,"\n");

  // If the port is the first of our controller then do not allow creation
  if ( seqanyctrl_client >= 0 && seqanyctrl_firstport && strstr(name, getenv("ANYCTRL_NAME"))  ) {
      seqanyctrl_firstport = 0;
      fprintf(stdout,"(tkgl_anyctrl) Port creation disabled for first port of : %s\n",name);
		  return -1;
	}

  // Do not allow MPC to create virtual connection to our virtual ports
	else if ( match(name,"Client-.* Virtual RawMIDI" )) {
		fprintf(stdout,"(tkgl_anyctrl) Port creation disabled for : %s\n",name);
		return -1;
	}

	return orig_snd_seq_create_simple_port(seq,name,caps,type);
}

long snd_midi_event_decode	(	snd_midi_event_t * 	dev,unsigned char * 	buf,long 	count, const snd_seq_event_t * 	ev )
{

	// Disable running status. Board effect : disabled for all ports...
	snd_midi_event_no_status(dev,1);

	return orig_snd_midi_event_decode(dev,buf,count,ev);

}
