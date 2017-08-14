#ifndef _VS1053_PLUGINS_H_
#define _VS1053_PLUGINS_H_

#include "types.h"

// Need to load one of these patches for some AAC files to play
//#define VS1053B_PATCHES_PLUGIN
#define VS1053B_PATCHES_LATM_PLUGIN
//#define VS1053B_PATCHES_FLAC_PLUGIN
//#define VS1053B_PATCHES_FLAC_LATM_PLUGIN

#if defined VS1053B_PATCHES_PLUGIN
# define VS1053B_PATCH_PLUGIN_SIZE  3147
#elif defined VS1053B_PATCHES_LATM_PLUGIN
# define VS1053B_PATCH_PLUGIN_SIZE  4351
#elif defined VS1053B_PATCHES_FLAC_PLUGIN
# define VS1053B_PATCH_PLUGIN_SIZE  8161
#elif defined VS1053B_PATCHES_FLAC_LATM_PLUGIN
# define VS1053B_PATCH_PLUGIN_SIZE  8582
#endif

extern uint16_t const vs1053b_patches_plugin[];

#endif
