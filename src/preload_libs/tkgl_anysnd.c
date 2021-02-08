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
static void pcm_list(void);


// Globals
static snd_pcm_t *myHandleOUT, *myHandleIN;

// Internal MPC product id
enum MPCIds {
    MPC_X,MPC_LIVE,MPC_FORCE,MPC_ONE,MPC_LIVE_MK2,_END_MPCID
};

const uint8_t MPCSysexId[] = {0x3a,0x3b,0x40,0x46,0x47};
const char * MPCProductString[] = {
	"MPC X",
	"MPC Live",
	"Force",
	"MPC One",
	"MPC Live Mk II"
};


// Alsa API

// pcm

static typeof(&snd_card_get_index)             orig_snd_card_get_index;
static typeof(&snd_pcm_open)                   orig_snd_pcm_open;
static typeof(&snd_pcm_close) orig_snd_pcm_close;
static typeof(&snd_device_name_hint) orig_snd_device_name_hint;
static typeof(&snd_config_expand) orig_snd_config_expand;
static typeof(&snd_hwdep_open) orig_snd_hwdep_open;
static typeof(&snd_ctl_open) orig_snd_ctl_open;
static typeof(&snd_ctl_pcm_next_device) orig_snd_ctl_pcm_next_device;
static typeof(&snd_ctl_pcm_info) orig_snd_ctl_pcm_info;
static typeof(&snd_pcm_info_get_subdevice_name) orig_snd_pcm_info_get_subdevice_name;
static typeof(&snd_pcm_info_get_name)	orig_snd_pcm_info_get_name;

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
// Setup tkgl anyctrl
///////////////////////////////////////////////////////////////////////////////
static void tkgl_init()
{
	fprintf(stdout,"------------------------------------\n");
	fprintf (stdout,"TKGL_ANYSND V1.0 by the KikGen Labs\n");
	fprintf(stdout,"------------------------------------\n");

	// Alsa hooks
	orig_snd_card_get_index      = dlsym(RTLD_NEXT, "snd_card_get_index");
	orig_snd_pcm_open            = dlsym(RTLD_NEXT, "snd_pcm_open");
	orig_snd_device_name_hint    = dlsym(RTLD_NEXT, "snd_device_name_hint");
	orig_snd_config_expand       = dlsym(RTLD_NEXT, "snd_config_expand");
  orig_snd_hwdep_open          = dlsym(RTLD_NEXT, "snd_hwdep_open");
  orig_snd_ctl_open            = dlsym(RTLD_NEXT, "snd_ctl_open");
  orig_snd_ctl_pcm_next_device = dlsym(RTLD_NEXT, "snd_ctl_pcm_next_device");
  orig_snd_ctl_pcm_info        = dlsym(RTLD_NEXT, "snd_ctl_pcm_info");
  orig_snd_pcm_info_get_subdevice_name = dlsym(RTLD_NEXT, "snd_pcm_info_get_subdevice_name");
  orig_snd_pcm_info_get_name   = dlsym(RTLD_NEXT, "snd_pcm_info_get_name");

//pcm_list();



     // Open in blocking mode
     // if ( orig_snd_pcm_open(&myHandleOUT,"dmix",SND_PCM_STREAM_PLAYBACK, 0  ) == 0 )
     //    printf("tkgl - pcm out created\n");
     // else
     //    printf("tkgl - error pcm out\n") ;
     //
     // if ( orig_snd_pcm_open(&myHandleIN,"hw:0",SND_PCM_STREAM_CAPTURE, 0  ) == 0 )
     //    printf("tkgl - pcm in created\n");
     // else
     //    printf("tkgl - error pcm in\n") ;


	fflush(stdout);
}



static void clean_print(char * str) {

  while ( *str ) {
      if ( *str >= '0' || *str <='z' ) putchar(*str);
      str++;
  }
}

////////////////////////////////////////////////////////////////////////////////
// LIST PCM to screen (from aplay.c)
////////////////////////////////////////////////////////////////////////////////
static void pcm_list(void)
{
	void **hints, **n;
	char *name, *descr, *descr1, *io;
	const char *filter;

	if (snd_device_name_hint(-1, "pcm", &hints) < 0) return;

	n = hints;
	//filter = stream == SND_PCM_STREAM_CAPTURE ? "Input" : "Output";

  while (*n != NULL) {
		name = snd_device_name_get_hint(*n, "NAME");
		descr = snd_device_name_get_hint(*n, "DESC");
		io = snd_device_name_get_hint(*n, "IOID");
    if (name != NULL) {
      clean_print(name);
      free(name);
    }
    if (io != NULL) {
      printf(" (");
      clean_print(io);
      printf(")");
      free(io);
    }
    printf(" - ");

    if (descr != NULL)  {
      clean_print(descr);
      free(descr);
    }

    printf("\n");
		n++;
	}
	snd_device_name_free_hint(hints);
}


////////////////////////////////////////////////////////////////////////////////
// Clean DUMP of a buffer to screen
////////////////////////////////////////////////////////////////////////////////
static void ShowBufferHexDump(const uint32_t* data, uint16_t sz, uint8_t nl)
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

    /* Find the real __libc_start_main()... */
    typeof(&__libc_start_main) orig = dlsym(RTLD_NEXT, "__libc_start_main");

	tkgl_init();

    /* ... and call it with our custom main function */
    return orig(main, argc, argv, init, fini, rtld_fini, stack_end);
}

///////////////////// ALSA API HOOKS //////////////////////////////////////////

int snd_device_name_hint	(	int 	card,const char * 	iface,void *** 	hints ) {

    printf("tkgl - snd_device_name_hint. card %d, iface %s \n");
    return orig_snd_device_name_hint(card,iface,hints);

}



int snd_card_get_index	(	const char * 	snd_string	) {

	printf("tkgl - snd_card_get_index %s",snd_string);

	// if ( strcmp(snd_string,"/dev/snd/by-path/platform-ff500000.usb-usb-0:1.5:1.0") == 0 )
	//     return 0;
	//if ( *snd_string == '0') return orig_snd_card_get_index( "2"	);

	int r =  orig_snd_card_get_index( snd_string );
	printf(" - Result get index = %d\n",r);

	return r;

}
// enum snd_pcm_stream_t {
//     SND_PCM_STREAM_PLAYBACK = 0,
//     SND_PCM_STREAM_CAPTURE,
//     SND_PCM_STREAM_LAST     = SND_PCM_STREAM_CAPTURE,
// };
int snd_pcm_open	(	snd_pcm_t ** 	pcmp,const char * 	name,snd_pcm_stream_t 	stream,int 	mode  ) {

	printf("tkgl - snd_pcm_open pcmp %p, name %s, stream : %d, mode %d\n",pcmp,name,stream, mode);

	int r = orig_snd_pcm_open	(	pcmp,name,stream,mode  ) ;

	printf(" ------> Result snd_pcm_open  = %d\n",r);

	return  r;

}

// This function is called automatically by snd_pcm_open
// int snd_config_expand ( snd_config_t * 	config,snd_config_t * 	root,const char * 	args,snd_config_t * 	private_data, snd_config_t ** 	result ) {
// 	printf("tkgl - snd_config_expand config %p, root %p, args %s \n",config,root,args);
//     return orig_snd_config_expand(config,root,args,private_data,result);
// }

int snd_hwdep_open	(	snd_hwdep_t ** 	hwdep, const char * 	name, int 	mode ) {

  printf("tkgl - snd_hwdep_open hwdep %p, name %s, mode %d\n",hwdep,name,mode);
  return orig_snd_hwdep_open(hwdep, name, mode );


}

int snd_ctl_open	(	snd_ctl_t ** 	ctlp, const char * 	name, int 	mode ) {
  printf("tkgl - snd_ctl_open ctlp %p, name %s, mode %d\n",ctlp,name,mode);
  return orig_snd_ctl_open(ctlp, name, mode);
}

int snd_ctl_pcm_next_device	(	snd_ctl_t * 	ctl, int * 	device ) {
  printf("tkgl - snd_ctl_pcm_next_device ctl %p, device %d \n",ctl,device);
  return orig_snd_ctl_pcm_next_device(ctl, device);
}


int snd_ctl_pcm_info	(	snd_ctl_t * 	ctl, snd_pcm_info_t * 	info ) {

  printf("tkgl - snd_ctl_pcm_info ctl %p,  \n",ctl);
  return orig_snd_ctl_pcm_info(ctl, info);

}

const char* snd_pcm_info_get_name	(	const snd_pcm_info_t * 	pcm_obj	) {

  printf("tkgl - snd_pcm_info_get_name :  %s \n",orig_snd_pcm_info_get_name(pcm_obj));
  return orig_snd_pcm_info_get_name(pcm_obj);;

}

const char* snd_pcm_info_get_subdevice_name	(	const snd_pcm_info_t * 	pcm_obj	) {

    printf("tkgl - snd_pcm_info_get_subdevice_name :  %s \n",orig_snd_pcm_info_get_subdevice_name(pcm_obj));
    return orig_snd_pcm_info_get_subdevice_name(pcm_obj);

}
