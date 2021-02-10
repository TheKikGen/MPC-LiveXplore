/*
__ __| |           |  /_) |     ___|             |           |
  |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
  |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
 _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
-----------------------------------------------------------------------------
TKGL_IAMFORCECTRL  LD_PRELOAD library.
This "low-level" library allows you to hijack the MPC Force application with
an MPC Live as a "fake" Force controller.
All sysex are reinterpreted, as the inmusic internal identification id.
I also used a customize Smartpad firmware to manage 8x8 pads with colors feedback.

Buy a Force if you like it. It is by far, a better experience, and no more
expensive than buying a MPC Live + a 64 pads midi controller !!!

Consider this as a proof of concept.  no more.

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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// MPC Controller names (regexp)
#define CTRL_FORCE "Akai Pro Force"
#define CTRL_MPC_X "MPC X Controller"
#define CTRL_MPC_LIVE "MPC Live Controller"
#define CTRL_MPC_ALL ".*(MPC.*Controller|Akai Pro Force).*"


// Function prototypes

// Globals

// Internal MPC product sysex id
enum MPCIds {
    MPC_X,MPC_LIVE,MPC_FORCE,MPC_ONE,MPC_LIVE_MK2,_END_MPCID
};
const uint8_t MPCSysexId[] = {0x3a,0x3b,0x40,0x46,0x47};
const char * MPCProductString[] = {
	"MPC X", 	"MPC Live", 	"Force", 	"MPC One", 	"MPC Live Mk II"
};

// Sysex patterns. 0xFF means any value.  That sysex will be amended with the
// fake Force sysex id in rawmidi write function.
static const uint8_t AkaiSysex[] = {0xF0,0x47, 0x7F};

// Identity request (universal sysex)
static const uint8_t IdentityReplySysexHeader[] = {0xF0,0x7E,0x00,0x06,0x02,0x47};

// What the Force is supposed to reply
static const uint8_t IdentityReplySysexForce[]  = {0xF0,0x7E,0x00,0x06,0x02,0x47,0x40,0x00,0x19,0x00,0x00,0x04,0x03};

// What the Live replies
static const uint8_t IdentityReplySysexLive[]   = {0xF0,0x7E,0x00,0x06,0x02,0x47,0x3B,0x00,0x19,0x00,0x01,0x01,0x01};

// Our MPC sysex id , detected after idetity request
static uint8_t MPCId = 0;

// Internal product code that will be changed on the fly when the file wille be opened
static int product_code_handler = -1 ;
static uint8_t product_code_found = 0;

// Product codes to simulate what we need to
const char * ProductCodeForce = "ADA2";
const char * ProductCodeLive  = "ACV8";

// MPC alsa informations
static int  mpc_midi_card = -1;
static int  mpc_seq_client = -1;
static char mpc_midi_private_alsa_name[20];
static char mpc_midi_public_alsa_name[20];

// Midi controller seq client
static int seqanyctrl_client=-1;

// Virtual rawmidi pointers
static snd_rawmidi_t *rawvirt_inpriv  = NULL;
static snd_rawmidi_t *rawvirt_outpriv = NULL ;
static snd_rawmidi_t *rawvirt_outpub  = NULL ;

// Sequencers virtual client addresses
static int seqvirt_client_inpriv  = -1;
static int seqvirt_client_outpriv = -1;
static int seqvirt_client_outpub  = -1;

// Alsa API
static typeof(&snd_rawmidi_open) orig_snd_rawmidi_open;
static typeof(&snd_rawmidi_read) orig_snd_rawmidi_read;
static typeof(&snd_rawmidi_write) orig_snd_rawmidi_write;
static typeof(&snd_seq_create_simple_port) orig_snd_seq_create_simple_port;
static typeof(&snd_midi_event_decode) orig_snd_midi_event_decode;


// Other more generic APIs
static typeof(&open64) orig_open64;
static typeof(&read) orig_read;
//static typeof(&open) orig_open;

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


///////////////////////////////////////////////////////////////////////////////
// Get MPC hardware name from sysex id
///////////////////////////////////////////////////////////////////////////////

static int GetIndexOfMPCId(uint8_t id){
	for (int i = 0 ; i < sizeof(MPCSysexId) ; i++ )
		if ( MPCSysexId[i] == id ) return i;
	return -1;
}

const char * GetHwNameFromMPCId(uint8_t id){
	int i = GetIndexOfMPCId(id);
	if ( i >= 0) return MPCProductString[i];
	else return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Setup tkgl anyctrl
///////////////////////////////////////////////////////////////////////////////
static void tkgl_init()
{
	fprintf(stdout,"------------------------------------\n");
	fprintf (stdout,"TKGL_IAMFORCE V1.0 by the KikGen Labs\n");
	fprintf(stdout,"------------------------------------\n");


  orig_open64 = dlsym(RTLD_NEXT, "open64");
  //orig_open = dlsym(RTLD_NEXT, "open");
  orig_read = dlsym(RTLD_NEXT, "read");

	// Alsa hooks
	orig_snd_rawmidi_open = dlsym(RTLD_NEXT, "snd_rawmidi_open");
	orig_snd_rawmidi_read = dlsym(RTLD_NEXT,"snd_rawmidi_read");
	orig_snd_rawmidi_write = dlsym(RTLD_NEXT, "snd_rawmidi_write");
	orig_snd_seq_create_simple_port = dlsym(RTLD_NEXT, "snd_seq_create_simple_port");
	orig_snd_midi_event_decode = dlsym(RTLD_NEXT, "snd_midi_event_decode");

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
		if ( seqanyctrl_client  < 0 ) {
			fprintf(stderr,"(tkgl_anyctrl) **** Error : %s seq client not found\n",port_name);
			exit(1);
		}
		fprintf(stdout,"(tkgl_anyctrl) %s connect port is %d:0\n",port_name,seqanyctrl_client);
	}

	// Create 3 virtuals ports : Private I/O, Public O
	// This will trig snd_seq_create_simple_port for the first time outside MPC app

	// Private In
	if ( orig_snd_rawmidi_open(&rawvirt_inpriv, NULL, "virtual", 2) == 0 ) {
		seqvirt_client_inpriv = GetLastSeqClient();
		fprintf(stdout,"(tkgl_anyctrl) Virtual private input port %d created.\n",seqvirt_client_inpriv);
	}

	// Private Out
	if ( orig_snd_rawmidi_open(NULL, &rawvirt_outpriv, "virtual", 3) == 0) {
		seqvirt_client_outpriv = GetLastSeqClient();
		fprintf(stdout,"(tkgl_anyctrl) Virtual private output port %d created.\n",seqvirt_client_outpriv);
	}

	// Public Out
	if ( orig_snd_rawmidi_open(NULL, &rawvirt_outpub,  "virtual", 3) == 0) {
		seqvirt_client_outpub =  GetLastSeqClient();
		fprintf(stdout,"(tkgl_anyctrl) Virtual public output port %d created.\n",seqvirt_client_outpub);
	}

	if ( seqvirt_client_inpriv < 0 || seqvirt_client_outpriv < 0 || seqvirt_client_outpub < 0 ) {
		fprintf(stderr,"(tkgl_anyctrl) **** Error : impossible to create virtual ports\n");
		exit(1);
	}

	// Make connections of our virtuals ports

	// Private MPC controller port 1 to virtual In 0
	aconnect(	mpc_seq_client, 1, seqvirt_client_inpriv, 0);

	// Virtual out priv 0 to Private MPC controller port 1
	aconnect(	seqvirt_client_outpriv, 0, mpc_seq_client, 1);

	// Virtual out public to Public MPC controller port 1
	aconnect(	seqvirt_client_outpub, 0, mpc_seq_client, 0);

	// Connect our controller if used
	if (seqanyctrl_client >= 0) {
		// port 0 to virtual In 0,
		aconnect(	seqanyctrl_client, 0, seqvirt_client_inpriv, 0);

		// Virtual out priv 0 to our controller port 0
		aconnect(	seqvirt_client_outpriv, 0, seqanyctrl_client, 0);

		// Virtual out public to our controller port 0
		aconnect(	seqvirt_client_outpub, 0, seqanyctrl_client, 0);
	}


	fflush(stdout);
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
		//		if ( c == 0 && idx > 0) fprintf(stdout,"                        ");
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

    // Find the real __libc_start_main()...
    typeof(&__libc_start_main) orig = dlsym(RTLD_NEXT, "__libc_start_main");


    // Initialize everything
    tkgl_init();

    // ... and call main again
    return orig(main, argc, argv, init, fini, rtld_fini, stack_end);
}

///////////////////// ALSA API HOOKS //////////////////////////////////////////

int snd_rawmidi_open(snd_rawmidi_t **inputp, snd_rawmidi_t **outputp, const char *name, int mode)
{

	fprintf(stdout,"(tkgl_anyctrl) snd_rawmidi_open name %s mode %d\n",name,mode);

	// Substitute the hardware private input port by our input virtual ports

	if ( strcmp(mpc_midi_private_alsa_name,name) == 0   ) {

		// Private In
		if (inputp) *inputp = rawvirt_inpriv;
		else if (outputp) *outputp = rawvirt_outpriv ;
		else return -1;
		fprintf(stdout,"(tkgl_anyctrl) %s substitution by virtual rawmidi successfull\n",name);

		return 0;
	}

	else if ( strcmp(mpc_midi_public_alsa_name,name) == 0   ) {

		if (outputp) *outputp = rawvirt_outpub;
		else return -1;
		fprintf(stdout,"(tkgl_anyctrl) %s substitution by virtual rawmidi successfull\n",name);

		return 0;
	}

	return orig_snd_rawmidi_open(inputp, outputp, name, mode);
}


///////////////////////////////////////////////////////////////////////////////
// Alsa Rawmidi read
///////////////////////////////////////////////////////////////////////////////
ssize_t snd_rawmidi_read(snd_rawmidi_t *rawmidi, void *buffer, size_t size) {

	int r = orig_snd_rawmidi_read(rawmidi, buffer, size);
  uint8_t * myBuff = (uint8_t*)buffer;

  for ( int i = 0 ; i < size ; i++ ) {

    // Check if identity request ? (could be optimized....)
		if (  myBuff[i] == 0xF0 &&  memcmp(&myBuff[i],IdentityReplySysexLive,sizeof(IdentityReplySysexLive)) == 0 ) {
        MPCId = myBuff[i + 6];

        // If so, substitue sysex identity request by the Force one
        memcpy(&myBuff[i],IdentityReplySysexForce, sizeof(IdentityReplySysexForce));
        i += sizeof(IdentityReplySysexForce) ;
		}
	}

	return r;
}

///////////////////////////////////////////////////////////////////////////////
// Alsa Rawmidi write
///////////////////////////////////////////////////////////////////////////////
ssize_t snd_rawmidi_write(snd_rawmidi_t * 	rawmidi,const void * 	buffer,size_t 	size) {
	uint8_t * myBuff = (uint8_t*)buffer;

	for ( int i = 0 ; i < size ; i++ ) {
    // If we detect the Akai sysex header, change the harwware id by our true hardware id.
    // Messages are compatibles. Some Force midi msg are not interpreted (e.g. Oled)
		if (  myBuff[i] == 0xF0 &&  memcmp(&myBuff[i],AkaiSysex,sizeof(AkaiSysex)) == 0 ) {
        // Update the sysex id in the sysex
				myBuff[i+3] = MPCId;
        i += sizeof(AkaiSysex) ;
		}
	}

	return 	orig_snd_rawmidi_write(rawmidi, buffer, size);

}

///////////////////////////////////////////////////////////////////////////////
// Alsa create a siple seq port
///////////////////////////////////////////////////////////////////////////////
int snd_seq_create_simple_port	(	snd_seq_t * 	seq, const char * 	name, unsigned int 	caps, unsigned int 	type )
{

	// We do not allow ports creation for our device or our virtuals ports
  // Because this could lead to infinite midi loop in the MPC midi settings
	if ( seqanyctrl_client >= 0 && strstr(name, getenv("ANYCTRL_NAME") ) ) {
		fprintf(stdout,"(tkgl_anyctrl) Port creation disabled for : %s\n",name);
		return -1;
	}
	else if ( match(name,"Client-.* Virtual RawMIDI" )) {
		fprintf(stdout,"(tkgl_anyctrl) Port creation disabled for : %s\n",name);
		return -1;
	}

	return orig_snd_seq_create_simple_port(seq,name,caps,type);
}

///////////////////////////////////////////////////////////////////////////////
// Decode a midi seq event
///////////////////////////////////////////////////////////////////////////////
long snd_midi_event_decode	(	snd_midi_event_t * 	dev,unsigned char * 	buf,long 	count, const snd_seq_event_t * 	ev )
{

	// Disable running status to be a true "raw" midi. Board effect : disabled for all ports...
	snd_midi_event_no_status(dev,1);

	return orig_snd_midi_event_decode(dev,buf,count,ev);

}


///////////////////////////////////////////////////////////////////////////////
// open64
///////////////////////////////////////////////////////////////////////////////
// We intercept all file opening until we found inmusic,product-code in the path.
// We could do the same for serial number, or panel/touch orientaton.

int open64(const char *pathname, int flags,...) {
   int r;

   // If O_CREAT is used to create a file, the file access mode must be given.
   if (flags & O_CREAT) {
       va_list args;
       va_start(args, flags);
       int mode = va_arg(args, int);
       va_end(args);
       r =  orig_open64(pathname, flags, mode);
   } else {
       r = orig_open64(pathname, flags);
   }

   if ( product_code_handler < 0 && strcmp(pathname,"/sys/firmware/devicetree/base/inmusic,product-code") == 0 ) {
     // Save the product code file descriptor
     product_code_handler = r;
   }

   return r ;
}

// int open(const char *pathname, int flags,...) {
//
//    // If O_CREAT is used to create a file, the file access mode must be given.
//    if (flags & O_CREAT) {
//        va_list args;
//        va_start(args, flags);
//        int mode = va_arg(args, int);
//        va_end(args);
//        return orig_open(pathname, flags, mode);
//    } else {
//        return orig_open(pathname, flags);
//    }
// }

///////////////////////////////////////////////////////////////////////////////
// read
///////////////////////////////////////////////////////////////////////////////
ssize_t read(int fildes, void *buf, size_t nbyte) {

  // Not yet
  if ( product_code_handler < 0 )  return orig_read(fildes,buf,nbyte);

  // If we got the file descriptor, we can swap the product code with the Force one.
  if ( fildes == product_code_handler ) {
    memcpy(buf,ProductCodeForce,sizeof(ProductCodeForce) +1 );
    product_code_handler = -1;
    return nbyte;
  }


}
