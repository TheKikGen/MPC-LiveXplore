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
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <alsa/asoundlib.h>
#include <dlfcn.h>
#include <libgen.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>

#include "tkgl_mpcmapper.h"

// Log utilities ---------------------------------------------------------------

enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

#define tklog_trace(...) tklog(LOG_TRACE,  __VA_ARGS__)
#define tklog_debug(...) tklog(LOG_DEBUG,  __VA_ARGS__)
#define tklog_info(...)  tklog(LOG_INFO,   __VA_ARGS__)
#define tklog_warn(...)  tklog(LOG_WARN,   __VA_ARGS__)
#define tklog_error(...) tklog(LOG_ERROR,  __VA_ARGS__)
#define tklog_fatal(...) tklog(LOG_FATAL,  __VA_ARGS__)

static const char *tklog_level_strings[] = {
  "TRACE", "DEBUG", "INFO", "WARN", "***ERROR", "***FATAL"
};

static void tklog(int level, const char *fmt, ...) {

  va_list ap;

  //time_t timestamp = time( NULL );
  //struct tm * now = localtime( & timestamp );

  //char buftime[16];
  //buftime[strftime(buftime, sizeof(buftime), "%H:%M:%S", now)] = '\0';

  fprintf(stdout, "[tkgl %-8s]  ",tklog_level_strings[level]);

  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);

  fflush(stdout);

}

// Function prototypes ---------------------------------------------------------

// Globals ---------------------------------------------------------------------

// Raw midi dump flag (for debugging purpose)
static uint8_t rawMidiDumpFlag = 0 ;     // Before transformation
static uint8_t rawMidiDumpPostFlag = 0 ; // After our tranformation

// Config file name
static char *configFileName = NULL;

// Buttons and controls Mapping tables
// SHIFT values have bit 7 set

static int map_ButtonsLeds[MAPPING_TABLE_SIZE];
static int map_ButtonsLeds_Inv[MAPPING_TABLE_SIZE]; // Inverted table

static int map_Ctrl[MAPPING_TABLE_SIZE];
static int map_Ctrl_Inv[MAPPING_TABLE_SIZE]; // Inverted table

// To navigate in matrix quadran when MPC spoofing a Force
static int MPCPad_OffsetL = 0;
static int MPCPad_OffsetC = 0;
static int MPCPad_OffsetLLast = 0;

// To change the bank group when using Force as a MPC
static uint8_t MpcBankGroup = 0 ;

// Force Matric pads color cache
static ForceMPCPadColor_t PadSysexColorsCache[256];

// End user virtual port name
static char *user_virtual_portname = NULL;

// End user virtual port handles
static snd_rawmidi_t *rawvirt_user_in    = NULL ;
static snd_rawmidi_t *rawvirt_user_out   = NULL ;

// MPC Current pad bank.  A-H = 0-7
static int MPC_PadBank = BANK_A ;

// SHIFT Holded mode
// Holding shift will activate the shift mode
static bool shiftHoldedMode=false;

// Qlink knobs shift mode
static bool QlinkKnobsShiftMode=false;

// Columns modes in Force simulated on a MPC
static int ForceColumnMode = -1 ;


// Our MPC product id (index in the table)
static int MPCOriginalId = -1;
// The one used
static int MPCId = -1;
// and the spoofed one,
static int MPCSpoofedID = -1;

// Internal product code file handler to change on the fly when the file will be opened
// That avoids all binding stuff in shell
static int product_code_file_handler = -1 ;
static int product_compatible_file_handler = -1 ;
// Power supply file handlers
static int pws_online_file_handler = -1 ;
static int pws_voltage_file_handler = -1 ;
static FILE *fpws_voltage_file_handler;

static int pws_present_file_handler = -1 ;
static int pws_status_file_handler = -1 ;
static int pws_capacity_file_handler = -1 ;

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
//static typeof(&snd_seq_event_input) orig_snd_seq_event_input;

// Globals used to rename a virtual port and get the client id.  No other way...
static int  snd_seq_virtual_port_rename_flag  = 0;
static char snd_seq_virtual_port_newname [30];
static int  snd_seq_virtual_port_clientid=-1;

// Other more generic APIs
static typeof(&open64) orig_open64;
//static typeof(&read) orig_read;
//static typeof(&open) orig_open;
static typeof(&close) orig_close;

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
// Read a key,value pair from our config file (INI file format)
///////////////////////////////////////////////////////////////////////////////
// Can return the full key list in the sectionKeys
// If sectionKeys is NULL, will return the value of the key if matching

static int GetKeyValueFromConfFile(const char * confFileName, const char *sectionName,const char* key, char value[], char sectionKeys[][INI_FILE_KEY_MAX_LEN], size_t keyMaxCount ) {

  FILE *fp = fopen(confFileName, "r");
  if ( fp == NULL) return -1;

  char line [INI_FILE_LINE_MAX_LEN];

  bool   withinMySection = false;
  bool   parseFullSection = (keyMaxCount > 0 );

  int keyIndex = 0;

  char *p = NULL;
  char *v = NULL ;
  int i = 0;


  while (fgets(line, sizeof(line), fp)) {

      //fprintf(stdout,"[tkgl]  read line : %s\n",line);

    // Remove spaces before
    p = line;
    while (isspace (*p)) p++;

    // Empty line or comments
    if ( p[0] == '\0' || p[0] == '#' || p[0] == ';' ) continue;

    // Remove spaces after
    for ( i = strlen (p) - 1; (isspace (p[i]));  i--) ;
    p[i + 1] = '\0';

    // A section ?
    if ( p[0] == '[' && p[strlen(p) - 1 ] == ']' ) {

      if ( strncmp(p + 1,sectionName,strlen(p)-2 ) == 0 ) {
        withinMySection = true;
        //fprintf(stdout,"[tkgl]  Section %s found.\n",sectionName);
      }
      continue;
    }

    // Section was already found : read the value of the key
    if ( withinMySection ) {
      // Next section  ? stop
      if ( p[0] == '[' ) break;

      // Search "=" sign
      char * v = strstr(p,"=");
      if ( v == NULL ) {
          tklog_error("Error in Configuration file : '=' missing : %s. \n",line);
          return -1; // Syntax error
      }
      *v = '\0';
      v++;  // V now point on the value

      // Remove spaces after the key name (before =)
      for ( i = strlen (p) - 1; (isspace (p[i]));  i--) ;
      p[i + 1] = '\0';

      // Remove spaces before the value (after =)
      while (isspace (*v)) v++;

      // We need the full section in a char * array
      if ( parseFullSection ) {

        if ( keyIndex < keyMaxCount ) {
          strncpy(&sectionKeys[keyIndex][0],p,INI_FILE_KEY_MAX_LEN - 1);
          keyIndex++;
        }
        else {
          tklog_warn("Maximum of %d keys by section reached for key %s. The key is ignored. \n",keyMaxCount,p);
        }
      }
      else {
        // Check the key value

        if ( strcmp( p,key ) == 0 ) {
          strcpy(value,v);
          keyIndex = 1;
          //fprintf(stdout,"Loaded : %s.\n",line);

          break;
        }
      }
    }
  }
  fclose(fp);
  //fprintf(stdout,"config file closed.\n");


  return keyIndex ;
}

///////////////////////////////////////////////////////////////////////////////
// Load mapping tables from config file
///////////////////////////////////////////////////////////////////////////////
static void LoadMappingFromConfFile(const char * confFileName) {

  // shortcuts
  char *ProductStrShort     = DeviceInfoBloc[MPCOriginalId].productStringShort;
  char *currProductStrShort = DeviceInfoBloc[MPCId].productStringShort;

  // Section name strings
  char btLedMapSectionName[64];
  char ctrlMapSectionName[64];

  char btLedSrcSectionName[64];
  char ctrlSrcSectionName[64];

  char btLedDestSectionName[64];
  char ctrlDestSectionName[64];

  // Keys and values
  char srcKeyName[64];
  char destKeyName[64];

  char myKey[64];
  char myValue[64];

  // Array of keys in the section
  char keyNames [MAPPING_TABLE_SIZE][INI_FILE_KEY_MAX_LEN] ;

  int  keysCount = 0;

  // Initialize global mapping tables
  for ( int i = 0 ; i < MAPPING_TABLE_SIZE ; i++ ) {

    map_ButtonsLeds[i] = -1;
    map_ButtonsLeds_Inv[i] = -1;

    map_Ctrl[i] = -1;
    map_Ctrl_Inv[i] = -1;

  }

  if ( confFileName == NULL ) return ;  // No config file ...return

  // Make section names
  sprintf(btLedMapSectionName,"Map_%s_%s_ButtonsLeds", ProductStrShort,currProductStrShort);
  sprintf(ctrlMapSectionName, "Map_%s_%s_Controls",    ProductStrShort,currProductStrShort);

  sprintf(btLedSrcSectionName,"%s_ButtonsLeds",ProductStrShort);
  sprintf(ctrlSrcSectionName,"%s_Controls",    ProductStrShort);

  sprintf(btLedDestSectionName,"%s_ButtonsLeds",currProductStrShort);
  sprintf(ctrlDestSectionName,"%s_Controls",    currProductStrShort);


  // Get keys of the mapping section. You need to pass the key max len string corresponding
  // to the size of the string within the array
  keysCount = GetKeyValueFromConfFile(confFileName, btLedMapSectionName,NULL,NULL,keyNames,MAPPING_TABLE_SIZE) ;

  if (keysCount < 0) {
    tklog_error("Configuration file %s read error.\n", confFileName);
    return ;
  }

  tklog_info("%d keys found in section %s . \n",keysCount,btLedMapSectionName);

  if ( keysCount <= 0 ) {
    tklog_error("Missing section %s in configuration file %s or syntax error. No mapping set. \n",btLedMapSectionName,confFileName);
    return;
  }

  // Get Globals within the mapping section name of the current device
  if ( GetKeyValueFromConfFile(confFileName, btLedMapSectionName,"_p_QlinkKnobsShiftMode",myValue,NULL,0) ) {

    QlinkKnobsShiftMode = ( atoi(myValue) == 1 ? true:false );
    if ( QlinkKnobsShiftMode) tklog_info("QlinkKnobsShiftMode was set to 1.\n");

  } else {
    QlinkKnobsShiftMode = false; // default
    tklog_warn("Missing _p_QlinkKnobsShiftMod key in section . Was set to 0 by default.\n");
  }

  // Read the Buttons & Leds mapping section entries
  for ( int i = 0 ; i < keysCount ; i++ ) {

    // Buttons & Leds mapping

    // Ignore parameters
    if ( strncmp(keyNames[i],"_p_",3) == 0 ) continue;

    strcpy(srcKeyName, keyNames[i] );

    if (  GetKeyValueFromConfFile(confFileName, btLedMapSectionName,srcKeyName,myValue,NULL,0) != 1 ) {
      tklog_error("Value not found for %s key in section[%s].\n",srcKeyName,btLedMapSectionName);
      continue;
    }
    // Mapped button name
    // Check if the reserved keyword "SHIFT_" is present
    // Shift mode of a src button
    bool srcShift = false;
    if ( strncmp(srcKeyName,"SHIFT_",6) == 0 )  {
        srcShift = true;
        strcpy(srcKeyName, &srcKeyName[6] );
    }

    strcpy(destKeyName,myValue);
    bool destShift = false;
    if ( strncmp(destKeyName,"SHIFT_",6) == 0 )  {
        destShift = true;
        strcpy(destKeyName, &destKeyName[6] );
    }

    // Read value in original device section
    if (  GetKeyValueFromConfFile(confFileName, btLedSrcSectionName,srcKeyName,myValue,NULL,0) != 1 ) {
      tklog_error("Value not found for %s key in section[%s].\n",srcKeyName,btLedSrcSectionName);
      continue;
    }

    // Save the button value
    int srcButtonValue =  strtol(myValue, NULL, 0);

    // Read value in target device section
    if (  GetKeyValueFromConfFile(confFileName, btLedDestSectionName,destKeyName,myValue,NULL,0) != 1 ) {
      tklog_error("Error *** Value not found for %s key in section[%s].\n",destKeyName,btLedDestSectionName);
      continue;
    }

    int destButtonValue = strtol(myValue, NULL, 0);

    if ( srcButtonValue <=127 && destButtonValue <=127 ) {

      // If shift mapping, set the bit 7
      srcButtonValue   = ( srcShift  ? srcButtonValue  + 0x80 : srcButtonValue );
      destButtonValue  = ( destShift ? destButtonValue + 0x80 : destButtonValue );

      map_ButtonsLeds[srcButtonValue]      = destButtonValue;
      map_ButtonsLeds_Inv[destButtonValue] = srcButtonValue;

      tklog_info("Button-Led %s%s (%d) mapped to %s%s (%d)\n",srcShift?"(SHIFT) ":"",srcKeyName,srcButtonValue,destShift?"(SHIFT) ":"",destKeyName,map_ButtonsLeds[srcButtonValue]);

    }
    else {
      tklog_error("Configuration file Error : values above 127 found. Check sections [%s] %s, [%s] %s.\n",btLedSrcSectionName,srcKeyName,btLedDestSectionName,destKeyName);
      return;
    }

  } // for
}

///////////////////////////////////////////////////////////////////////////////
// Prepare a fake midi message in the Private midi context
///////////////////////////////////////////////////////////////////////////////
void PrepareFakeMidiMsg(uint8_t buf[]) {
  buf[0]  = 0x8F ;
  buf[1]  = 0x00 ;
  buf[2]  = 0x00 ;
}

///////////////////////////////////////////////////////////////////////////////
// Set pad colors
///////////////////////////////////////////////////////////////////////////////
// 2 implementations : call with a 32 bits color int value or with r,g,b values
void SetPadColor(const uint8_t mpcId, const uint8_t padNumber, const uint8_t r,const uint8_t g,const uint8_t b ) {

  uint8_t sysexBuff[128];
  int p = 0;

  // F0 47 7F [3B] 65 00 04 [Pad #] [R] [G] [B] F7

  memcpy(sysexBuff, AkaiSysex,sizeof(AkaiSysex));
  p += sizeof(AkaiSysex) ;

  // Add the current product id
  sysexBuff[p++] = mpcId ;

  // Add the pad color fn and pad number and color
  memcpy(&sysexBuff[p], MPCSysexPadColorFn,sizeof(MPCSysexPadColorFn));
  sysexBuff[p++] = padNumber ;

  // #define COLOR_CORAL      00FF0077

  sysexBuff[p++] = r ;
  sysexBuff[p++] = g ;
  sysexBuff[p++] = b ;

  sysexBuff[p++]  = 0xF7;

  // Send the sysex to the MPC controller
  snd_rawmidi_write(rawvirt_outpriv,sysexBuff,p);

}

void SetPadColorFromColorInt(const uint8_t mpcId, const uint8_t padNumber, const uint32_t rgbColorValue) {

  // Colors R G B max value is 7f in SYSEX. So the bit 8 is always set to 0.

  uint8_t r = ( rgbColorValue >> 16 ) & 0x7F ;
  uint8_t g = ( rgbColorValue >> 8  ) & 0x7F ;
  uint8_t b = rgbColorValue & 0x7F;

  SetPadColor(mpcId, padNumber, r, g , b );

}


///////////////////////////////////////////////////////////////////////////////
// Get an ALSA sequencer client containing a name
///////////////////////////////////////////////////////////////////////////////
int GetSeqClientFromPortName(const char * name) {

	if ( name == NULL) return -1;
	char port_name[128];

	snd_seq_t *seq;
	if (orig_snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		fprintf(stderr,"*** Error : impossible to open default seq\n");
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
		fprintf(stderr,"*** Error : impossible to open default seq\n");
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
        //fprintf(stdout,"client %s -- %d port %s %d\n",snd_seq_client_info_get_name(cinfo) ,r, snd_seq_port_info_get_name(pinfo), snd_seq_port_info_get_port(pinfo) );
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
		tklog_error("Impossible to open default seq\n");
		return -1;
	}

	if ((client = snd_seq_client_id(seq)) < 0) {
		tklog_error("Impossible to get seq client id\n");
		snd_seq_close(seq);
		return - 1;
	}

	/* set client info */
	if (snd_seq_set_client_name(seq, "ALSA Connector") < 0) {
		tklog_error("Set client name failed\n");
		snd_seq_close(seq);
		return -1;
	}

	/* set subscription */
	sprintf(addr,"%d:%d",src_client,src_port);
	if (snd_seq_parse_address(seq, &sender, addr) < 0) {
		snd_seq_close(seq);
		tklog_error("Invalid source address %s\n", addr);
		return -1;
	}

	sprintf(addr,"%d:%d",dest_client,dest_port);
	if (snd_seq_parse_address(seq, &dest, addr) < 0) {
		snd_seq_close(seq);
		tklog_error("Invalid destination address %s\n", addr);
		return -1;
	}

	snd_seq_port_subscribe_alloca(&subs);
	snd_seq_port_subscribe_set_sender(subs, &sender);
	snd_seq_port_subscribe_set_dest(subs, &dest);
	snd_seq_port_subscribe_set_queue(subs, queue);
	snd_seq_port_subscribe_set_exclusive(subs, exclusive);
	snd_seq_port_subscribe_set_time_update(subs, convert_time);
	snd_seq_port_subscribe_set_time_real(subs, convert_real);

	if (snd_seq_get_port_subscription(seq, subs) == 0) {
		snd_seq_close(seq);
    tklog_info("Connection of midi port %d:%d to %d:%d already subscribed\n",src_client,src_port,dest_client,dest_port);
		return 0;
	}

  if (snd_seq_subscribe_port(seq, subs) < 0) {
		snd_seq_close(seq);
    tklog_error("Connection of midi port %d:%d to %d:%d failed !\n",src_client,src_port,dest_client,dest_port);
		return 1;
	}

  tklog_info("Connection of midi port %d:%d to %d:%d successfull\n",src_client,src_port,dest_client,dest_port);


	snd_seq_close(seq);
}


///////////////////////////////////////////////////////////////////////////////
// Get MPC hardware name from sysex id
///////////////////////////////////////////////////////////////////////////////

static int GetIndexOfMPCId(uint8_t id){
	for (int i = 0 ; i < _END_MPCID ; i++ )
		if ( DeviceInfoBloc[i].sysexId == id ) return i;
	return -1;
}

const char * GetHwNameFromMPCId(uint8_t id){
	int i = GetIndexOfMPCId(id);
	if ( i >= 0) return DeviceInfoBloc[i].productString ;
	else return NULL;
}


///////////////////////////////////////////////////////////////////////////////
// Show MPCMAPPER HELP
///////////////////////////////////////////////////////////////////////////////
void ShowHelp(void) {

  tklog_info("\n") ;
  tklog_info("--tgkl_help               : Show this help\n") ;
  tklog_info("--tkgl_ctrlname=<name>    : Use external controller containing <name>\n") ;
  tklog_info("--tkgl_iamX               : Emulate MPC X\n") ;
  tklog_info("--tkgl_iamLive            : Emulate MPC Live\n") ;
  tklog_info("--tkgl_iamForce           : Emulate Force\n") ;
  tklog_info("--tkgl_iamOne             : Emulate MPC One\n") ;
  tklog_info("--tkgl_iamLive2           : Emulate MPC Live Mk II\n") ;
  tklog_info("--tkgl_virtualport=<name> : Create end user virtual port <name>\n") ;
  tklog_info("--tkgl_mididump           : Dump original raw midi flow\n") ;
  tklog_info("--tkgl_mididumpPost       : Dump raw midi flow after transformation\n") ;
  tklog_info("--tkgl_configfile=<name>  : Use configuration file <name>\n") ;
  tklog_info("\n") ;
  exit(0);
}


///////////////////////////////////////////////////////////////////////////////
// Setup tkgl anyctrl
///////////////////////////////////////////////////////////////////////////////
static void tkgl_init()
{

  // System call hooks
  orig_open64 = dlsym(RTLD_NEXT, "open64");
  //orig_open   = dlsym(RTLD_NEXT, "open");
  orig_close  = dlsym(RTLD_NEXT, "close");
  //orig_read   = dlsym(RTLD_NEXT, "read");

	// Alsa hooks
	orig_snd_rawmidi_open           = dlsym(RTLD_NEXT, "snd_rawmidi_open");
	orig_snd_rawmidi_read           = dlsym(RTLD_NEXT, "snd_rawmidi_read");
	orig_snd_rawmidi_write          = dlsym(RTLD_NEXT, "snd_rawmidi_write");
	orig_snd_seq_create_simple_port = dlsym(RTLD_NEXT, "snd_seq_create_simple_port");
	orig_snd_midi_event_decode      = dlsym(RTLD_NEXT, "snd_midi_event_decode");
  orig_snd_seq_open               = dlsym(RTLD_NEXT, "snd_seq_open");
  orig_snd_seq_port_info_set_name = dlsym(RTLD_NEXT, "snd_seq_port_info_set_name");
  //orig_snd_seq_event_input        = dlsym(RTLD_NEXT, "snd_seq_event_input");

  // Read product code
  char product_code[4];
  int fd = open(PRODUCT_CODE_PATH,O_RDONLY);
  read(fd,&product_code,4);

  // Find the id in the product code table
  for (int i = 0 ; i < _END_MPCID; i++) {
    if ( strncmp(DeviceInfoBloc[i].productCode,product_code,4) == 0 ) {
      MPCOriginalId = i;
      break;
    }
  }
  if ( MPCOriginalId < 0) {
    tklog_fatal("Error when reading the product-code file\n");
    exit(1);
  }
  tklog_info("Original Product code : %s (%s)\n",DeviceInfoBloc[MPCOriginalId].productCode,DeviceInfoBloc[MPCOriginalId].productString);

  if ( MPCSpoofedID >= 0 ) {
    tklog_info("Product code spoofed to %s (%s)\n",DeviceInfoBloc[MPCSpoofedID].productCode,DeviceInfoBloc[MPCSpoofedID].productString);
    MPCId = MPCSpoofedID ;
  } else MPCId = MPCOriginalId ;

  // Fake the power supply ?
  if ( DeviceInfoBloc[MPCOriginalId].fakePowerSupply ) tklog_info("The power supply will be faked to allow battery mode.\n");

  // read mapping config file if any
  LoadMappingFromConfFile(configFileName);

	// Initialize card id for public and private
	mpc_midi_card = GetCardFromShortName(CTRL_MPC_ALL);
	if ( mpc_midi_card < 0 ) {
			tklog_fatal("Error : MPC controller card not found (regex pattern is '%s')\n",CTRL_MPC_ALL);
			exit(1);
	}

	// Get MPC seq
	// Public is port 0, Private is port 1
	mpc_seq_client = GetSeqClientFromPortName("Private");
	if ( mpc_seq_client  < 0 ) {
		tklog_fatal("MPC controller seq client not found\n");
		exit(1);
	}

	sprintf(mpc_midi_private_alsa_name,"hw:%d,0,1",mpc_midi_card);
	sprintf(mpc_midi_public_alsa_name,"hw:%d,0,0",mpc_midi_card);
	tklog_info("MPC controller card id hw:%d found\n",mpc_midi_card);
	tklog_info("MPC controller Private port is %s\n",mpc_midi_private_alsa_name);
	tklog_info("MPC controller Public port is %s\n",mpc_midi_public_alsa_name);
	tklog_info("MPC controller seq client is %d\n",mpc_seq_client);

	// Get our controller seq  port client
	//const char * port_name = getenv("ANYCTRL_NAME") ;
	if ( anyctrl_name  != NULL) {

		// Initialize card id for public and private
		seqanyctrl_client = GetSeqClientFromPortName(anyctrl_name);
    anyctrl_midi_card = GetCardFromShortName(anyctrl_name);

		if ( seqanyctrl_client  < 0 || anyctrl_midi_card < 0 ) {
			tklog_fatal("%s seq client or card not found\n",anyctrl_name);
			exit(1);
		}
		tklog_info("%s connect port is %d:0\n",anyctrl_name,seqanyctrl_client);
    tklog_info("%s card id hw:%d found\n",anyctrl_name,anyctrl_midi_card);

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
		tklog_fatal("Impossible to create one or many virtual ports\n");
		exit(1);
	}

  tklog_info("Virtual private input port %d  created.\n",seqvirt_client_inpriv);
	tklog_info("Virtual private output port %d created.\n",seqvirt_client_outpriv);
	tklog_info("Virtual public output port %d created.\n",seqvirt_client_outpub);


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
      tklog_fatal("Impossible to create virtual user port %s\n",user_virtual_portname);
  		exit(1);
    }
    tklog_info("Virtual user port %s succesfully created.\n",user_virtual_portname);
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
			if ( c == 0 && idx >= 0) tklog_trace("");
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


////////////////////////////////////////////////////////////////////////////////
// RawMidi dump
////////////////////////////////////////////////////////////////////////////////
static void RawMidiDump(snd_rawmidi_t *rawmidi, char io,char rw,const uint8_t* data, size_t sz) {

  const char *name = snd_rawmidi_name(rawmidi);

  tklog_trace("%s dump snd_rawmidi_%s from controller %s\n",io == 'i'? "Entry":"Post",rw == 'r'? "read":"write",name);
  ShowBufferHexDump(data, sz,16);
  tklog_trace("\n");

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
    fprintf(stdout,"\n%s",TKGL_LOGO);
    tklog_info("---------------------------------------------------------\n");
  	tklog_info("TKGL_MPCMAPPER Version : %s\n",VERSION);
    tklog_info("(c) The KikGen Labs.\n");
    tklog_info("https://github.com/TheKikGen/MPC-LiveXplore\n");
  	tklog_info("---------------------------------------------------------\n");

    // Show the command line
    tklog_info("MPC args : ") ;

    for ( int i = 1 ; i < argc ; i++ ) {
      fprintf(stdout, "%s ", argv[i] ) ;
    }
    fprintf(stdout, "\n") ;
    tklog_info("\n") ;

    // Scan command line
    char * tkgl_SpoofArg = NULL;

    for ( int i = 1 ; i < argc ; i++ ) {

      // help
      if ( ( strcmp("--tkgl_help",argv[i]) == 0 ) ) {
         ShowHelp();
      }
      else
      if ( ( strncmp("--tkgl_ctrlname=",argv[i],16) == 0 ) && ( strlen(argv[i]) >16 ) ) {
         anyctrl_name = argv[i] + 16;
         tklog_info("--tgkl_ctrlname specified for %s midi controller\n",anyctrl_name) ;
      }
      else
      // Spoofed product id
      if ( ( strcmp("--tkgl_iamX",argv[i]) == 0 ) ) {
        MPCSpoofedID = MPC_X;
        tkgl_SpoofArg = argv[i];
      }
      else
      if ( ( strcmp("--tkgl_iamLive",argv[i]) == 0 ) ) {
        MPCSpoofedID = MPC_LIVE;
        tkgl_SpoofArg = argv[i];
      }
      else
      if ( ( strcmp("--tkgl_iamForce",argv[i]) == 0 ) ) {
        MPCSpoofedID = MPC_FORCE;
        tkgl_SpoofArg = argv[i];
      }
      else
      if ( ( strcmp("--tkgl_iamOne",argv[i]) == 0 ) ) {
        MPCSpoofedID = MPC_ONE;
        tkgl_SpoofArg = argv[i];
      }
      else
      if ( ( strcmp("--tkgl_iamLive2",argv[i]) == 0 ) ) {
        MPCSpoofedID = MPC_LIVE_MK2;
        tkgl_SpoofArg = argv[i];
      }
      else
      // End user virtual port visible from the MPC app
      if ( ( strncmp("--tkgl_virtualport=",argv[i],19) == 0 ) && ( strlen(argv[i]) >19 ) ) {
         user_virtual_portname = argv[i] + 19;
         tklog_info("--tkgl_virtualport specified as %s port name\n",user_virtual_portname) ;
      }
      else
      // Dump rawmidi
      if ( ( strcmp("--tkgl_mididump",argv[i]) == 0 ) ) {
        rawMidiDumpFlag = 1 ;
        tklog_info("--tkgl_mididump specified : dump original raw midi message (ENTRY)\n") ;
      }
      else
      if ( ( strcmp("--tkgl_mididumpPost",argv[i]) == 0 ) ) {
        rawMidiDumpPostFlag = 1 ;
        tklog_info("--tkgl_mididumpPost specified : dump raw midi message after transformation (POST)\n") ;
      }
      else
      // Config file name
      if ( ( strncmp("--tkgl_configfile=",argv[i],18) == 0 ) && ( strlen(argv[i]) >18 )  ) {
        configFileName = argv[i] + 18 ;
        tklog_info("--tkgl_configfile specified. File %s will be used for mapping\n",configFileName) ;
      }


    }

    if ( MPCSpoofedID >= 0 ) {
      tklog_info("%s specified. %s spoofing.\n",tkgl_SpoofArg,DeviceInfoBloc[MPCSpoofedID].productString ) ;
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

	//tklog_info("snd_rawmidi_open name %s mode %d\n",name,mode);

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

    //tklog_info("PORT ID IS %d\n",r);

    return r;

  }

	// Substitute the hardware private input port by our input virtual ports

	else if ( strcmp(mpc_midi_private_alsa_name,name) == 0   ) {

		// Private In
		if (inputp) *inputp = rawvirt_inpriv;
		else if (outputp) *outputp = rawvirt_outpriv ;
		else return -1;
		tklog_info("%s substitution by virtual rawmidi successfull\n",name);

		return 0;
	}

	else if ( strcmp(mpc_midi_public_alsa_name,name) == 0   ) {

		if (outputp) *outputp = rawvirt_outpub;
		else return -1;
		tklog_info("%s substitution by virtual rawmidi successfull\n",name);

		return 0;
	}

	return orig_snd_rawmidi_open(inputp, outputp, name, mode);
}


///////////////////////////////////////////////////////////////////////////////
// Refresh MPC pads colors from Force PAD Colors cache
///////////////////////////////////////////////////////////////////////////////
static void Mpc_ResfreshPadsColorFromForceCache(uint8_t padL, uint8_t padC, uint8_t nbLine) {

  // Write again the color like a Force.
  // The midi modification will be done within the corpse of the hooked fn.
  // Pads from 64 are columns pads

  uint8_t sysexBuff[12] = { 0xF0, 0x47, 0x7F, 0x40, 0x65, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0xF7};
  //                                                                 [Pad #] [R]   [G]   [B]

  for ( int l = 0 ; l< nbLine ; l++ ) {

    for ( int c = 0 ; c < 4 ; c++ ) {

      int padF = ( l + padL ) * 8 + c + padC;
      sysexBuff[7] = padF;
      sysexBuff[8] = PadSysexColorsCache[padF].r ;
      sysexBuff[9] = PadSysexColorsCache[padF].g;
      sysexBuff[10] = PadSysexColorsCache[padF].b;
      // Send the sysex to the MPC controller
      snd_rawmidi_write(rawvirt_outpriv,sysexBuff,sizeof(sysexBuff));

    }

  }

}

///////////////////////////////////////////////////////////////////////////////
// Show the current MPC quadran within the Force matrix
///////////////////////////////////////////////////////////////////////////////
static void Mpc_ShowForceMatrixQuadran(uint8_t forcePadL, uint8_t forcePadC) {

  uint8_t sysexBuff[12] = { 0xF0, 0x47, 0x7F, 0x40, 0x65, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0xF7};
  //                                                                 [Pad #] [R]   [G]   [B]
  sysexBuff[3] = DeviceInfoBloc[MPCOriginalId].sysexId;

  uint8_t q = ( forcePadL == 4 ? 0 : 1 ) * 4 + ( forcePadC == 4 ? 3 : 2 ) ;

  for ( int l = 0 ; l < 2 ; l++ ) {
    for ( int c = 0 ; c < 2 ; c++ ) {
      sysexBuff[7] = l * 4 + c + 2 ;
      if ( sysexBuff[7] == q ) {
        sysexBuff[8]  =  0x7F ;
        sysexBuff[9]  =  0x7F ;
        sysexBuff[10] =  0x7F ;
      }
      else {
        sysexBuff[8]  =  0x7F ;
        sysexBuff[9]  =  0x00 ;
        sysexBuff[10] =  0x00 ;
      }
      //tklog_info("[tkgl] MPC Pad quadran : l,c %d,%d Pad %d r g b %02X %02X %02X\n",forcePadL,forcePadC,sysexBuff[7],sysexBuff[8],sysexBuff[9],sysexBuff[10]);

      orig_snd_rawmidi_write(rawvirt_outpriv,sysexBuff,sizeof(sysexBuff));
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Draw a pad line on MPC pads from a Force PAD line in the current Colors cache
///////////////////////////////////////////////////////////////////////////////
void Mpc_DrawPadLineFromForceCache(uint8_t forcePadL, uint8_t forcePadC, uint8_t mpcPadL) {

  uint8_t sysexBuff[12] = { 0xF0, 0x47, 0x7F, 0x40, 0x65, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0xF7};
  //                                                                 [Pad #] [R]   [G]   [B]
  sysexBuff[3] = DeviceInfoBloc[MPCOriginalId].sysexId;

  uint8_t p = forcePadL*8 + forcePadC ;

  for ( int c = 0 ; c < 4 ; c++ ) {
    sysexBuff[7] = mpcPadL * 4 + c ;
    p = forcePadL*8 + c + forcePadC  ;
    sysexBuff[8]  = PadSysexColorsCache[p].r ;
    sysexBuff[9]  = PadSysexColorsCache[p].g;
    sysexBuff[10] = PadSysexColorsCache[p].b;

    //tklog_info("[tkgl] MPC Pad Line refresh : %d r g b %02X %02X %02X\n",sysexBuff[7],sysexBuff[8],sysexBuff[9],sysexBuff[10]);

    orig_snd_rawmidi_write(rawvirt_outpriv,sysexBuff,sizeof(sysexBuff));
  }
}

///////////////////////////////////////////////////////////////////////////////
// MIDI READ - APP ON MPC READING AS FORCE
///////////////////////////////////////////////////////////////////////////////
static size_t Mpc_MapReadFromForce(void *midiBuffer, size_t maxSize,size_t size) {

    uint8_t * myBuff = (uint8_t*)midiBuffer;

    size_t i = 0 ;
    while  ( i < size ) {

      // AKAI SYSEX ------------------------------------------------------------
      // IDENTITY REQUEST
      if (  myBuff[i] == 0xF0 &&  memcmp(&myBuff[i],IdentityReplySysexHeader,sizeof(IdentityReplySysexHeader)) == 0 ) {
        // If so, substitue sysex identity request by the faked one
        memcpy(&myBuff[i+sizeof(IdentityReplySysexHeader)],DeviceInfoBloc[MPCId].sysexIdReply, sizeof(DeviceInfoBloc[MPCId].sysexIdReply) );
        i += sizeof(IdentityReplySysexHeader) + sizeof(DeviceInfoBloc[MPCId].sysexIdReply) ;
      }

      // KNOBS TURN ------------------------------------------------------------
      // If it's a shift + knob turn, add an offset
      //  B0 [10-31] [7F - n]
      else
      if (  myBuff[i] == 0xB0 ) {

        if ( shiftHoldedMode && QlinkKnobsShiftMode && DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount < 16
                  && myBuff[i+1] >= 0x10 && myBuff[i+1] <= 0x31 )
            myBuff[i+1] +=  DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount;

        i += 3;
      }

      // BUTTONS - LEDS --------------------------------------------------------
      // In that direction, it's a button press/release
      // Check if we must remap...
      else
      if (  myBuff[i] == 0x90   ) { //|| myBuff[i] == 0x80
        //tklog_info("%d button pressed/released (%02x) \n",myBuff[i+1], myBuff[i+2]);

        // Shift is an exception and  mapping is ignored (the SHIFT button can't be mapped)
        // Double click on SHIFT is not managed at all. Avoid it.
        if ( myBuff[i+1] == SHIFT_KEY_VALUE ) {
            shiftHoldedMode = ( myBuff[i+2] == 0x7F ? true:false ) ;
  //          tklog_info("%d SHIFT MODE\n", shiftHoldedMode   );
        }

        // SHIFT button is current holded
        // SHIFT double click on the MPC side is not taken into account for the moment
        else
        if ( shiftHoldedMode ) {

          // KNOB TOUCH : If it's a shift + knob "touch", add the offset
          // 90 [54-63] 7F
          if ( QlinkKnobsShiftMode && DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount < 16
                  && myBuff[i+1] >= 0x54 && myBuff[i+1] <= 0x63 )
                 myBuff[i+1] += DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount;

          // Look for shift mapping above 0x7F
          if ( map_ButtonsLeds[ myBuff[i+1] + 0x80  ] >= 0 ) {

              uint8_t mapValue = map_ButtonsLeds[ myBuff[i+1] + 0x80 ];

  //             tklog_info("Shift Button %d\n",mapValue);
              // If the SHIFT mapping is also activated at destination, and as the shift key
              // is currently holded, we send only the corresponding button, that will generate the shift + button code,
              // Otherwise, we must release the shift key, by inserting a shift key note off

              // Shift mapped also at destination
              if ( mapValue >= 0x80 ) {
                  myBuff[i+1] = mapValue - 0x80;
              }
              else {
                  // We are holding shift, but the dest key is not a SHIFT mapping
                  // insert SHIFT BUTTON note off in the midi buffer
                  // (we assume brutally we have room; Should check max size)
                  if ( size > maxSize - 3 ) fprintf(stdout,"Warning : midi buffer overflow when inserting SHIFT note off !!\n");
                  memcpy( &myBuff[i + 3 ], &myBuff[i], size - i );
                  size +=3;

                  myBuff[i + 1] = SHIFT_KEY_VALUE ;
                  myBuff[i + 2] = 0x00 ; // Button released
                  i += 3;

                  // Now, map our Key
                  myBuff[ i + 1 ] = mapValue;
              }

          }
          else {
              // If no shift mapping, use the normal mapping
              myBuff[i+1] = map_ButtonsLeds[ myBuff[i+1] ];
          }
        } // Shiftmode

        else
        if ( map_ButtonsLeds[ myBuff[i+1] ] >= 0 ) {
          //tklog_info("MAP %d->%d\n",myBuff[i+1],map_ButtonsLeds[ myBuff[i+1] ]);
          myBuff[i+1] = map_ButtonsLeds[ myBuff[i+1] ];
        }

        // Post mapping : activate the special column mode when Force spoofed on a MPC
        // Colum mode Button holded
        if ( myBuff[i+2] == 0x7F ) {
            uint8_t mapValue = myBuff[i+1] ;

              switch (myBuff[i+1]) {
                case FORCE_ASSIGN_A:
                case FORCE_ASSIGN_B:
                case FORCE_MUTE:
                case FORCE_SOLO:
                case FORCE_REC_ARM:
                case FORCE_CLIP_STOP:
                  ForceColumnMode = myBuff[i+1];
                  break;
                default:
                  ForceColumnMode = -1 ;
              }

//              tklog_info("[tkgl] COLUMN MODE ON %d\n",ForceColumnMode);
              if ( ForceColumnMode >= 0 ) {
                Mpc_DrawPadLineFromForceCache(8, MPCPad_OffsetC, 3);
                Mpc_ShowForceMatrixQuadran(MPCPad_OffsetL, MPCPad_OffsetC);
              }

        }
        // Button released
        else {

    //         tklog_info("COLUMN MODE OFF %d\n",ForceColumnMode);
             ForceColumnMode = -1 ;
             Mpc_ResfreshPadsColorFromForceCache(MPCPad_OffsetL,MPCPad_OffsetC,4);
        }

        i += 3;

      }

      // PADS ------------------------------------------------------------------
      else
      if (  myBuff[i] == 0x99 || myBuff[i] == 0x89 || myBuff[i] == 0xA9 ) {

        // Remap MPC hardware pad
        // Get MPC pad id in the true order
        uint8_t padM = MPCPadsTable[myBuff[i+1] - MPCPADS_TABLE_IDX_OFFSET ];
        uint8_t padL = padM / 4  ;
        uint8_t padC = padM % 4  ;

        // Compute the Force pad id without offset
        uint8_t padF = ( 3 - padL + MPCPad_OffsetL ) * 8 + padC + MPCPad_OffsetC ;

        // Check if SHIFT is holded for the 0 column
        if ( shiftHoldedMode ) {
          bool buttonSimul = false; // Simulate a Force button
          uint8_t buttonValue = 0x7F;
          bool refreshPads = false;

          if ( padC == 0 ) {
            // SHIFT + PAD  in column 0 = Launch the corresponding line
            // LAUNCH_1=56
            // Replace the midi pad note on by a button on/off
            buttonValue = padF / 8 + 56; // Launch line
            buttonSimul = true;
          }
          // Navigte in the matrix with shift + Pads
          // 12     13      ( Up 15  )   16
          // 08 (Left 09 ) ( Down 10) (Right 11)
          else
          if (padM == 9  ) {
            buttonValue = FORCE_LEFT;
            buttonSimul = true;
          }
          else
          if (padM == 11  ) {
            buttonValue = FORCE_RIGHT; // Right
            buttonSimul = true;
          }
          else
          if (padM == 14  ) {
            buttonValue = FORCE_UP; // Up
            buttonSimul = true;
          }
          else
          if (padM == 10  ) {
            buttonValue = FORCE_DOWN; // Down
            buttonSimul = true;
          }
          // Navigate in the Force quadrans
          // 04 05 (06 Left top quadran   ) (07 right top    quadran)
          // 00 01 (02 Left bottom quadran) (03 right bottom quadran)
          else
          // Top Left
          if (padM == 6  ) {
            MPCPad_OffsetL = MPCPad_OffsetC = 0;
            refreshPads = true;
          }
          else
          // Top Right
          if (padM == 7  ) {
            MPCPad_OffsetL = 0;
            MPCPad_OffsetC = 4;
            refreshPads = true;
          }
          else
          // bottom left
          if (padM == 2 ) {
            MPCPad_OffsetL = 4;
            MPCPad_OffsetC = 0;
            refreshPads = true;
          }
          else
          // bottom right
          if (padM == 3 ) {
            MPCPad_OffsetL = 4;
            MPCPad_OffsetC = 4;
            refreshPads = true;
          }

          // Simulate a button press/release
          if ( buttonSimul && myBuff[i] != 0xA9 ) {
            myBuff[i+2] = ( myBuff[i] == 0x99 ? 0x7F:0x00 ) ;
            myBuff[i]   = 0x90; // MPC Button
            //tklog_info("remapped to %02x %02x %02x  ! \n",myBuff[i],myBuff[i+1],myBuff[i+2]);
            myBuff[i+1] = buttonValue;

          }
          else {
            // Generate a fake midi message
            PrepareFakeMidiMsg(&myBuff[i]);
          }

          // Update the MPC pad colors from Force pad colors cache
          if ( refreshPads )  Mpc_ResfreshPadsColorFromForceCache(MPCPad_OffsetL,MPCPad_OffsetC,4);

        } // Shitfmode

        // Check if  columns solo mute... mode
        else if ( ForceColumnMode >= 0 ) {

          // Generate the "column" button event press or release
          myBuff[i+2] = ( myBuff[i] == 0x99 ? 0x7F:0x00 ) ;
          myBuff[i]   = 0x90; // MPC Button
          // Pads botom line  90 29-30 00/7f
          myBuff[i+1] = 0x29 + padC + MPCPad_OffsetC;
        }

        else myBuff[i+1] = padF + FORCEPADS_TABLE_IDX_OFFSET;


        i += 3;

      }

      else i++;

    }

    return size;
}

///////////////////////////////////////////////////////////////////////////////
// MIDI READ - APP ON MPC READING AS MPC
///////////////////////////////////////////////////////////////////////////////
static size_t Mpc_MapReadFromMpc(void *midiBuffer, size_t maxSize,size_t size) {

    uint8_t * myBuff = (uint8_t*)midiBuffer;

    size_t i = 0 ;
    while  ( i < size ) {

      // AKAI SYSEX ------------------------------------------------------------
      // IDENTITY REQUEST
      if (  myBuff[i] == 0xF0 &&  memcmp(&myBuff[i],IdentityReplySysexHeader,sizeof(IdentityReplySysexHeader)) == 0 ) {
        // If so, substitue sysex identity request by the faked one
        memcpy(&myBuff[i+sizeof(IdentityReplySysexHeader)],DeviceInfoBloc[MPCId].sysexIdReply, sizeof(DeviceInfoBloc[MPCId].sysexIdReply) );
        i += sizeof(IdentityReplySysexHeader) + sizeof(DeviceInfoBloc[MPCId].sysexIdReply) ;
      }

      // KNOBS TURN ------------------------------------------------------------
      else
      if (  myBuff[i] == 0xB0 ) {
        if ( shiftHoldedMode && QlinkKnobsShiftMode && DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount < 16
                  && myBuff[i+1] >= 0x10 && myBuff[i+1] <= 0x31 )
            myBuff[i+1] +=  DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount;
        i += 3;
      }

      // BUTTONS - LEDS --------------------------------------------------------
      else
      if (  myBuff[i] == 0x90   ) { //|| myBuff[i] == 0x80
        // Shift is an exception and  mapping is ignored (the SHIFT button can't be mapped)
        // Double click on SHIFT is not managed at all. Avoid it.
        if ( myBuff[i+1] == SHIFT_KEY_VALUE ) {
            shiftHoldedMode = ( myBuff[i+2] == 0x7F ? true:false ) ;
        }
        else
        // SHIFT button is current holded
        // SHIFT double click on the MPC side is not taken into account for the moment
        if ( shiftHoldedMode ) {

          // KNOB TOUCH : If it's a shift + knob "touch", add the offset
          // 90 [54-63] 7F
          if ( QlinkKnobsShiftMode && DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount < 16
                  && myBuff[i+1] >= 0x54 && myBuff[i+1] <= 0x63 )
                 myBuff[i+1] += DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount;

          // Look for shift mapping above 0x7F
          if ( map_ButtonsLeds[ myBuff[i+1] + 0x80  ] >= 0 ) {

              uint8_t mapValue = map_ButtonsLeds[ myBuff[i+1] + 0x80 ];

              // If the SHIFT mapping is also activated at destination, and as the shift key
              // is currently holded, we send only the corresponding button, that will generate the shift + button code,
              // Otherwise, we must release the shift key, by inserting a shift key note off

              // Shift mapped also at destination
              if ( mapValue >= 0x80 ) {
                  myBuff[i+1] = mapValue - 0x80;
              }
              else {
                  // We are holding shift, but the dest key is not a SHIFT mapping
                  // insert SHIFT BUTTON note off in the midi buffer
                  // (we assume brutally we have room; Should check max size)
                  if ( size > maxSize - 3 ) fprintf(stdout,"Warning : midi buffer overflow when inserting SHIFT note off !!\n");
                  memcpy( &myBuff[i + 3 ], &myBuff[i], size - i );
                  size +=3;

                  myBuff[i + 1] = SHIFT_KEY_VALUE ;
                  myBuff[i + 2] = 0x00 ; // Button released
                  i += 3;

                  // Now, map our Key
                  myBuff[ i + 1 ] = mapValue;
              }

          }
          else {
              // If no shift mapping, use the normal mapping
              myBuff[i+1] = map_ButtonsLeds[ myBuff[i+1] ];
          }
        } // Shiftmode
        else
        if ( map_ButtonsLeds[ myBuff[i+1] ] >= 0 ) {
          //tklog_info("MAP %d->%d\n",myBuff[i+1],map_ButtonsLeds[ myBuff[i+1] ]);
          myBuff[i+1] = map_ButtonsLeds[ myBuff[i+1] ];
        }

        i += 3;

      }

      // PADS -----------------------------------------------------------------
      else
      if (  myBuff[i] == 0x99 || myBuff[i] == 0x89 || myBuff[i] == 0xA9 ) {

        i += 3;

      }

      else i++;

    }

    return size;
}

///////////////////////////////////////////////////////////////////////////////
// MIDI WRITE - APP ON MPC MAPPING TO MPC
///////////////////////////////////////////////////////////////////////////////
static void Mpc_MapAppWriteToMpc(const void *midiBuffer, size_t size) {

  uint8_t * myBuff = (uint8_t*)midiBuffer;

  size_t i = 0 ;
  while  ( i < size ) {

    // AKAI SYSEX
    // If we detect the Akai sysex header, change the harwware id by our true hardware id.
    // Messages are compatibles. Some midi msg are not always interpreted (e.g. Oled)
    if (  myBuff[i] == 0xF0 &&  memcmp(&myBuff[i],AkaiSysex,sizeof(AkaiSysex)) == 0 ) {
        // Update the sysex id in the sysex for our original hardware
        i += sizeof(AkaiSysex) ;
        myBuff[i] = DeviceInfoBloc[MPCOriginalId].sysexId; // MPC are several...
        i++;

          // SET PAD COLORS SYSEX FN
          // F0 47 7F [3B] -> 65 00 04 [Pad #] [R] [G] [B] F7
        if ( memcmp(&myBuff[i],MPCSysexPadColorFn,sizeof(MPCSysexPadColorFn)) == 0 ) {
              i += sizeof(MPCSysexPadColorFn) ;

              uint8_t padF = myBuff[i];
              // Update Force pad color cache
              PadSysexColorsCache[padF].r = myBuff[i + 1 ];
              PadSysexColorsCache[padF].g = myBuff[i + 2 ];
              PadSysexColorsCache[padF].b = myBuff[i + 3 ];

              i += 5 ; // Next msg
        }
    }
    // Buttons-Leds.  In that direction, it's a LED ON / OFF for the button
    // Check if we must remap...
    else
    if (  myBuff[i] == 0xB0  ) {

      if ( map_ButtonsLeds_Inv[ myBuff[i+1] ] >= 0 ) {
        //tklog_info("MAP INV %d->%d\n",myBuff[i+1],map_ButtonsLeds_Inv[ myBuff[i+1] ]);
        myBuff[i+1] = map_ButtonsLeds_Inv[ myBuff[i+1] ];
      }
      i += 3;
    }
    else i++;
  }
}

///////////////////////////////////////////////////////////////////////////////
// MIDI WRITE - APP ON MPC MAPPING TO FORCE
///////////////////////////////////////////////////////////////////////////////
static void Mpc_MapAppWriteToForce(const void *midiBuffer, size_t size) {

  uint8_t * myBuff = (uint8_t*)midiBuffer;

  bool refreshMutePadLine = false;

  size_t i = 0 ;
  while  ( i < size ) {

    // AKAI SYSEX
    // If we detect the Akai sysex header, change the harwware id by our true hardware id.
    // Messages are compatibles. Some midi msg are not always interpreted (e.g. Oled)
    if (  myBuff[i] == 0xF0 &&  memcmp(&myBuff[i],AkaiSysex,sizeof(AkaiSysex)) == 0 ) {
        // Update the sysex id in the sysex for our original hardware
        i += sizeof(AkaiSysex) ;
        myBuff[i] = DeviceInfoBloc[MPCOriginalId].sysexId;
        i++;

        // SET PAD COLORS SYSEX FN  F0 47 7F [3B] -> 65 00 04 [Pad #] [R] [G] [B] F7
        if ( memcmp(&myBuff[i],MPCSysexPadColorFn,sizeof(MPCSysexPadColorFn)) == 0 ) {
            i += sizeof(MPCSysexPadColorFn) ;

            uint8_t padF = myBuff[i];
            uint8_t padL = padF / 8 ;
            uint8_t padC = padF % 8 ;

            // Update Force pad color cache
            PadSysexColorsCache[padF].r = myBuff[i + 1 ];
            PadSysexColorsCache[padF].g = myBuff[i + 2 ];
            PadSysexColorsCache[padF].b = myBuff[i + 3 ];

            // Apply eventulal L,C pad offset if MPC
            padF = 0x7F; // set the default pad color to an unknow pad #
            if ( padL >= MPCPad_OffsetL && padL < MPCPad_OffsetL + 4 ) {
              if ( padC >= MPCPad_OffsetC  && padC < MPCPad_OffsetC + 4 ) {
                //tklog_info("Pad (%d,%d) In the quadran (%d,%d)\n",padL,padC,MPCPad_OffsetL,MPCPad_OffsetC);
                padF = (  3 - ( padL - MPCPad_OffsetL  ) ) * 4 + ( padC - MPCPad_OffsetC)  ;
              }
            }

            // Update the midi buffer
            myBuff[i] = padF;

            i += 5 ; // Next msg
        }

    }

    // Buttons-Leds.  In that direction, it's a LED ON / OFF for the button
    // Check if we must remap...

    else
    if (  myBuff[i] == 0xB0  ) {
      if ( map_ButtonsLeds_Inv[ myBuff[i+1] ] >= 0 ) {
        //tklog_info("MAP INV %d->%d\n",myBuff[i+1],map_ButtonsLeds_Inv[ myBuff[i+1] ]);
        myBuff[i+1] = map_ButtonsLeds_Inv[ myBuff[i+1] ];
      }
      i += 3;
    }

    else i++;
  }

  // Check if we must refresh the pad mutes line on the MPC
  if ( ForceColumnMode >= 0 ) {
    Mpc_DrawPadLineFromForceCache(8, MPCPad_OffsetC, 3);
    Mpc_ShowForceMatrixQuadran(MPCPad_OffsetL, MPCPad_OffsetC);
  }
}

///////////////////////////////////////////////////////////////////////////////
// MIDI READ - APP ON FORCE READING AS ITSELF
///////////////////////////////////////////////////////////////////////////////
static size_t Force_MapReadFromForce(void *midiBuffer, size_t maxSize,size_t size) {

    uint8_t * myBuff = (uint8_t*)midiBuffer;

    size_t i = 0 ;
    while  ( i < size ) {

      // AKAI SYSEX ------------------------------------------------------------
      // IDENTITY REQUEST REPLY
      if (  myBuff[i] == 0xF0 &&  memcmp(&myBuff[i],IdentityReplySysexHeader,sizeof(IdentityReplySysexHeader)) == 0 ) {
        // If so, substitue sysex identity request by the faked one
        i += sizeof(IdentityReplySysexHeader) + sizeof(DeviceInfoBloc[MPCId].sysexIdReply) ;
      }

      // KNOBS TURN ------------------------------------------------------------
      //  B0 [10-31] [7F - n]
      else
      if (  myBuff[i] == 0xB0 ) {

        i += 3;
      }


      // BUTTONS - LEDS --------------------------------------------------------

      else
      if (  myBuff[i] == 0x90   ) { //|| myBuff[i] == 0x80

        // Shift is an exception and  mapping is ignored (the SHIFT button can't be mapped)
        // Double click on SHIFT is not managed at all. Avoid it.
        if ( myBuff[i+1] == SHIFT_KEY_VALUE ) {
            shiftHoldedMode = ( myBuff[i+2] == 0x7F ? true:false ) ;
        }
        else
        // SHIFT button is current holded
        // SHIFT double click on the MPC side is not taken into account for the moment
        if ( shiftHoldedMode ) {

          // KNOB TOUCH : If it's a shift + knob "touch", add the offset
          // 90 [54-63] 7F
          if ( QlinkKnobsShiftMode && DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount < 16
                  && myBuff[i+1] >= 0x54 && myBuff[i+1] <= 0x63 )
                 myBuff[i+1] += DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount;

          // Look for shift mapping above 0x7F
          if ( map_ButtonsLeds[ myBuff[i+1] + 0x80  ] >= 0 ) {

              uint8_t mapValue = map_ButtonsLeds[ myBuff[i+1] + 0x80 ];

              // If the SHIFT mapping is also activated at destination, and as the shift key
              // is currently holded, we send only the corresponding button, that will generate the shift + button code,
              // Otherwise, we must release the shift key, by inserting a shift key note off

              // Shift mapped also at destination
              if ( mapValue >= 0x80 ) {
                  myBuff[i+1] = mapValue - 0x80;
              }
              else {
                  // We are holding shift, but the dest key is not a SHIFT mapping
                  // insert SHIFT BUTTON note off in the midi buffer
                  // (we assume brutally we have room; Should check max size)
                  if ( size > maxSize - 3 ) fprintf(stdout,"Warning : midi buffer overflow when inserting SHIFT note off !!\n");
                  memcpy( &myBuff[i + 3 ], &myBuff[i], size - i );
                  size +=3;

                  myBuff[i + 1] = SHIFT_KEY_VALUE ;
                  myBuff[i + 2] = 0x00 ; // Button released
                  i += 3;

                  // Now, map our Key
                  myBuff[ i + 1 ] = mapValue;
              }

          }
          else {
              // If no shift mapping, use the normal mapping
              myBuff[i+1] = map_ButtonsLeds[ myBuff[i+1] ];
          }
        } // Shiftmode
        else
        if ( map_ButtonsLeds[ myBuff[i+1] ] >= 0 ) {
          //tklog_info("MAP %d->%d\n",myBuff[i+1],map_ButtonsLeds[ myBuff[i+1] ]);
          myBuff[i+1] = map_ButtonsLeds[ myBuff[i+1] ];
        }

        i += 3;

      }

      // PADS -----------------------------------------------------------------
      else
      if (  myBuff[i] == 0x99 || myBuff[i] == 0x89 || myBuff[i] == 0xA9 ) {

        i += 3;

      }

      else i++;

    }

    return size;
}

///////////////////////////////////////////////////////////////////////////////
// MIDI READ - APP ON FORCE READING AS MPC
///////////////////////////////////////////////////////////////////////////////
static size_t Force_MapReadFromMpc(void *midiBuffer, size_t maxSize,size_t size) {

    uint8_t * myBuff = (uint8_t*)midiBuffer;

    size_t i = 0 ;
    while  ( i < size ) {

// AKAI SYSEX ==================================================================

// IDENTITY REQUEST REPLAY -----------------------------------------------------

      if (  myBuff[i] == 0xF0 &&  memcmp(&myBuff[i],IdentityReplySysexHeader,sizeof(IdentityReplySysexHeader)) == 0 ) {
        // If so, substitue sysex identity request by the faked one
        memcpy(&myBuff[i+sizeof(IdentityReplySysexHeader)],DeviceInfoBloc[MPCId].sysexIdReply, sizeof(DeviceInfoBloc[MPCId].sysexIdReply) );
        i += sizeof(IdentityReplySysexHeader) + sizeof(DeviceInfoBloc[MPCId].sysexIdReply) ;
      }

// CC KNOBS TURN ---------------------------------------------------------------

      else
      if (  myBuff[i] == 0xB0 ) {
        // If it's a shift + knob turn, add an offset   B0 [10-31] [7F - n]
        if ( shiftHoldedMode && QlinkKnobsShiftMode && DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount < 16
                  && myBuff[i+1] >= 0x10 && myBuff[i+1] <= 0x31 ) {

          myBuff[i+1] +=  DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount;

        }
       // Todo mapping
        i += 3;
      }

// BUTTONS PRESSED / RELEASED --------------------------------------------------

      else
      if (  myBuff[i] == 0x90   ) { //
        // Shift is an exception and  mapping is ignored (the SHIFT button can't be mapped)
        // Double click on SHIFT is not managed at all. Avoid it.
        if ( myBuff[i+1] == SHIFT_KEY_VALUE ) {
            shiftHoldedMode = ( myBuff[i+2] == 0x7F ? true:false ) ;
        }
        else
        // SHIFT button is currently holded
        // SHIFT native double click on the MPC side is not taken into account for the moment
        if ( shiftHoldedMode ) {

          // KNOB TOUCH : If it's a shift + knob "touch", add the offset   90 [54-63] 7F
          // Before the mapping
          if ( QlinkKnobsShiftMode && DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount < 16
                  && myBuff[i+1] >= 0x54 && myBuff[i+1] <= 0x63 )
          {
            myBuff[i+1] += DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount;
          }

          // Look for shift mapping above 0x7F
          // If the SHIFT mapping is also activated at destination, and as the shift key
          // is currently holded, we send only the corresponding button, that will generate the shift + button code,
          // Otherwise, we must release the shift key, by inserting a shift key note off
          if ( map_ButtonsLeds[ myBuff[i+1] + 0x80  ] >= 0 ) {

              uint8_t mapValue = map_ButtonsLeds[ myBuff[i+1] + 0x80 ];

              // Shift mapped also at destination
              if ( mapValue >= 0x80 ) {
                  myBuff[i+1] = mapValue - 0x80;
              }
              else {
                  // We are holding shift, but the dest key is not a SHIFT mapping
                  // insert SHIFT BUTTON note off in the midi buffer
                  // (we assume brutally we have room; Should check max size)
                  if ( size > maxSize - 3 ) fprintf(stdout,"Warning : midi buffer overflow when inserting SHIFT note off !!\n");
                  memcpy( &myBuff[i + 3 ], &myBuff[i], size - i );
                  size +=3;

                  myBuff[i + 1] = SHIFT_KEY_VALUE ;
                  myBuff[i + 2] = 0x00 ; // Button released
                  i += 3;

                  // Now, map our Key
                  myBuff[ i + 1 ] = mapValue;
              }

          }
          else {
              // If no shift mapping, use the normal mapping
              myBuff[i+1] = map_ButtonsLeds[ myBuff[i+1] ] > 0 ? map_ButtonsLeds[ myBuff[i+1] ] : myBuff[i+1]  ;
          }
        } // Shif holded tmode

        // SHift not holded here
        else {

          // Remap the Force Button with the MPC Button
          if ( map_ButtonsLeds[ myBuff[i+1] ] >= 0 ) {

            myBuff[i+1] = map_ButtonsLeds[ myBuff[i+1] ];

          }
          else if ( configFileName != NULL ) {
            // No mapping in the configuration file
            // Erase bank button midi msg - Put a fake midi msg
            PrepareFakeMidiMsg(&myBuff[i]);
          }
        }

        i += 3;
      }

      // PADS -----------------------------------------------------------------
      else
      if (  myBuff[i] == 0x99 || myBuff[i] == 0x89 || myBuff[i] == 0xA9 ) {

          // Remap Force hardware pad
          uint8_t padF = myBuff[i+1] - FORCEPADS_TABLE_IDX_OFFSET ;
          uint8_t padL = padF / 8 ;
          uint8_t padC = padF % 8 ;

          if ( padC >= 2 && padC < 6  && padL > 3  ) {

            // Compute the MPC pad id
            uint8_t p = ( 3 - padL % 4 ) * 4 + (padC -2) % 4;
            myBuff[i+1] = MPCPadsTable2[p];

          } else {
            // Fake event
            PrepareFakeMidiMsg(&myBuff[i]);
          }


        i += 3;

      }

      else i++;

    }

    return size;
}

///////////////////////////////////////////////////////////////////////////////
// MIDI WRITE - APP ON FORCE MAPPING TO MPC
///////////////////////////////////////////////////////////////////////////////
static void Force_MapAppWriteToMpc(const void *midiBuffer, size_t size) {

  uint8_t * myBuff = (uint8_t*)midiBuffer;

  size_t i = 0 ;
  while  ( i < size ) {

    // AKAI SYSEX
    // If we detect the Akai sysex header, change the harwware id by our true hardware id.
    // Messages are compatibles. Some midi msg are not always interpreted (e.g. Oled)
    if (  myBuff[i] == 0xF0 &&  memcmp(&myBuff[i],AkaiSysex,sizeof(AkaiSysex)) == 0 ) {
        // Update the sysex id in the sysex for our original hardware
        i += sizeof(AkaiSysex) ;
        myBuff[i] = DeviceInfoBloc[MPCOriginalId].sysexId;
        i++;

        // PAD SET COLOR SYSEX - MPC spoofed on a Force
        // F0 47 7F [3B] -> 65 00 04 [Pad #] [R] [G] [B] F7
        if ( memcmp(&myBuff[i],MPCSysexPadColorFn,sizeof(MPCSysexPadColorFn)) == 0 ) {
          i += sizeof(MPCSysexPadColorFn) ;

          // Translate pad number
          uint8_t padM = myBuff[i];
          uint8_t padL = padM / 4 ;
          uint8_t padC = padM % 4 ;

          // Place the 4x4 in the 8x8 matrix
          padL += 0 ;
          padC += 2 ;

          myBuff[i] = ( 7 - padL ) * 8 + padC;

          i += 5 ; // Next msg
        }
    }

    // Buttons-Leds.  In that direction, it's a LED ON / OFF for the button
    else
    if (  myBuff[i] == 0xB0  ) {
      if ( map_ButtonsLeds_Inv[ myBuff[i+1] ] >= 0 ) {
        myBuff[i+1] = map_ButtonsLeds_Inv[ myBuff[i+1] ];
      }
      else if ( configFileName != NULL ) {
        // No mapping in the configuration file
        // Erase bank button midi msg - Put a fake midi msg
        PrepareFakeMidiMsg(&myBuff[i]);
      }

      i += 3; // Next midi msg
    }

    else i++;
  }
}

///////////////////////////////////////////////////////////////////////////////
// MIDI WRITE - APP ON FORCE MAPPING TO ITSELF
///////////////////////////////////////////////////////////////////////////////
static void Force_MapAppWriteToForce(const void *midiBuffer, size_t size) {

  uint8_t * myBuff = (uint8_t*)midiBuffer;

  size_t i = 0 ;
  while  ( i < size ) {

    // AKAI SYSEX
    if (  myBuff[i] == 0xF0 &&  memcmp(&myBuff[i],AkaiSysex,sizeof(AkaiSysex)) == 0 ) {
        // Update the sysex id in the sysex for our original hardware
        i += sizeof(AkaiSysex) ;
        //myBuff[i] = DeviceInfoBloc[MPCOriginalId].sysexId;
        i++;

        // PAD COLOR SYSEX - Store color in the pad color cache
        // SET PAD COLORS SYSEX FN
        // F0 47 7F [3B] -> 65 00 04 [Pad #] [R] [G] [B] F7
        if ( memcmp(&myBuff[i],MPCSysexPadColorFn,sizeof(MPCSysexPadColorFn)) == 0 ) {
              i += sizeof(MPCSysexPadColorFn) ;

              // Update Force pad color cache
              uint8_t padF = myBuff[i];
              PadSysexColorsCache[padF].r = myBuff[i + 1 ];
              PadSysexColorsCache[padF].g = myBuff[i + 2 ];
              PadSysexColorsCache[padF].b = myBuff[i + 3 ];

              i += 5 ; // Next msg
        }

    }

    // Buttons-Leds.  In that direction, it's a LED ON / OFF for the button
    // Check if we must remap...
    else
    if (  myBuff[i] == 0xB0  ) {

      if ( map_ButtonsLeds_Inv[ myBuff[i+1] ] >= 0 ) {
        //tklog_info("MAP INV %d->%d\n",myBuff[i+1],map_ButtonsLeds_Inv[ myBuff[i+1] ]);
        myBuff[i+1] = map_ButtonsLeds_Inv[ myBuff[i+1] ];
      }
      i += 3;
    }

    else i++;
  }

}

///////////////////////////////////////////////////////////////////////////////
// Alsa Rawmidi read
///////////////////////////////////////////////////////////////////////////////
ssize_t snd_rawmidi_read(snd_rawmidi_t *rawmidi, void *buffer, size_t size) {

	ssize_t r = orig_snd_rawmidi_read(rawmidi, buffer, size);
  if ( rawMidiDumpFlag ) RawMidiDump(rawmidi, 'i','r' , buffer, r);


  // Map in all cases if the app writes to the controller
  if ( rawmidi == rawvirt_inpriv  ) {

    // We are running on a Force
    if ( MPCOriginalId == MPC_FORCE ) {
      // We want to map things on Force it self
      if ( MPCId == MPC_FORCE ) {
        r = Force_MapReadFromForce(buffer,size,r);
      }
      // Simulate a MPC on a Force
      else {
        r = Force_MapReadFromMpc(buffer,size,r);
      }
    }
    // We are running on a MPC
    else {
      // We need to remap on a MPC it self
      if ( MPCId != MPC_FORCE ) {
        r = Mpc_MapReadFromMpc(buffer,size,r);
      }
      // Simulate a Force on a MPC
      else {
        r = Mpc_MapReadFromForce(buffer,size,r);
      }
    }
  }

  if ( rawMidiDumpPostFlag ) RawMidiDump(rawmidi, 'o','r' , buffer, r);

	return r;
}

///////////////////////////////////////////////////////////////////////////////
// Alsa Rawmidi write
///////////////////////////////////////////////////////////////////////////////
ssize_t snd_rawmidi_write(snd_rawmidi_t * 	rawmidi,const void * 	buffer,size_t 	size) {

  if ( rawMidiDumpFlag ) RawMidiDump(rawmidi, 'i','w' , buffer, size);

  // Map in all cases if the app writes to the controller
  if ( rawmidi == rawvirt_outpriv || rawmidi == rawvirt_outpub  ) {

    // We are running on a Force
    if ( MPCOriginalId == MPC_FORCE ) {
      // We want to map things on Force it self
      if ( MPCId == MPC_FORCE ) {
        Force_MapAppWriteToForce(buffer,size);
      }
      // Simulate a MPC on a Force
      else {
        Force_MapAppWriteToMpc(buffer,size);
      }
    }
    // We are running on a MPC
    else {
      // We need to remap on a MPC it self
      if ( MPCId != MPC_FORCE ) {
        Mpc_MapAppWriteToMpc(buffer,size);
      }
      // Simulate a Force on a MPC
      else {
        Mpc_MapAppWriteToForce(buffer,size);
      }
    }
  }


  if ( rawMidiDumpPostFlag ) RawMidiDump(rawmidi, 'o','w' , buffer, size);

	return 	orig_snd_rawmidi_write(rawmidi, buffer, size);

}

///////////////////////////////////////////////////////////////////////////////
// Alsa open sequencer
///////////////////////////////////////////////////////////////////////////////
int snd_seq_open (snd_seq_t **handle, const char *name, int streams, int mode) {

  //tklog_info("snd_seq_open %s (%p) \n",name,handle);

  return orig_snd_seq_open(handle, name, streams, mode);

}

///////////////////////////////////////////////////////////////////////////////
// Alsa set a seq port name
///////////////////////////////////////////////////////////////////////////////
void snd_seq_port_info_set_name	(	snd_seq_port_info_t * 	info, const char * 	name )
{
  //tklog_info("snd_seq_port_info_set_name %s (%p) \n",name);


  return snd_seq_port_info_set_name	(	info, name );
}

///////////////////////////////////////////////////////////////////////////////
// Alsa create a simple seq port
///////////////////////////////////////////////////////////////////////////////
int snd_seq_create_simple_port	(	snd_seq_t * 	seq, const char * 	name, unsigned int 	caps, unsigned int 	type )
{
//tklog_info("Port creation of  %s\n",name);

  // Rename virtual port correctly. Impossible with the native Alsa...
  if ( strncmp (" Virtual RawMIDI",name,16) && snd_seq_virtual_port_rename_flag  )
  {
    //tklog_info("Virtual port renamed to %s \n",snd_seq_virtual_port_newname);
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
  // Because this could lead to infinite midi loop in the MPC midi end user settings
	if (  ( seqanyctrl_client >= 0 && strstr(name, anyctrl_name ) )
   //||   ( match(name,"^Client-[0-9][0-9][0-9] TKGL.*" ) )

   // In some specific cases, public and private ports could appear in the APP when spoofing,
   // because port names haven't the same prefixes (eg. Force vs MPC). The consequence is
   // that the MPC App receives midi message of buttons and encoders in midi tracks.
   // So we mask here Private and Public ports eventually requested by MPC App, which
   // should be only internal rawmidi ports.

   // This match will also catch our TKGL virtual ports having Private or Public suffix.
   ||  ( ( match(name,".* Public$|.* Private$" )) )

 )
 {
    tklog_info("Port %s creation canceled.\n",name);
    return -1;
 }

 return orig_snd_seq_create_simple_port(seq,name,caps,type);
}

///////////////////////////////////////////////////////////////////////////////
// Decode a midi seq event
///////////////////////////////////////////////////////////////////////////////
long snd_midi_event_decode	(	snd_midi_event_t * 	dev,unsigned char * 	buf,long 	count, const snd_seq_event_t * 	ev )
{

	// Disable running status to be a true "raw" midi. Side effect : disabled for all ports...
	snd_midi_event_no_status(dev,1);
  long r = orig_snd_midi_event_decode(dev,buf,count,ev);

	return r ;

}


///////////////////////////////////////////////////////////////////////////////
// close
///////////////////////////////////////////////////////////////////////////////
int close(int fd) {

  if ( fd == product_code_file_handler )            product_code_file_handler = -1;
  else if ( fd == product_compatible_file_handler ) product_compatible_file_handler = -1;
  else if ( fd == pws_online_file_handler   )       pws_online_file_handler   = -1 ;
  else if ( fd == pws_voltage_file_handler  )       pws_voltage_file_handler  = -1 ;
  else if ( fd == pws_present_file_handler  )       pws_present_file_handler  = -1 ;
  else if ( fd == pws_status_file_handler   )       pws_status_file_handler   = -1 ;
  else if ( fd == pws_capacity_file_handler )       pws_capacity_file_handler = -1 ;

  return orig_close(fd);
}

///////////////////////////////////////////////////////////////////////////////
// fake_open : use memfd to create a fake file in memory
///////////////////////////////////////////////////////////////////////////////
int fake_open(const char * name, char *content, size_t contentSize) {
  int fd = memfd_create(name, MFD_ALLOW_SEALING);
  write(fd,content, contentSize);
  lseek(fd, 0, SEEK_SET);
  return fd  ;
}

///////////////////////////////////////////////////////////////////////////////
// open64
///////////////////////////////////////////////////////////////////////////////
// Intercept all file opening to fake those we want to.

int open64(const char *pathname, int flags,...) {
   // If O_CREAT is used to create a file, the file access mode must be given.
   // We do not fake the create function at all.
   if (flags & O_CREAT) {
       va_list args;
       va_start(args, flags);
       int mode = va_arg(args, int);
       va_end(args);
      return  orig_open64(pathname, flags, mode);
   }

   // Existing file
   // Fake files sections

   // product code
   if ( product_code_file_handler < 0 && strcmp(pathname,PRODUCT_CODE_PATH) == 0 ) {
     // Create a fake file in memory
     product_code_file_handler = fake_open(pathname,DeviceInfoBloc[MPCId].productCode, strlen(DeviceInfoBloc[MPCId].productCode) ) ;
     return product_code_file_handler ;
   }

   // product compatible
   if ( product_compatible_file_handler < 0 && strcmp(pathname,PRODUCT_COMPATIBLE_PATH) == 0 ) {
     char buf[64];
     sprintf(buf,PRODUCT_COMPATIBLE_STR,DeviceInfoBloc[MPCId].productCompatible);
     product_compatible_file_handler = fake_open(pathname,buf, strlen(buf) ) ;
     return product_compatible_file_handler ;
   }

   // Fake power supply files if necessary only (this allows battery mode)
   else
   if ( DeviceInfoBloc[MPCOriginalId].fakePowerSupply ) {

     if ( pws_voltage_file_handler   < 0 && strcmp(pathname,POWER_SUPPLY_VOLTAGE_NOW_PATH) == 0 ) {
        pws_voltage_file_handler = fake_open(pathname,POWER_SUPPLY_VOLTAGE_NOW,strlen(POWER_SUPPLY_VOLTAGE_NOW) ) ;
        return pws_voltage_file_handler ;
     }

     if ( pws_online_file_handler   < 0 && strcmp(pathname,POWER_SUPPLY_ONLINE_PATH) == 0 ) {
        pws_online_file_handler = fake_open(pathname,POWER_SUPPLY_ONLINE,strlen(POWER_SUPPLY_ONLINE) ) ;
        return pws_online_file_handler ;
     }

     if ( pws_present_file_handler  < 0 && strcmp(pathname,POWER_SUPPLY_PRESENT_PATH) == 0 ) {
       pws_present_file_handler = fake_open(pathname,POWER_SUPPLY_PRESENT,strlen(POWER_SUPPLY_PRESENT)  ) ;
       return pws_present_file_handler ;
     }

     if ( pws_status_file_handler   < 0 && strcmp(pathname,POWER_SUPPLY_STATUS_PATH) == 0 ) {
       pws_status_file_handler = fake_open(pathname,POWER_SUPPLY_STATUS,strlen(POWER_SUPPLY_STATUS) ) ;
       return pws_status_file_handler ;
     }

     if ( pws_capacity_file_handler < 0 && strcmp(pathname,POWER_SUPPLY_CAPACITY_PATH) == 0 ) {
       pws_capacity_file_handler = fake_open(pathname,POWER_SUPPLY_CAPACITY,strlen(POWER_SUPPLY_CAPACITY) ) ;
       return pws_capacity_file_handler ;
     }
   }

   return orig_open64(pathname, flags) ;
}

// ///////////////////////////////////////////////////////////////////////////////
// // Process an input midi seq event
// ///////////////////////////////////////////////////////////////////////////////
// int snd_seq_event_input( snd_seq_t* handle, snd_seq_event_t** ev )
// {
//
//   int r = orig_snd_seq_event_input(handle,ev);
//   // if ((*ev)->type != SND_SEQ_EVENT_CLOCK ) {
//   //      dump_event(*ev);
//   //
//   //
//   //    // tklog_info("[tkgl] Src = %02d:%02d -> Dest = %02d:%02d \n",(*ev)->source.client,(*ev)->source.port,(*ev)->dest.client,(*ev)->dest.port);
//   //    // ShowBufferHexDump(buf, r,16);
//   //    // tklog_info("[tkgl] ----------------------------------\n");
//   //  }
//
//
//   return r;
//
// }

// ///////////////////////////////////////////////////////////////////////////////
// // open
// ///////////////////////////////////////////////////////////////////////////////
// int open(const char *pathname, int flags,...) {
//
// //  printf("(tkgl) Open %s\n",pathname);
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

//
// ///////////////////////////////////////////////////////////////////////////////
// // read
// ///////////////////////////////////////////////////////////////////////////////
// ssize_t read(int fildes, void *buf, size_t nbyte) {
//
//   return orig_read(fildes,buf,nbyte);
// }
