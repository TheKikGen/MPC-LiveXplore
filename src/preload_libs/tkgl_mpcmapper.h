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


#define VERSION "BETA2"

#define TKGL_LOGO "\
__ __| |           |  /_) |     ___|             |           |\n\
  |   __ \\   _ \\  ' /  | |  / |      _ \\ __ \\   |      _` | __ \\   __|\n\
  |   | | |  __/  . \\  |   <  |   |  __/ |   |  |     (   | |   |\\__ \\\n\
 _|  _| |_|\\___| _|\\_\\_|_|\\_\\\\____|\\___|_|  _| _____|\\__,_|_.__/ ____/\n\
"

// MPC Controller names (regexp) - Use aconnect -l
#define CTRL_FORCE "Akai Pro Force"
#define CTRL_MPC_X "MPC X Controller"
#define CTRL_MPC_LIVE "MPC Live Controller"
#define CTRL_MPC_LIVE_2 "MPC Live II"
#define CTRL_MPC_MPC_ONE "MPC One MIDI"
#define CTRL_MPC_ALL ".*(Akai.*Force$|MPC.*(Controller|One MIDI|Live II)$)"

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


// Pads Color cache captured from sysex events
typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} ForceMPCPadColor_t;


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


// Sysex patterns.
static const uint8_t AkaiSysex[]                  = {0xF0,0x47, 0x7F};
static const uint8_t MPCSysexPadColorFn[]                = {0x65,0x00,0x04};
static const uint8_t IdentityReplySysexHeader[]   = {0xF0,0x7E,0x00,0x06,0x02,0x47};
