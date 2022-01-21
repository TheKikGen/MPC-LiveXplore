/*
__ __| |           |  /_) |     ___|             |           |
  |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
  |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
 _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
-----------------------------------------------------------------------------
TKGL_MPCMAPPER  LD_PRELOAD library.
This "low-level" library allows you to hijack the MPC/Force application to add
your own midi mapping to input and output midi messages.

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

// Product code file
#define PRODUCT_CODE_PATH "/sys/firmware/devicetree/base/inmusic,product-code"

// Function prototypes

// Globals


// Raw midi dump flag
int rawMidiDumpFlag = 0 ;
int rawMidiDumpPostFlag = 0 ; // After our tranformation

// End user virtual port name
static char *user_virtual_portname = NULL;

// End user virtual port handles
static snd_rawmidi_t *rawvirt_user_in    = NULL ;
static snd_rawmidi_t *rawvirt_user_out   = NULL ;

// Internal MPC product sysex id
enum MPCIds {MPC_X, MPC_LIVE,  MPC_FORCE,MPC_ONE,MPC_LIVE_MK2,_END_MPCID };
const uint8_t MPCSysexId[]      = {  0x3a,   0x3b,       0x40,    0x46,      0x47};
const char * MPCProductString[] = { "MPC X", "MPC Live", "Force",	"MPC One", "MPC Live Mk II" };
const char * MPCProductCode[]   = {	"ACV5",  "ACV8", 	   "ADA2", 	"ACVA", 	 "ACVB" };
const uint8_t MPCIdentityReplySysex[][7] = {
  {0x3A,0x00,0x19,0x00,0x01,0x01,0x01}, // X
  {0x3B,0x00,0x19,0x00,0x01,0x01,0x01}, // Live
  {0x40,0x00,0x19,0x00,0x00,0x04,0x03}, // Force
  {0x46,0x00,0x19,0x00,0x01,0x01,0x01}, // One
  {0x47,0x00,0x19,0x00,0x01,0x01,0x01}, // Live MkII
};

// Sysex patterns. 0xFF means any value.  That sysex will be amended with the
// fake Force sysex id in rawmidi write function.
static const uint8_t AkaiSysex[] = {0xF0,0x47, 0x7F};

// Header of an Akai identity reply
static const uint8_t IdentityReplySysexHeader[]   = {0xF0,0x7E,0x00,0x06,0x02,0x47};

// Our MPC product id (index in the table)
static int MPCOriginalId = -1;
// The one used
static int MPCId = -1;
// and the spoofed one,
static int MPCIdSpoofed = -1;

// Internal product code file handler to change on the fly when the file will be opened
static int product_code_file_handler = -1 ;

// MPC alsa informations
static int  mpc_midi_card = -1;
static int  mpc_seq_client = -1;
static char mpc_midi_private_alsa_name[20];
static char mpc_midi_public_alsa_name[20];

// Midi our controller seq client
static int seqanyctrl_client=-1;
static snd_rawmidi_t *rawmidi_inanyctrl  = NULL;
static snd_rawmidi_t *rawmidi_outanyctrl = NULL;
static int  anyctrl_midi_card = -1;
static char * anyctrl_name = NULL;

// Virtual rawmidi pointers to fake the MPC app
static snd_rawmidi_t *rawvirt_inpriv  = NULL;
static snd_rawmidi_t *rawvirt_outpriv = NULL ;
static snd_rawmidi_t *rawvirt_outpub  = NULL ;

// Sequencers virtual client addresses
static int seqvirt_client_inpriv  = -1;
static int seqvirt_client_outpriv = -1;
static int seqvirt_client_outpub  = -1;

// Alsa API hooks declaration
static typeof(&snd_rawmidi_open) orig_snd_rawmidi_open;
static typeof(&snd_rawmidi_read) orig_snd_rawmidi_read;
static typeof(&snd_rawmidi_write) orig_snd_rawmidi_write;
static typeof(&snd_seq_create_simple_port) orig_snd_seq_create_simple_port;
static typeof(&snd_midi_event_decode) orig_snd_midi_event_decode;
static typeof(&snd_seq_open) orig_snd_seq_open;
static typeof(&snd_seq_port_info_set_name) orig_snd_seq_port_info_set_name;
static typeof(&snd_seq_event_input) orig_snd_seq_event_input;

// Globals used to rename a virtual port and get the client id.  No other way...
static int  snd_seq_virtual_port_rename_flag  = 0;
static char snd_seq_virtual_port_newname [30];
static int  snd_seq_virtual_port_clientid=-1;

// Other more generic APIs
static typeof(&open64) orig_open64;
static typeof(&read) orig_read;
static typeof(&open) orig_open;

///////////////////////////////////////////////////////////////////////////////
// Match string against a regular expression
///////////////////////////////////////////////////////////////////////////////
int match(const char *string, const char *pattern)
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
	if (orig_snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		fprintf(stderr,"[tkgl]  *** Error : impossible to open default seq\n");
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
			if (strstr(port_name,name) != NULL) {
        int r = snd_seq_client_info_get_client(cinfo);
				snd_seq_close(seq);
				return r ;
			}
		}
	}

	snd_seq_close(seq);
	return  -1;
}


///////////////////////////////////////////////////////////////////////////////
// Get the last port ALSA sequencer client
///////////////////////////////////////////////////////////////////////////////
int GetLastPortSeqClient() {

	int r = -1;

	snd_seq_t *seq;
	if (orig_snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		fprintf(stderr,"[tkgl]  *** Error : impossible to open default seq\n");
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
        //fprintf(stdout,"[tkgl]  client %s -- %d port %s %d\n",snd_seq_client_info_get_name(cinfo) ,r, snd_seq_port_info_get_name(pinfo), snd_seq_port_info_get_port(pinfo) );
		}
	}

	snd_seq_close(seq);
	return  r;
}

///////////////////////////////////////////////////////////////////////////////
// Get an ALSA card from a matching regular expression pattern
///////////////////////////////////////////////////////////////////////////////
int GetCardFromShortName(const char *pattern) {
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
	if (orig_snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		fprintf(stderr,"[tkgl]  *** Error : impossible to open default seq\n");
		return -1;
	}

	if ((client = snd_seq_client_id(seq)) < 0) {
		fprintf(stderr,"[tkgl]  *** Error : impossible to get seq client id\n");
		snd_seq_close(seq);
		return - 1;
	}

	/* set client info */
	if (snd_seq_set_client_name(seq, "ALSA Connector") < 0) {
		fprintf(stderr,"[tkgl]  *** Error : set client name failed\n");
		snd_seq_close(seq);
		return -1;
	}

	/* set subscription */
	sprintf(addr,"%d:%d",src_client,src_port);
	if (snd_seq_parse_address(seq, &sender, addr) < 0) {
		snd_seq_close(seq);
		fprintf(stderr,"[tkgl]  *** Error : invalid source address %s\n", addr);
		return -1;
	}

	sprintf(addr,"%d:%d",dest_client,dest_port);
	if (snd_seq_parse_address(seq, &dest, addr) < 0) {
		snd_seq_close(seq);
		fprintf(stderr,"[tkgl]  *** Error : invalid destination address %s\n", addr);
		return -1;
	}

	snd_seq_port_subscribe_alloca(&subs);
	snd_seq_port_subscribe_set_sender(subs, &sender);
	snd_seq_port_subscribe_set_dest(subs, &dest);
	snd_seq_port_subscribe_set_queue(subs, queue);
	snd_seq_port_subscribe_set_exclusive(subs, exclusive);
	snd_seq_port_subscribe_set_time_update(subs, convert_time);
	snd_seq_port_subscribe_set_time_real(subs, convert_real);

	fprintf(stdout,"[tkgl]  connection %d:%d to %d:%d",src_client,src_port,dest_client,dest_port);
	if (snd_seq_get_port_subscription(seq, subs) == 0) {
		snd_seq_close(seq);
		fprintf(stdout," already subscribed\n");
		return 0;
	}
	if (snd_seq_subscribe_port(seq, subs) < 0) {
		snd_seq_close(seq);
		fprintf(stdout," failed !\n");
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

  // System call hooks
  orig_open64 = dlsym(RTLD_NEXT, "open64");
  orig_open   = dlsym(RTLD_NEXT, "open");
  orig_read   = dlsym(RTLD_NEXT, "read");

	// Alsa hooks
	orig_snd_rawmidi_open           = dlsym(RTLD_NEXT, "snd_rawmidi_open");
	orig_snd_rawmidi_read           = dlsym(RTLD_NEXT, "snd_rawmidi_read");
	orig_snd_rawmidi_write          = dlsym(RTLD_NEXT, "snd_rawmidi_write");
	orig_snd_seq_create_simple_port = dlsym(RTLD_NEXT, "snd_seq_create_simple_port");
	orig_snd_midi_event_decode      = dlsym(RTLD_NEXT, "snd_midi_event_decode");
  orig_snd_seq_open               = dlsym(RTLD_NEXT, "snd_seq_open");
  orig_snd_seq_port_info_set_name = dlsym(RTLD_NEXT, "snd_seq_port_info_set_name");
  orig_snd_seq_event_input        = dlsym(RTLD_NEXT, "snd_seq_event_input");

	// Initialize card id for public and private
	mpc_midi_card = GetCardFromShortName(CTRL_MPC_ALL);
	if ( mpc_midi_card < 0 ) {
			fprintf(stderr,"[tkgl]  **** Error : MPC controller card not found\n");
			exit(1);
	}

	// Get MPC seq
	// Public is port 0, Private is port 1
	mpc_seq_client = GetSeqClientFromPortName("Private");
	if ( mpc_seq_client  < 0 ) {
		fprintf(stderr,"[tkgl]  **** Error : MPC controller seq client not found\n");
		exit(1);
	}

	sprintf(mpc_midi_private_alsa_name,"hw:%d,0,1",mpc_midi_card);
	sprintf(mpc_midi_public_alsa_name,"hw:%d,0,0",mpc_midi_card);
	fprintf(stdout,"[tkgl]  MPC controller card id hw:%d found\n",mpc_midi_card);
	fprintf(stdout,"[tkgl]  MPC controller Private port is %s\n",mpc_midi_private_alsa_name);
	fprintf(stdout,"[tkgl]  MPC controller Public port is %s\n",mpc_midi_public_alsa_name);
	fprintf(stdout,"[tkgl]  MPC controller seq client is %d\n",mpc_seq_client);

	// Get our controller seq  port client
	//const char * port_name = getenv("ANYCTRL_NAME") ;
	if ( anyctrl_name  != NULL) {

		// Initialize card id for public and private
		seqanyctrl_client = GetSeqClientFromPortName(anyctrl_name);
    anyctrl_midi_card = GetCardFromShortName(anyctrl_name);

		if ( seqanyctrl_client  < 0 || anyctrl_midi_card < 0 ) {
			fprintf(stderr,"[tkgl]  **** Error : %s seq client or card not found\n",anyctrl_name);
			exit(1);
		}
		fprintf(stdout,"[tkgl]  %s connect port is %d:0\n",anyctrl_name,seqanyctrl_client);
    fprintf(stdout,"[tkgl]  %s card id hw:%d found\n",anyctrl_name,anyctrl_midi_card);

	}

	// Create 3 virtuals ports : Private I/O, Public O
	// This will trig our hacked snd_seq_create_simple_port during the call.
  // NB : the snd_rawmidi_open is hacked here to return the client id of the virtual port.
  // So, return is either < 0 if error, or > 0, being the client number if everything is ok.
  // The port is always 0. This is the standard behaviour.

  seqvirt_client_inpriv  = snd_rawmidi_open(&rawvirt_inpriv, NULL,  "[virtual]TKGL Virtual In Private", 2);
  seqvirt_client_outpriv = snd_rawmidi_open(NULL, &rawvirt_outpriv, "[virtual]TKGL Virtual Out Private", 3);
  seqvirt_client_outpub  = snd_rawmidi_open(NULL, &rawvirt_outpub,  "[virtual]TKGL Virtual Out Public", 3);

	if ( seqvirt_client_inpriv < 0 || seqvirt_client_outpriv < 0 || seqvirt_client_outpub < 0 ) {
		fprintf(stderr,"[tkgl]  **** Error : impossible to create one or many virtual ports\n");
		exit(1);
	}

  fprintf(stdout,"[tkgl]  Virtual private input port %d  created.\n",seqvirt_client_inpriv);
	fprintf(stdout,"[tkgl]  Virtual private output port %d created.\n",seqvirt_client_outpriv);
	fprintf(stdout,"[tkgl]  Virtual public output port %d created.\n",seqvirt_client_outpub);


  // Make connections of our virtuals ports
  // MPC APP <---> VIRTUAL PORTS <---> MPC CONTROLLER PRIVATE & PUBLIC PORTS

	// Private MPC controller port 1 out to virtual In priv 0
	aconnect(	mpc_seq_client, 1, seqvirt_client_inpriv, 0);

	// Virtual out priv 0 to Private MPC controller port 1 in
	aconnect(	seqvirt_client_outpriv, 0, mpc_seq_client, 1);

	// Virtual out pub to Public MPC controller port 1 in
  // No need to cable the out...
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


  // Create a user virtual port if asked on the command line

  if ( user_virtual_portname != NULL) {

    char temp_portname[64];
    sprintf(temp_portname,"[virtual]%s",user_virtual_portname);
    if ( snd_rawmidi_open(&rawvirt_user_in, &rawvirt_user_out,  temp_portname, 0 ) < 0 ) {
      fprintf(stderr,"[tkgl]  **** Error : impossible to create virtual user port %s\n",user_virtual_portname);
  		exit(1);
    }
    fprintf(stderr,"[tkgl]  Virtual user port %s succesfully created.\n",user_virtual_portname);
    //snd_rawmidi_open(&read_handle, &write_handle, "virtual", 0);

  }

	fflush(stdout);
}

////////////////////////////////////////////////////////////////////////////////
// Clean DUMP of a buffer to screen
////////////////////////////////////////////////////////////////////////////////
static void ShowBufferHexDump(const uint8_t* data, size_t sz, uint8_t nl)
{
    uint8_t b;
    char asciiBuff[33];
    uint8_t c=0;

    for (uint16_t idx = 0 ; idx < sz; idx++) {
			if ( c == 0 && idx >= 0) fprintf(stdout,"[tkgl]  ");
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

    // Banner
    fprintf(stdout,"[tkgl]  ------------------------------------------\n");
  	fprintf(stdout,"[tkgl]  TKGL_MPCMAPPER V1.0 by the KikGen Labs\n");
  	fprintf(stdout,"[tkgl]  ------------------------------------------\n");

    // Scan command line
    char * tkgl_SpoofArg = NULL;

    for ( int i = 1 ; i < argc ; i++ ) {
      if ( ( strncmp("--tkgl_ctrlname=",argv[i],16) == 0 ) && ( strlen(argv[i]) >16 ) ) {
         anyctrl_name = argv[i] + 16;
         fprintf(stdout,"[tkgl]  --tgkl_ctrlname specified for %s midi controller\n",anyctrl_name) ;
      }
      else
      // Spoofed product id
      if ( ( strcmp("--tkgl_iamX",argv[i]) == 0 ) ) {
        MPCIdSpoofed = MPC_X;
        tkgl_SpoofArg = argv[i];
      }
      else
      if ( ( strcmp("--tkgl_iamLive",argv[i]) == 0 ) ) {
        MPCIdSpoofed = MPC_LIVE;
        tkgl_SpoofArg = argv[i];
      }
      else
      if ( ( strcmp("--tkgl_iamForce",argv[i]) == 0 ) ) {
        MPCIdSpoofed = MPC_FORCE;
        tkgl_SpoofArg = argv[i];
      }
      else
      if ( ( strcmp("--tkgl_iamOne",argv[i]) == 0 ) ) {
        MPCIdSpoofed = MPC_ONE;
        tkgl_SpoofArg = argv[i];
      }
      else
      if ( ( strcmp("--tkgl_iamLive2",argv[i]) == 0 ) ) {
        MPCIdSpoofed = MPC_LIVE_MK2;
        tkgl_SpoofArg = argv[i];
      }
      else
      // End user virtual port visible from the MPC app
      if ( ( strncmp("--tkgl_virtualport=",argv[i],19) == 0 ) && ( strlen(argv[i]) >19 ) ) {
         user_virtual_portname = argv[i] + 19;
         fprintf(stdout,"[tkgl]  --tkgl_virtualport specified as %s port name\n",user_virtual_portname) ;
      }
      else
      // Dump rawmidi
      if ( ( strcmp("--tkgl_mididump",argv[i]) == 0 ) ) {
        rawMidiDumpFlag = 1 ;
        fprintf(stdout,"[tkgl]  --tkgl_mididump specified : dump original raw midi message (ENTRY)\n") ;
      }
      else
      if ( ( strcmp("--tkgl_mididumpPost",argv[i]) == 0 ) ) {
        rawMidiDumpPostFlag = 1 ;
        fprintf(stdout,"[tkgl]  --tkgl_mididumpPost specified : dump raw midi message after transformation (POST)\n") ;
      }


    }

    if ( MPCIdSpoofed >= 0 ) {
      fprintf(stdout,"[tkgl]  %s specified. %s spoofing.\n",tkgl_SpoofArg,MPCProductString[MPCIdSpoofed]) ;
    }

    // Initialize everything
    tkgl_init();

    // ... and call main again
    return orig(main, argc, argv, init, fini, rtld_fini, stack_end);
}

///////////////////////////////////////////////////////////////////////////////
// ALSA snd_rawmidi_open hooked
///////////////////////////////////////////////////////////////////////////////
// This function allows changing the name of a virtual port, by using the
// naming convention "[virtual]port name"
// and if creation succesfull, will return the client number. Port is always 0 if virtual.

int snd_rawmidi_open(snd_rawmidi_t **inputp, snd_rawmidi_t **outputp, const char *name, int mode)
{

	fprintf(stdout,"[tkgl]  snd_rawmidi_open name %s mode %d\n",name,mode);


  // Rename the virtual port as we need
  // Port Name must not be emtpy - 30 chars max
  if ( strncmp(name,"[virtual]",9) == 0 ) {
    int l = strlen(name);
    if ( l <= 9 || l > 30 + 9 ) return -1;

    // Prepare the renaming of the virtual port
    strcpy(snd_seq_virtual_port_newname,name + 9);
    snd_seq_virtual_port_rename_flag = 1;

    // Create the virtual port via the fake Alsa rawmidi virtual open
    int r = orig_snd_rawmidi_open(inputp, outputp, "virtual", mode);
    if ( r < 0 ) return r;

    // Get the port id that was populated in the port creation sub function
    // and reset it
    r = snd_seq_virtual_port_clientid;
    snd_seq_virtual_port_clientid=-1;

    //fprintf(stdout,"[tkgl]  PORT ID IS %d\n",r);

    return r;

  }

	// Substitute the hardware private input port by our input virtual ports

	else if ( strcmp(mpc_midi_private_alsa_name,name) == 0   ) {

		// Private In
		if (inputp) *inputp = rawvirt_inpriv;
		else if (outputp) *outputp = rawvirt_outpriv ;
		else return -1;
		fprintf(stdout,"[tkgl]  %s substitution by virtual rawmidi successfull\n",name);

		return 0;
	}

	else if ( strcmp(mpc_midi_public_alsa_name,name) == 0   ) {

		if (outputp) *outputp = rawvirt_outpub;
		else return -1;
		fprintf(stdout,"[tkgl]  %s substitution by virtual rawmidi successfull\n",name);

		return 0;
	}

	return orig_snd_rawmidi_open(inputp, outputp, name, mode);
}

///////////////////////////////////////////////////////////////////////////////
// MAP MPC controller hardware midi event to MPC application
///////////////////////////////////////////////////////////////////////////////
static void Map_AppReadFromMPC(void *midiBuffer, size_t size) {

    uint8_t * myBuff = (uint8_t*)midiBuffer;

    for ( int i = 0 ; i < size ; i++ ) {

      // Check if identity request ? (could be optimized....)
      if (  myBuff[i] == 0xF0 &&  memcmp(&myBuff[i],IdentityReplySysexHeader,sizeof(IdentityReplySysexHeader)) == 0 ) {
        // If so, substitue sysex identity request by the faked one
        memcpy(&myBuff[i+sizeof(IdentityReplySysexHeader)],MPCIdentityReplySysex[MPCId], sizeof(MPCIdentityReplySysex[MPCId])  );
        i += sizeof(IdentityReplySysexHeader) + sizeof(MPCIdentityReplySysex[MPCId]);
      }
    }

}

static void Map_AppWriteToMPC(const void *midiBuffer, size_t size) {

  uint8_t * myBuff = (uint8_t*)midiBuffer;

  for ( int i = 0 ; i < size ; i++ ) {
    // If we detect the Akai sysex header, change the harwware id by our true hardware id.
    // Messages are compatibles. Some midi msg are not always interpreted (e.g. Oled)

    if (  myBuff[i] == 0xF0 &&  memcmp(&myBuff[i],AkaiSysex,sizeof(AkaiSysex)) == 0 ) {
        // Update the sysex id in the sysex
        myBuff[i+3] = MPCSysexId[MPCOriginalId];
        i += sizeof(AkaiSysex) ;
    }
  }

}


///////////////////////////////////////////////////////////////////////////////
// Alsa Rawmidi read
///////////////////////////////////////////////////////////////////////////////
ssize_t snd_rawmidi_read(snd_rawmidi_t *rawmidi, void *buffer, size_t size) {

	ssize_t r = orig_snd_rawmidi_read(rawmidi, buffer, size);
  if ( rawMidiDumpFlag ) {
    const char *name = snd_rawmidi_name(rawmidi);
    fprintf(stdout,"[tkgl]  ENTRY Dump snd_rawmidi_read from controller %s\n",name);
    ShowBufferHexDump(buffer, size,16);
    fprintf(stdout,"[tkgl]\n");
  }


  // Map only if not on the original hardware
  if ( MPCId != MPCOriginalId && rawmidi == rawvirt_inpriv) {
        Map_AppReadFromMPC(buffer,size);
  }

  if ( rawMidiDumpPostFlag ) {
    const char *name = snd_rawmidi_name(rawmidi);
    fprintf(stdout,"[tkgl]  POST Dump snd_rawmidi_read from controller %s\n",name);
    ShowBufferHexDump(buffer, size,16);
    fprintf(stdout,"[tkgl]\n");
  }


	return r;
}

///////////////////////////////////////////////////////////////////////////////
// Alsa Rawmidi write
///////////////////////////////////////////////////////////////////////////////
ssize_t snd_rawmidi_write(snd_rawmidi_t * 	rawmidi,const void * 	buffer,size_t 	size) {

  if ( rawMidiDumpFlag ) {
    const char *name = snd_rawmidi_name(rawmidi);
    fprintf(stdout,"[tkgl]  ENTRY Dump snd_rawmidi_write to controller %s\n",name);
    ShowBufferHexDump(buffer, size,16);
    fprintf(stdout,"[tkgl]\n");
  }

  //const char *name = snd_rawmidi_name(rawmidi);
  //fprintf(stdout,"[tkgl]  Rawmidi_write %s\n",name);

  // Map only if not on the original hardware
  if ( MPCId != MPCOriginalId  &&
       ( rawmidi == rawvirt_outpriv || rawmidi == rawvirt_outpub ) ) {
         Map_AppWriteToMPC(buffer,size);
  }

  if ( rawMidiDumpPostFlag ) {
    const char *name = snd_rawmidi_name(rawmidi);
    fprintf(stdout,"[tkgl]  POST Dump snd_rawmidi_write to controller %s\n",name);
    ShowBufferHexDump(buffer, size,16);
    fprintf(stdout,"[tkgl]\n");
  }

	return 	orig_snd_rawmidi_write(rawmidi, buffer, size);

}

///////////////////////////////////////////////////////////////////////////////
// Alsa open sequencer
///////////////////////////////////////////////////////////////////////////////
int snd_seq_open (snd_seq_t **handle, const char *name, int streams, int mode) {

  fprintf(stdout,"[tkgl]  snd_seq_open %s (%p) \n",name,handle);

  return orig_snd_seq_open(handle, name, streams, mode);

}

///////////////////////////////////////////////////////////////////////////////
// Alsa set a seq port name
///////////////////////////////////////////////////////////////////////////////
void snd_seq_port_info_set_name	(	snd_seq_port_info_t * 	info, const char * 	name )
{
  fprintf(stdout,"[tkgl]  snd_seq_port_info_set_name %s (%p) \n",name);


  return snd_seq_port_info_set_name	(	info, name );
}

///////////////////////////////////////////////////////////////////////////////
// Alsa create a simple seq port
///////////////////////////////////////////////////////////////////////////////
int snd_seq_create_simple_port	(	snd_seq_t * 	seq, const char * 	name, unsigned int 	caps, unsigned int 	type )
{
//fprintf(stdout,"[tkgl]  Port creation of  %s\n",name);

  // Rename virtual port correctly. Impossible with the native Alsa...
  if ( strncmp (" Virtual RawMIDI",name,16) && snd_seq_virtual_port_rename_flag  )
  {
    //fprintf(stdout,"[tkgl]  Virtual port renamed to %s \n",snd_seq_virtual_port_newname);
    snd_seq_virtual_port_rename_flag = 0;
    int r = orig_snd_seq_create_simple_port(seq,snd_seq_virtual_port_newname,caps,type);
    if ( r < 0 ) return r;

    // Get port information
    snd_seq_port_info_t *pinfo;
    snd_seq_port_info_alloca(&pinfo);
    snd_seq_get_port_info(seq, 0, pinfo);
    snd_seq_virtual_port_clientid =  snd_seq_port_info_get_client(pinfo);

    return r;
  }


	// We do not allow ports creation by MPC app for our device or our virtuals ports
  // Because this could lead to infinite midi loop in the MPC midi settings
	if (  ( seqanyctrl_client >= 0 && strstr(name, anyctrl_name ) )
   //||   ( match(name,"^Client-[0-9][0-9][0-9] TKGL.*" ) )

   // In some specific cases, public and private port could appear in the APP when spoofing,
   // because port names haven't the same prefixes (eg. Force vs MPC). The consequence is
   // that the MPC App receives midi message of buttons and encoders in midi tracks.
   // So we mask here Private and Public ports eventually requested by MPC App, which
   // should be only internal rawmidi ports.

   // This match will also catch our TKGL virtual ports having Private or Public suffix.
   ||  ( ( match(name,".* Public$|.* Private$" )) )

 )
 {
    fprintf(stdout,"[tkgl]  Port %s creation canceled.\n",name);
    return -1;
 }

 return orig_snd_seq_create_simple_port(seq,name,caps,type);
}

///////////////////////////////////////////////////////////////////////////////
// Clean dump of a midi seq event
///////////////////////////////////////////////////////////////////////////////

static void dump_event(const snd_seq_event_t *ev)
{
	printf("%3d:%-3d ", ev->source.client, ev->source.port);
	switch (ev->type) {
	case SND_SEQ_EVENT_NOTEON:
		printf("Note on                %2d %3d %3d\n",
		       ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
		break;
	case SND_SEQ_EVENT_NOTEOFF:
		printf("Note off               %2d %3d %3d\n",
		       ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
		break;
	case SND_SEQ_EVENT_KEYPRESS:
		printf("Polyphonic aftertouch  %2d %3d %3d\n",
		       ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
		break;
	case SND_SEQ_EVENT_CONTROLLER:
		printf("Control change         %2d %3d %3d\n",
		       ev->data.control.channel, ev->data.control.param, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_PGMCHANGE:
		printf("Program change         %2d %3d\n",
		       ev->data.control.channel, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_CHANPRESS:
		printf("Channel aftertouch     %2d %3d\n",
		       ev->data.control.channel, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_PITCHBEND:
		printf("Pitch bend             %2d  %6d\n",
		       ev->data.control.channel, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_CONTROL14:
		printf("Control change         %2d %3d %5d\n",
		       ev->data.control.channel, ev->data.control.param, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_NONREGPARAM:
		printf("Non-reg. parameter     %2d %5d %5d\n",
		       ev->data.control.channel, ev->data.control.param, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_REGPARAM:
		printf("Reg. parameter         %2d %5d %5d\n",
		       ev->data.control.channel, ev->data.control.param, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_SONGPOS:
		printf("Song position pointer     %5d\n",
		       ev->data.control.value);
		break;
	case SND_SEQ_EVENT_SONGSEL:
		printf("Song select               %3d\n",
		       ev->data.control.value);
		break;
	case SND_SEQ_EVENT_QFRAME:
		printf("MTC quarter frame         %02xh\n",
		       ev->data.control.value);
		break;
	case SND_SEQ_EVENT_TIMESIGN:
		// XXX how is this encoded?
		printf("SMF time signature        (%#08x)\n",
		       ev->data.control.value);
		break;
	case SND_SEQ_EVENT_KEYSIGN:
		// XXX how is this encoded?
		printf("SMF key signature         (%#08x)\n",
		       ev->data.control.value);
		break;
	case SND_SEQ_EVENT_START:
		if (ev->source.client == SND_SEQ_CLIENT_SYSTEM &&
		    ev->source.port == SND_SEQ_PORT_SYSTEM_TIMER)
			printf("Queue start               %d\n",
			       ev->data.queue.queue);
		else
			printf("Start\n");
		break;
	case SND_SEQ_EVENT_CONTINUE:
		if (ev->source.client == SND_SEQ_CLIENT_SYSTEM &&
		    ev->source.port == SND_SEQ_PORT_SYSTEM_TIMER)
			printf("Queue continue            %d\n",
			       ev->data.queue.queue);
		else
			printf("Continue\n");
		break;
	case SND_SEQ_EVENT_STOP:
		if (ev->source.client == SND_SEQ_CLIENT_SYSTEM &&
		    ev->source.port == SND_SEQ_PORT_SYSTEM_TIMER)
			printf("Queue stop                %d\n",
			       ev->data.queue.queue);
		else
			printf("Stop\n");
		break;
	case SND_SEQ_EVENT_SETPOS_TICK:
		printf("Set tick queue pos.       %d\n", ev->data.queue.queue);
		break;
	case SND_SEQ_EVENT_SETPOS_TIME:
		printf("Set rt queue pos.         %d\n", ev->data.queue.queue);
		break;
	case SND_SEQ_EVENT_TEMPO:
		printf("Set queue tempo           %d\n", ev->data.queue.queue);
		break;
	case SND_SEQ_EVENT_CLOCK:
		printf("Clock\n");
		break;
	case SND_SEQ_EVENT_TICK:
		printf("Tick\n");
		break;
	case SND_SEQ_EVENT_QUEUE_SKEW:
		printf("Queue timer skew          %d\n", ev->data.queue.queue);
		break;
	case SND_SEQ_EVENT_TUNE_REQUEST:
		/* something's fishy here ... */
		printf("Tuna request\n");
		break;
	case SND_SEQ_EVENT_RESET:
		printf("Reset\n");
		break;
	case SND_SEQ_EVENT_SENSING:
		printf("Active Sensing\n");
		break;
	case SND_SEQ_EVENT_CLIENT_START:
		printf("Client start              %d\n",
		       ev->data.addr.client);
		break;
	case SND_SEQ_EVENT_CLIENT_EXIT:
		printf("Client exit               %d\n",
		       ev->data.addr.client);
		break;
	case SND_SEQ_EVENT_CLIENT_CHANGE:
		printf("Client changed            %d\n",
		       ev->data.addr.client);
		break;
	case SND_SEQ_EVENT_PORT_START:
		printf("Port start                %d:%d\n",
		       ev->data.addr.client, ev->data.addr.port);
		break;
	case SND_SEQ_EVENT_PORT_EXIT:
		printf("Port exit                 %d:%d\n",
		       ev->data.addr.client, ev->data.addr.port);
		break;
	case SND_SEQ_EVENT_PORT_CHANGE:
		printf("Port changed              %d:%d\n",
		       ev->data.addr.client, ev->data.addr.port);
		break;
	case SND_SEQ_EVENT_PORT_SUBSCRIBED:
		printf("Port subscribed           %d:%d -> %d:%d\n",
		       ev->data.connect.sender.client, ev->data.connect.sender.port,
		       ev->data.connect.dest.client, ev->data.connect.dest.port);
		break;
	case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
		printf("Port unsubscribed         %d:%d -> %d:%d\n",
		       ev->data.connect.sender.client, ev->data.connect.sender.port,
		       ev->data.connect.dest.client, ev->data.connect.dest.port);
		break;
	case SND_SEQ_EVENT_SYSEX:
		{
			unsigned int i;
			printf("System exclusive         ");
			for (i = 0; i < ev->data.ext.len; ++i)
				printf(" %02X", ((unsigned char*)ev->data.ext.ptr)[i]);
			printf("\n");
		}
		break;
	default:
		printf("Event type %d\n",  ev->type);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Process an input midi seq event
///////////////////////////////////////////////////////////////////////////////
int snd_seq_event_input( snd_seq_t* handle, snd_seq_event_t** ev )
{

  int r = orig_snd_seq_event_input(handle,ev);
  // if ((*ev)->type != SND_SEQ_EVENT_CLOCK ) {
  //      dump_event(*ev);
  //
  //
  //    // fprintf(stdout,"[tkgl] Src = %02d:%02d -> Dest = %02d:%02d \n",(*ev)->source.client,(*ev)->source.port,(*ev)->dest.client,(*ev)->dest.port);
  //    // ShowBufferHexDump(buf, r,16);
  //    // fprintf(stdout,"[tkgl] ----------------------------------\n");
  //  }


  return r;

}

///////////////////////////////////////////////////////////////////////////////
// Decode a midi seq event
///////////////////////////////////////////////////////////////////////////////
long snd_midi_event_decode	(	snd_midi_event_t * 	dev,unsigned char * 	buf,long 	count, const snd_seq_event_t * 	ev )
{

	// Disable running status to be a true "raw" midi. Side effect : disabled for all ports...
	snd_midi_event_no_status(dev,1);
  long r = orig_snd_midi_event_decode(dev,buf,count,ev);
  // if (r > 0) {
  //      if (ev->type != SND_SEQ_EVENT_CLOCK ) {
  // //       dump_event(ev);
  //
  //
  //         fprintf(stdout,"[tkgl] Src = %02d:%02d -> Dest = %02d:%02d \n",ev->source.client,ev->source.port,ev->dest.client,ev->dest.port);
  //         ShowBufferHexDump(buf, r,16);
  //         fprintf(stdout,"[tkgl] ----------------------------------\n");
  //       }
  //
  // }

	return r ;

}


///////////////////////////////////////////////////////////////////////////////
// open64
///////////////////////////////////////////////////////////////////////////////
// We intercept all file opening until we found inmusic,product-code in the path.
// We could do the same for serial number, or panel/touch orientaton.

int open64(const char *pathname, int flags,...) {
   int r;

//   printf("(tkgl) Open64 %s\n",pathname);

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

   // Specific part
   if ( product_code_file_handler < 0 && strcmp(pathname,PRODUCT_CODE_PATH) == 0 ) {
     // Save the product code file descriptor
     product_code_file_handler = r;
   }

   return r ;
}

///////////////////////////////////////////////////////////////////////////////
// open
///////////////////////////////////////////////////////////////////////////////
int open(const char *pathname, int flags,...) {

//  printf("(tkgl) Open %s\n",pathname);


   // If O_CREAT is used to create a file, the file access mode must be given.
   if (flags & O_CREAT) {
       va_list args;
       va_start(args, flags);
       int mode = va_arg(args, int);
       va_end(args);
       return orig_open(pathname, flags, mode);
   } else {
       return orig_open(pathname, flags);
   }
}

///////////////////////////////////////////////////////////////////////////////
// read
///////////////////////////////////////////////////////////////////////////////
ssize_t read(int fildes, void *buf, size_t nbyte) {

  // Not yet
  if ( product_code_file_handler < 0 )  return orig_read(fildes,buf,nbyte);

  // If we got the file descriptor, we can swap the product code with the spoofed one.
  if ( fildes == product_code_file_handler ) {
    char product_code[5];
    orig_read(fildes,&product_code,nbyte);

    // Find the id in the product code table
    for (int i = 0 ; i < _END_MPCID; i++) {
      if ( strncmp(MPCProductCode[i],product_code,4) == 0 ) {
        MPCOriginalId = i;
        break;
      }
    }
    if ( MPCOriginalId < 0) {
      fprintf(stdout,"[tkgl]  *** Error when reading the product-code file\n");
      exit(1);
    }
    fprintf(stdout,"[tkgl]  Original Product code : %s (%s)\n",MPCProductCode[MPCOriginalId],MPCProductString[MPCOriginalId]);

    if ( MPCIdSpoofed >= 0 ) {
      fprintf(stdout,"[tkgl]  Product code spoofed to %s (%s)\n",MPCProductCode[MPCIdSpoofed],MPCProductString[MPCIdSpoofed]);
      MPCId = MPCIdSpoofed ;
    } else MPCId = MPCOriginalId ;

    memcpy(buf,MPCProductCode[MPCId],nbyte );
    product_code_file_handler = -1;
    return nbyte;
  }

}
