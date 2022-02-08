/*
__ __| |           |  /_) |     ___|             |           |
  |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
  |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
 _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
-----------------------------------------------------------------------------
TKGL_STRCMP : Print all strcmp in an application.
Preload syntax is :
	LD_PRELOAD=/full/path/to/tkgl_strcmp.so /usr/bin/MPC
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
#include <string.h>

#include <stdint.h>
#include <dlfcn.h>

// Alsa API
static typeof(&strcmp) orig_strcmp;

int strcmp(const char* s1, const char* s2){
		orig_strcmp = dlsym(RTLD_NEXT, "strcmp");

    fprintf(stdout,"(tkgl_strcmp) s1: \"%s\" -- s2: \"%s\"\n", s1,s2);

    return orig_strcmp(s1,s2);
}
