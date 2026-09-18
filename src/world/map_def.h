#ifndef MAP_DEF_H
#define MAP_DEF_H

#include "world/theme.h"

typedef struct {
    const char *name;  /* display name, shown in the map selector */
    const char *file;  /* assets/maps/<file>.map */
    ThemeID theme;
} MapDef;

#endif