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


// MPC Controller names (regexp)
#define CTRL_FORCE "Akai Pro Force"
#define CTRL_MPC_X "MPC X Controller"
#define CTRL_MPC_LIVE "MPC Live Controller"
#define CTRL_MPC_LIVE_2 "MPC Live II"
#define CTRL_MPC_ALL "(.*MPC.*Controller.*|.*MPC Live II$|.*Akai Pro Force.*)"

// Product code file and diffrent file path to fake
#define PRODUCT_CODE_PATH "/sys/firmware/devicetree/base/inmusic,product-code"
#define PRODUCT_COMPATIBLE_PATH "/sys/firmware/devicetree/base/compatible"
#define PRODUCT_COMPATIBLE_STR "inmusic,%sinmusic,az01rockchip,rk3288"

// Power supply faking
#define POWER_SUPPLY_ONLINE_PATH "/sys/class/power_supply/az01-ac-power/online"
#define POWER_SUPPLY_VOLTAGE_NOW_PATH "/sys/class/power_supply/az01-ac-power/voltage_now"
#define POWER_SUPPLY_PRESENT_PATH "/sys/class/power_supply/sbs-3-000b/present"
#define POWER_SUPPLY_STATUS_PATH "/sys/class/power_supply/sbs-3-000b/status"
#define POWER_SUPPLY_CAPACITY_PATH "/sys/class/power_supply/sbs-3-000b/capacity"

#define POWER_SUPPLY_ONLINE "1"
#define POWER_SUPPLY_VOLTAGE_NOW "18608000"
#define POWER_SUPPLY_PRESENT "1"
#define POWER_SUPPLY_STATUS "Full"
#define POWER_SUPPLY_CAPACITY "100"

// Colors R G B (nb . max r g b value is 7f. The bit 8 is always set )
#define COLOR_FIRE       0xFF0000
#define COLOR_ORANGE     0xFF6D17
#define COLOR_TANGERINE  0xFF4400
#define COLOR_APRICOT    0xFF8800
#define COLOR_YELLOW     0xECEC24
#define COLOR_CANARY     0xFFD500
#define COLOR_LEMON      0xE6FF00
#define COLOR_CHARTREUSE 0xA2FF00
#define COLOR_NEON       0x55FF00
#define COLOR_LIME       0x11FF00
#define COLOR_CLOVER     0x00FF33
#define COLOR_SEA        0x00FF80
#define COLOR_MINT       0x00FFC4
#define COLOR_CYAN       0x00F7FF
#define COLOR_SKY        0x00AAFF
#define COLOR_AZURE      0x0066FF
#define COLOR_GREY       0xA2A9AD
#define COLOR_MIDNIGHT   0x0022FF
#define COLOR_INDIGO     0x5200FF
#define COLOR_VIOLET     0x6F00FF
#define COLOR_GRAPPE     0xB200FF
#define COLOR_FUSHIA     0xFF00FF
#define COLOR_MAGENTA    0xFF00BB
#define COLOR_CORAL      0xFF0077

// Mute pads mod button value
#define FORCE_ASSIGN_A   123
#define FORCE_ASSIGN_B   124
#define FORCE_MUTE       91
#define FORCE_SOLO       92
#define FORCE_REC_ARM    93
#define FORCE_CLIP_STOP  94

#define FORCE_LEFT  0x72
#define FORCE_RIGHT 0X73
#define FORCE_UP    0X70
#define FORCE_DOWN  0x71

// MPC Bank buttons
#define BANK_A 0x23
#define BANK_B 0x24
#define BANK_C 0x25
#define BANK_D 0X26


// The shift value is hard coded here because it is equivalent whatever the platform is
#define SHIFT_KEY_VALUE 49

// Mapping tables index offset
#define MPCPADS_TABLE_IDX_OFFSET 0X24
#define FORCEPADS_TABLE_IDX_OFFSET 0X36

#define MAPPING_TABLE_SIZE 256

// INI FILE Constant
#define INI_FILE_KEY_MAX_LEN 64
#define INI_FILE_LINE_MAX_LEN 256


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

// Pads Color cache captured from sysex events
typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} ForceMPCPadColor_t;
static ForceMPCPadColor_t PadSysexColorsCache[256];

// End user virtual port name
static char *user_virtual_portname = NULL;

// End user virtual port handles
static snd_rawmidi_t *rawvirt_user_in    = NULL ;
static snd_rawmidi_t *rawvirt_user_out   = NULL ;


// Device info block typedef
typedef struct {
  char    * productCode;
  char    * productCompatible;
  bool      fakePowerSupply;
  uint8_t   sysexId;
  char    * productString;
  char    * productStringShort;
  uint8_t   qlinkKnobsCount;
  uint8_t   sysexIdReply[7];
} DeviceInfo_t;

// Internal MPC product sysex id  ( a struct may be better....)
enum MPCIds  {  MPC_X,   MPC_LIVE,   MPC_FORCE, MPC_ONE,   MPC_LIVE_MK2, _END_MPCID };
// Declare in the same order that enums above
const static DeviceInfo_t DeviceInfoBloc[] = {
  { .productCode = "ACV5", .productCompatible = "acv5", .fakePowerSupply = false, .sysexId = 0x3a, .productString = "MPC X",      .productStringShort = "X",     .qlinkKnobsCount = 16, .sysexIdReply = {0x3A,0x00,0x19,0x00,0x01,0x01,0x01} },
  { .productCode = "ACV8", .productCompatible = "acv8", .fakePowerSupply = true,  .sysexId = 0x3b, .productString = "MPC Live",   .productStringShort = "LIVE",  .qlinkKnobsCount = 4,  .sysexIdReply = {0x3B,0x00,0x19,0x00,0x01,0x01,0x01} },
  { .productCode = "ADA2", .productCompatible = "ada2", .fakePowerSupply = false, .sysexId = 0x40, .productString = "Force",      .productStringShort = "FORCE", .qlinkKnobsCount = 8,  .sysexIdReply = {0x40,0x00,0x19,0x00,0x00,0x04,0x03} },
  { .productCode = "ACVA", .productCompatible = "acva", .fakePowerSupply = false, .sysexId = 0x46, .productString = "MPC One",    .productStringShort = "ONE",   .qlinkKnobsCount = 4,  .sysexIdReply = {0x46,0x00,0x19,0x00,0x01,0x01,0x01} },
  { .productCode = "ACVB", .productCompatible = "acvb", .fakePowerSupply = true,  .sysexId = 0x47, .productString = "MPC Live 2", .productStringShort = "LIVE2", .qlinkKnobsCount = 4,  .sysexIdReply = {0x47,0x00,0x19,0x00,0x01,0x01,0x01} },
};

// Columns pads
//F0 47 7F 40 65 00 04 40 00 00 06 F7 F0 47 7F 40  | .G.@e..@.....G.@
//[tkgl]  65 00 04 41 00 00 06 F7
// F0 47 7F [3B] 65 00 04 [Pad #] [R] [G] [B] F7


// Sysex patterns.
static const uint8_t AkaiSysex[]                  = {0xF0,0x47, 0x7F};
const uint8_t MPCSysexPadColorFn[]                = {0x65,0x00,0x04};
static const uint8_t IdentityReplySysexHeader[]   = {0xF0,0x7E,0x00,0x06,0x02,0x47};

// MPC Current pad bank.  A-H = 0-7
static uint8_t MPC_PadBank = BANK_A ;

// SHIFT Holded mode
// Holding shift will activate the shift mode
static bool shiftHoldedMode=false;

// Qlink knobs shift mode
static bool QlinkKnobsShiftMode=false;

// Columns modes in Force simulated on a MPC
static int ForceColumnMode = -1 ;

// MPC hardware pads : a totally anarchic numbering!
// Force is orered from 0x36. Top left
enum MPCPads { MPC_PAD1, MPC_PAD2, MPC_PAD3, MPC_PAD4, MPC_PAD5,MPC_PAD6,MPC_PAD7, MPC_PAD8,
              MPC_PAD9, MPC_PAD10, MPC_PAD11, MPC_PAD12, MPC_PAD13, MPC_PAD14, MPC_PAD15, MPC_PAD16 };

// MPC ---------------------     FORCE------------------
// Press = 99 [pad#] [00-7F]     idem
// Release = 89 [pad#] 00        idem
// AFT A9 [pad#] [00-7F]         idem
// MPC PAd # from left bottom    Force pad# from up left to right bottom
// to up right (hexa) =
// 31 37 33 35                   36 37 38 39 3A 3B 3C 3D
// 30 2F 2D 2B                   3E 3F 40 41 42 43 44 45
// 28 26 2E 2C                   ...
// 25 24 2A 52
// wtf !!
//
// (13)31 (14)37 (15)33 (16)35
// (9) 30 (10)2F (11)2D (12)2B
// (5) 28 (6) 26 (7) 2E (8) 2C
// (1) 25 (2) 24 (3) 2A (4) 52

static const uint8_t MPCPadsTable[]
= { MPC_PAD2, MPC_PAD1, MPC_PAD6,  0xff,   MPC_PAD5,    0xff,  MPC_PAD3, MPC_PAD12,
//    0x24,       0x25,     0x26,    (0x27),   0x28,   (0x29),    0x2A,     0x2B,
    MPC_PAD8, MPC_PAD11, MPC_PAD7, MPC_PAD10, MPC_PAD9, MPC_PAD13, 0xff,  MPC_PAD15,
//    0x2C,       0x2D,     0x2E,     0x2F,     0x30,      0x31,   (0x32)   0x33,
  0xff,   MPC_PAD16, 0xff, MPC_PAD14,
// (0x34), 0x35,    (0x36)   0x37,
0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
0xff,0xff,0xff,0xff,0xff,0xff,
// 0x38 - 0x51
MPC_PAD4 };
// 0x52

static const uint8_t MPCPadsTable2[]
= {
  0x25,0x24,0x2A,0x52,
  0x28,0x26,0x2E,0x2C,
  0x30,0x2F,0x2D,0x2B,
  0x31,0x37,0x33,0x35
};

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
static typeof(&snd_seq_event_input) orig_snd_seq_event_input;

// Globals used to rename a virtual port and get the client id.  No other way...
static int  snd_seq_virtual_port_rename_flag  = 0;
static char snd_seq_virtual_port_newname [30];
static int  snd_seq_virtual_port_clientid=-1;

// Other more generic APIs
static typeof(&open64) orig_open64;
static typeof(&read) orig_read;
static typeof(&open) orig_open;
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
          fprintf(stdout,"[tkgl]  *** Error in Configuration file : '=' missing : %s. \n",line);
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
          fprintf(stdout,"[tkgl]  Maximum of %d keys by section reached for key %s. The key is ignored. \n",keyMaxCount,p);
        }
      }
      else {
        // Check the key value

        if ( strcmp( p,key ) == 0 ) {
          strcpy(value,v);
          keyIndex = 1;
          //fprintf(stdout,"[tkgl]  Loaded : %s.\n",line);

          break;
        }
      }
    }
  }
  fclose(fp);
  //fprintf(stdout,"[tkgl]  config file closed.\n");


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
  fprintf(stdout,"[tkgl]  %s configuration file found.\n", confFileName);

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

  fprintf(stdout,"[tkgl]  %d keys found in  %s . \n",keysCount,btLedMapSectionName);

  if ( keysCount <= 0 ) {
    fprintf(stdout,"[tkgl]  Error *** Missing section %s in configuration file %s or syntax error. No mapping set. \n",btLedMapSectionName,confFileName);
    return;
  }

  // Get Globals within the mapping section name of the current device
  if ( GetKeyValueFromConfFile(confFileName, btLedMapSectionName,"_p_QlinkKnobsShiftMode",myValue,NULL,0) ) {

    QlinkKnobsShiftMode = ( atoi(myValue) == 1 ? true:false );
    if ( QlinkKnobsShiftMode) fprintf(stdout,"[tkgl]  QlinkKnobsShiftMode was set to 1.\n");

  } else {
    QlinkKnobsShiftMode = false; // default
    fprintf(stdout,"[tkgl]  Warning : missing _p_QlinkKnobsShiftMod key in section . Was set to 0 by default.\n");
  }

  // Read the Buttons & Leds mapping section entries
  for ( int i = 0 ; i < keysCount ; i++ ) {

    // Buttons & Leds mapping

    // Ignore parameters
    if ( strncmp(keyNames[i],"_p_",3) == 0 ) continue;

    strcpy(srcKeyName, keyNames[i] );

    if (  GetKeyValueFromConfFile(confFileName, btLedMapSectionName,srcKeyName,myValue,NULL,0) != 1 ) {
      fprintf(stdout,"[tkgl]  Error *** Value not found for %s key in section[%s].\n",srcKeyName,btLedMapSectionName);
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
      fprintf(stdout,"[tkgl]  Error *** Value not found for %s key in section[%s].\n",srcKeyName,btLedSrcSectionName);
      continue;
    }

    // Save the button value
    int srcButtonValue =  strtol(myValue, NULL, 0);

    // Read value in target device section
    if (  GetKeyValueFromConfFile(confFileName, btLedDestSectionName,destKeyName,myValue,NULL,0) != 1 ) {
      fprintf(stdout,"[tkgl]  Error *** Value not found for %s key in section[%s].\n",destKeyName,btLedDestSectionName);
      continue;
    }

    int destButtonValue = strtol(myValue, NULL, 0);

    if ( srcButtonValue <=127 && destButtonValue <=127 ) {

      // If shift mapping, set the bit 7
      srcButtonValue   = ( srcShift  ? srcButtonValue  + 0x80 : srcButtonValue );
      destButtonValue  = ( destShift ? destButtonValue + 0x80 : destButtonValue );

      map_ButtonsLeds[srcButtonValue]      = destButtonValue;
      map_ButtonsLeds_Inv[destButtonValue] = srcButtonValue;

      fprintf(stdout,"[tkgl]  Button-Led %s%s (%d) mapped to %s%s (%d)\n",srcShift?"(SHIFT) ":"",srcKeyName,srcButtonValue,destShift?"(SHIFT) ":"",destKeyName,map_ButtonsLeds[srcButtonValue]);

    }
    else {
      fprintf(stdout,"[tkgl]  Configuration file Error : values above 127 found. Check sections [%s] %s, [%s] %s.\n",btLedSrcSectionName,srcKeyName,btLedDestSectionName,destKeyName);
      return;
    }

  } // for
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

  fprintf(stdout,"[tkgl]\n") ;
  fprintf(stdout,"[tkgl]  --tgkl_help               : Show this help\n") ;
  fprintf(stdout,"[tkgl]  --tkgl_ctrlname=<name>    : Use external controller containing <name>\n") ;
  fprintf(stdout,"[tkgl]  --tkgl_iamX               : Emulate MPC X\n") ;
  fprintf(stdout,"[tkgl]  --tkgl_iamLive            : Emulate MPC Live\n") ;
  fprintf(stdout,"[tkgl]  --tkgl_iamForce           : Emulate Force\n") ;
  fprintf(stdout,"[tkgl]  --tkgl_iamOne             : Emulate MPC One\n") ;
  fprintf(stdout,"[tkgl]  --tkgl_iamLive2           : Emulate MPC Live Mk II\n") ;
  fprintf(stdout,"[tkgl]  --tkgl_virtualport=<name> : Create end user virtual port <name>\n") ;
  fprintf(stdout,"[tkgl]  --tkgl_mididump           : Dump original raw midi flow\n") ;
  fprintf(stdout,"[tkgl]  --tkgl_mididumpPost       : Dump raw midi flow after transformation\n") ;
  fprintf(stdout,"[tkgl]  --tkgl_configfile=<name>  : Use configuration file <name>\n") ;
  fprintf(stdout,"[tkgl]\n") ;
  exit(0);
}


///////////////////////////////////////////////////////////////////////////////
// Setup tkgl anyctrl
///////////////////////////////////////////////////////////////////////////////
static void tkgl_init()
{

  // System call hooks
  orig_open64 = dlsym(RTLD_NEXT, "open64");
  orig_open   = dlsym(RTLD_NEXT, "open");
  orig_close  = dlsym(RTLD_NEXT, "close");
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

  // Read product code
  char product_code[4];
  int fd = orig_open(PRODUCT_CODE_PATH,O_RDONLY);
  orig_read(fd,&product_code,4);

  // Find the id in the product code table
  for (int i = 0 ; i < _END_MPCID; i++) {
    if ( strncmp(DeviceInfoBloc[i].productCode,product_code,4) == 0 ) {
      MPCOriginalId = i;
      break;
    }
  }
  if ( MPCOriginalId < 0) {
    fprintf(stdout,"[tkgl]  *** Error when reading the product-code file\n");
    exit(1);
  }
  fprintf(stdout,"[tkgl]  Original Product code : %s (%s)\n",DeviceInfoBloc[MPCOriginalId].productCode,DeviceInfoBloc[MPCOriginalId].productString);

  if ( MPCSpoofedID >= 0 ) {
    fprintf(stdout,"[tkgl]  Product code spoofed to %s (%s)\n",DeviceInfoBloc[MPCSpoofedID].productCode,DeviceInfoBloc[MPCSpoofedID].productString);
    MPCId = MPCSpoofedID ;
  } else MPCId = MPCOriginalId ;

  // Fake the power supply ?
  if ( DeviceInfoBloc[MPCOriginalId].fakePowerSupply ) fprintf(stdout,"[tkgl]  The power supply will be faked to allow battery mode.\n");

  // read mapping config file if any
  LoadMappingFromConfFile(configFileName);

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

      // help
      if ( ( strcmp("--tkgl_help",argv[i]) == 0 ) ) {
         ShowHelp();
      }
      else
      if ( ( strncmp("--tkgl_ctrlname=",argv[i],16) == 0 ) && ( strlen(argv[i]) >16 ) ) {
         anyctrl_name = argv[i] + 16;
         fprintf(stdout,"[tkgl]  --tgkl_ctrlname specified for %s midi controller\n",anyctrl_name) ;
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
      else
      // Config file name
      if ( ( strncmp("--tkgl_configfile=",argv[i],18) == 0 ) && ( strlen(argv[i]) >18 )  ) {
        configFileName = argv[i] + 18 ;
        fprintf(stdout,"[tkgl]  --tkgl_configfile specified. File %s will be used for mapping\n",configFileName) ;
      }


    }

    if ( MPCSpoofedID >= 0 ) {
      fprintf(stdout,"[tkgl]  %s specified. %s spoofing.\n",tkgl_SpoofArg,DeviceInfoBloc[MPCSpoofedID].productString ) ;
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
// Refresh MPC pads colors from Force PAD Colors cache
///////////////////////////////////////////////////////////////////////////////
static void MPC_UpdatePadColor(uint8_t padL, uint8_t padC, uint8_t nbLine) {

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
static void MPC_ShowMatrixQuadran(uint8_t forcePadL, uint8_t forcePadC) {

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
      //fprintf(stdout,"[tkgl] MPC Pad quadran : l,c %d,%d Pad %d r g b %02X %02X %02X\n",forcePadL,forcePadC,sysexBuff[7],sysexBuff[8],sysexBuff[9],sysexBuff[10]);

      orig_snd_rawmidi_write(rawvirt_outpriv,sysexBuff,sizeof(sysexBuff));
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Draw a pad line on MPC pads from a Force PAD line in the current Colors cache
///////////////////////////////////////////////////////////////////////////////
void MPC_UpdatePadColorLine(uint8_t forcePadL, uint8_t forcePadC, uint8_t mpcPadL) {

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

    //fprintf(stdout,"[tkgl] MPC Pad Line refresh : %d r g b %02X %02X %02X\n",sysexBuff[7],sysexBuff[8],sysexBuff[9],sysexBuff[10]);

    orig_snd_rawmidi_write(rawvirt_outpriv,sysexBuff,sizeof(sysexBuff));
  }
}

///////////////////////////////////////////////////////////////////////////////
// MIDI READ - APP ON MPC READING AS FORCE
///////////////////////////////////////////////////////////////////////////////
static size_t Map_AppOnMpcReadAsForce(void *midiBuffer, size_t maxSize,size_t size) {

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
        //fprintf(stdout,"[tkgl]  %d button pressed/released (%02x) \n",myBuff[i+1], myBuff[i+2]);

        // Shift is an exception and  mapping is ignored (the SHIFT button can't be mapped)
        // Double click on SHIFT is not managed at all. Avoid it.
        if ( myBuff[i+1] == SHIFT_KEY_VALUE ) {
            shiftHoldedMode = ( myBuff[i+2] == 0x7F ? true:false ) ;
  //          fprintf(stdout,"[tkgl]  %d SHIFT MODE\n", shiftHoldedMode   );
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

  //             fprintf(stdout,"[tkgl]  Shift Button %d\n",mapValue);
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
                  if ( size > maxSize - 3 ) fprintf(stdout,"[tkgl]  Warning : midi buffer overflow when inserting SHIFT note off !!\n");
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
          //fprintf(stdout,"[tkgl]  MAP %d->%d\n",myBuff[i+1],map_ButtonsLeds[ myBuff[i+1] ]);
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

//              fprintf(stdout,"[tkgl] COLUMN MODE ON %d\n",ForceColumnMode);
              if ( ForceColumnMode >= 0 ) {
                MPC_UpdatePadColorLine(8, MPCPad_OffsetC, 3);
                MPC_ShowMatrixQuadran(MPCPad_OffsetL, MPCPad_OffsetC);
              }

        }
        // Button released
        else {

    //         fprintf(stdout,"[tkgl]  COLUMN MODE OFF %d\n",ForceColumnMode);
             ForceColumnMode = -1 ;
             MPC_UpdatePadColor(MPCPad_OffsetL,MPCPad_OffsetC,4);
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
            //fprintf(stdout,"[tkgl]  remapped to %02x %02x %02x  ! \n",myBuff[i],myBuff[i+1],myBuff[i+2]);
            myBuff[i+1] = buttonValue;

          }
          else {
            // Generate a fake midi message
            myBuff[i]   = 0x8F;
            myBuff[i+1] = 0x00;
            myBuff[i+2] = 0x00 ;
          }

          // Update the MPC pad colors from Force pad colors cache
          if ( refreshPads )  MPC_UpdatePadColor(MPCPad_OffsetL,MPCPad_OffsetC,4);

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
static size_t Map_AppOnMpcReadAsMpc(void *midiBuffer, size_t maxSize,size_t size) {

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
                  if ( size > maxSize - 3 ) fprintf(stdout,"[tkgl]  Warning : midi buffer overflow when inserting SHIFT note off !!\n");
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
          //fprintf(stdout,"[tkgl]  MAP %d->%d\n",myBuff[i+1],map_ButtonsLeds[ myBuff[i+1] ]);
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
// MIDI READ - APP ON FORCE READING AS ITSELF
///////////////////////////////////////////////////////////////////////////////
static size_t Map_AppOnForceReadAsForce(void *midiBuffer, size_t maxSize,size_t size) {

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
                  if ( size > maxSize - 3 ) fprintf(stdout,"[tkgl]  Warning : midi buffer overflow when inserting SHIFT note off !!\n");
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
          //fprintf(stdout,"[tkgl]  MAP %d->%d\n",myBuff[i+1],map_ButtonsLeds[ myBuff[i+1] ]);
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
static size_t Map_AppOnForceReadAsMpc(void *midiBuffer, size_t maxSize,size_t size) {

    uint8_t * myBuff = (uint8_t*)midiBuffer;

    size_t i = 0 ;
    while  ( i < size ) {

      // AKAI SYSEX ------------------------------------------------------------
      // IDENTITY REQUEST REPLAY
      if (  myBuff[i] == 0xF0 &&  memcmp(&myBuff[i],IdentityReplySysexHeader,sizeof(IdentityReplySysexHeader)) == 0 ) {
        // If so, substitue sysex identity request by the faked one
        memcpy(&myBuff[i+sizeof(IdentityReplySysexHeader)],DeviceInfoBloc[MPCId].sysexIdReply, sizeof(DeviceInfoBloc[MPCId].sysexIdReply) );
        i += sizeof(IdentityReplySysexHeader) + sizeof(DeviceInfoBloc[MPCId].sysexIdReply) ;
      }
      // KNOBS TURN ------------------------------------------------------------
      // If it's a shift + knob turn, add an offset   B0 [10-31] [7F - n]
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
        // Shift is an exception and  mapping is ignored (the SHIFT button can't be mapped)
        // Double click on SHIFT is not managed at all. Avoid it.
        if ( myBuff[i+1] == SHIFT_KEY_VALUE ) {
            shiftHoldedMode = ( myBuff[i+2] == 0x7F ? true:false ) ;
//            fprintf(stdout,"[tkgl]  %d SHIFT MODE\n", shiftHoldedMode   );
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

  //             fprintf(stdout,"[tkgl]  Shift Button %d\n",mapValue);
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
                  if ( size > maxSize - 3 ) fprintf(stdout,"[tkgl]  Warning : midi buffer overflow when inserting SHIFT note off !!\n");
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
          //fprintf(stdout,"[tkgl]  MAP %d->%d\n",myBuff[i+1],map_ButtonsLeds[ myBuff[i+1] ]);
          myBuff[i+1] = map_ButtonsLeds[ myBuff[i+1] ];
        }

        // Intercept BANK Keys because we want to make them special keys on Force
        // All the Force pads are used to simulate 4 banks
        // The BANK_A buttons = Banks group "A" :  A B C D
        // The BANK_B buttons = Banks group "B" :  E F G H
        if ( myBuff[i+1] >= BANK_A && myBuff[i+1] <= BANK_A + 7 ) {

          if ( myBuff[i+1] == BANK_A ) MpcBankGroup = 0 ;
          else if ( myBuff[i+1] == BANK_B ) MpcBankGroup = 1 ;
          // Erase bank button midi msg - Put a fake midi msg
          myBuff[i]   = 0x8F ;
          myBuff[i+1] = 0x00 ;
          myBuff[i+2] = 0x00 ;

          // Light the right button led , according to the mapping
          uint8_t ledOnOff[] = { 0xB0, 0x00, 0x00, 0xB0, 0x00, 0x00 };

          ledOnOff[1] = ( map_ButtonsLeds_Inv[ BANK_A ] >= 0 ? map_ButtonsLeds_Inv[ BANK_A ] : BANK_A ) ;
          ledOnOff[2] = MpcBankGroup == 1 ? 0 : 3 ;

          ledOnOff[4] = ( map_ButtonsLeds_Inv[ BANK_B ] >= 0 ? map_ButtonsLeds_Inv[ BANK_B ] : BANK_B ) ;
          ledOnOff[5] = MpcBankGroup == 1 ? 3 : 0 ;

          orig_snd_rawmidi_write(rawvirt_outpriv,ledOnOff,sizeof(ledOnOff));

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

          // Use the 8x8 matrix as 4x pad banks
          // Regarding the pad pressed
          //  C  D   2  3
          //  A  B   0  1

          int prevMPC_PadBank = MPC_PadBank;
          uint8_t pbk = ( padL >= 4  ? ( padC < 4  ? BANK_A : BANK_B ) : ( padC < 4  ? BANK_C : BANK_D ) ) ;
          MPC_PadBank = pbk + MpcBankGroup * 4 ;

          // Bank changed since last press ?
          // If yes, simulate a press BANK_n before the pad

          if ( prevMPC_PadBank != MPC_PadBank ) {

            uint8_t msgSize = 6;
            if ( MpcBankGroup == 1 ) msgSize += 6;

            if ( size > maxSize - msgSize ) fprintf(stdout,"[tkgl]  Warning : midi buffer overflow when inserting BANK_n on/off !!\n");
            memcpy( &myBuff[i + msgSize ], &myBuff[i], size - i );
            size += msgSize ;

            // Change the bank group eventually (BANK_A or BANK_B pressed)
            // Insert BANK n  button
            if ( MpcBankGroup == 1 ) {
              // Insert SHIFT PRESS when bank is above D
              myBuff[ i++ ] = 0x90 ; // Button
              myBuff[ i++ ] = SHIFT_KEY_VALUE ;
              myBuff[ i++ ] = 0x7F ; // Button pressed
            }

            myBuff[ i++ ] = 0x90 ; // Button
            myBuff[ i++ ] = pbk  ;
            myBuff[ i++ ] = 0x7F ; // Button pressed
            myBuff[ i++ ] = 0x90 ; // Button
            myBuff[ i++ ] = pbk ;
            myBuff[ i++ ] = 0x00 ; // Button released

            if ( MpcBankGroup == 1 ) {
              // Insert SHIFT RELEASE
              myBuff[ i++ ] = 0x90 ; // Button
              myBuff[ i++ ] = SHIFT_KEY_VALUE ;
              myBuff[ i++ ] = 0x00 ; // Button released
            }

          }

         // Compute the MPC pad id
          uint8_t p = ( 3 - padL % 4 ) * 4 + padC % 4;
          myBuff[i+1] = MPCPadsTable2[p];

//fprintf(stdout,"[tkgl]  MPC pad transposed : %d\n",p);

        i += 3;

      }

      else i++;

    }

    return size;
}

///////////////////////////////////////////////////////////////////////////////
// MIDI WRITE - APP ON MPC MAPPING TO MPC
///////////////////////////////////////////////////////////////////////////////
static void Map_AppOnMpcWriteToMpc(const void *midiBuffer, size_t size) {

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
        //fprintf(stdout,"[tkgl]  MAP INV %d->%d\n",myBuff[i+1],map_ButtonsLeds_Inv[ myBuff[i+1] ]);
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
static void Map_AppOnMpcWriteToForce(const void *midiBuffer, size_t size) {

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
                //fprintf(stdout,"[tkgl]  Pad (%d,%d) In the quadran (%d,%d)\n",padL,padC,MPCPad_OffsetL,MPCPad_OffsetC);
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
        //fprintf(stdout,"[tkgl]  MAP INV %d->%d\n",myBuff[i+1],map_ButtonsLeds_Inv[ myBuff[i+1] ]);
        myBuff[i+1] = map_ButtonsLeds_Inv[ myBuff[i+1] ];
      }
      i += 3;
    }

    else i++;
  }

  // Check if we must refresh the pad mutes line on the MPC
  if ( ForceColumnMode >= 0 ) {
    MPC_UpdatePadColorLine(8, MPCPad_OffsetC, 3);
    MPC_ShowMatrixQuadran(MPCPad_OffsetL, MPCPad_OffsetC);
  }
}

///////////////////////////////////////////////////////////////////////////////
// MIDI WRITE - APP ON FORCE MAPPING TO MPC
///////////////////////////////////////////////////////////////////////////////
static void Map_AppOnForceWriteToMpc(const void *midiBuffer, size_t size) {

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

          // Make an offset according to the current root bank
          uint8_t pbk =  MPC_PadBank - MpcBankGroup * 4 ;

          if ( pbk == BANK_C || pbk == BANK_D ) padL += 4;
          if ( pbk == BANK_D || pbk == BANK_B ) padC += 4;

          // Compute the pad # for Force in a 4x4 matrix at the current offset

          myBuff[i] = ( 7 - padL ) * 8 + padC;

          // Update Force pad color cache
          PadSysexColorsCache[myBuff[i]].r = myBuff[i + 1 ];
          PadSysexColorsCache[myBuff[i]].g = myBuff[i + 2 ];
          PadSysexColorsCache[myBuff[i]].b = myBuff[i + 3 ];

          i += 5 ; // Next msg
        }
    }

    // Buttons-Leds.  In that direction, it's a LED ON / OFF for the button
    // Check if we must remap...
    else
    if (  myBuff[i] == 0xB0  ) {

      if ( map_ButtonsLeds_Inv[ myBuff[i+1] ] >= 0 ) {
        //fprintf(stdout,"[tkgl]  MAP INV %d->%d\n",myBuff[i+1],map_ButtonsLeds_Inv[ myBuff[i+1] ]);

        // BANK A-D are removed here because leds are managed in the read section
        if ( myBuff[i+1] >= BANK_A && myBuff[i+1] <= BANK_A + 7 ) {
          // Put a fake midi msg (remember : this a private midi
          // so no risk to send that to a synth !)
          myBuff[i]   = 0x8F ;
          myBuff[i+1] = 0x00 ;
          myBuff[i+2] = 0x00 ;
        }
        else myBuff[i+1] = map_ButtonsLeds_Inv[ myBuff[i+1] ];
      }
      i += 3;
    }

    else i++;
  }
}

///////////////////////////////////////////////////////////////////////////////
// MIDI WRITE - APP ON FORCE MAPPING TO ITSELF
///////////////////////////////////////////////////////////////////////////////
static void Map_AppOnForceWriteToForce(const void *midiBuffer, size_t size) {

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
        //fprintf(stdout,"[tkgl]  MAP INV %d->%d\n",myBuff[i+1],map_ButtonsLeds_Inv[ myBuff[i+1] ]);
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
  if ( rawMidiDumpFlag ) {
    const char *name = snd_rawmidi_name(rawmidi);
    fprintf(stdout,"[tkgl]  ENTRY Dump snd_rawmidi_read from controller %s\n",name);
    ShowBufferHexDump(buffer, r,16);
    fprintf(stdout,"[tkgl]\n");
  }

  // Map in all cases if the app writes to the controller
  if ( rawmidi == rawvirt_inpriv  ) {

    // We are running on a Force
    if ( MPCOriginalId == MPC_FORCE ) {
      // We want to map things on Force it self
      if ( MPCId == MPC_FORCE ) {
        r = Map_AppOnForceReadAsForce(buffer,size,r);
      }
      // Simulate a MPC on a Force
      else {
        r = Map_AppOnForceReadAsMpc(buffer,size,r);
      }
    }
    // We are running on a MPC
    else {
      // We need to remap on a MPC it self
      if ( MPCId != MPC_FORCE ) {
        r = Map_AppOnMpcReadAsMpc(buffer,size,r);
      }
      // Simulate a Force on a MPC
      else {
        r = Map_AppOnMpcReadAsForce(buffer,size,r);
      }
    }
  }

  if ( rawMidiDumpPostFlag ) {
    const char *name = snd_rawmidi_name(rawmidi);
    fprintf(stdout,"[tkgl]  POST Dump snd_rawmidi_read from controller %s\n",name);
    ShowBufferHexDump(buffer, r,16);
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

  // Map in all cases if the app writes to the controller
  if ( rawmidi == rawvirt_outpriv || rawmidi == rawvirt_outpub  ) {

    // We are running on a Force
    if ( MPCOriginalId == MPC_FORCE ) {
      // We want to map things on Force it self
      if ( MPCId == MPC_FORCE ) {
        Map_AppOnForceWriteToForce(buffer,size);
      }
      // Simulate a MPC on a Force
      else {
        Map_AppOnForceWriteToMpc(buffer,size);
      }
    }
    // We are running on a MPC
    else {
      // We need to remap on a MPC it self
      if ( MPCId != MPC_FORCE ) {
        Map_AppOnMpcWriteToMpc(buffer,size);
      }
      // Simulate a Force on a MPC
      else {
        Map_AppOnMpcWriteToForce(buffer,size);
      }
    }
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
    fprintf(stdout,"[tkgl]  Port %s creation canceled.\n",name);
    return -1;
 }

 return orig_snd_seq_create_simple_port(seq,name,caps,type);
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

  return orig_read(fildes,buf,nbyte);
}
