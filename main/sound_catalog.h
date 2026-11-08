#ifndef GVIEW_SOUND_CATALOG_H
#define GVIEW_SOUND_CATALOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SOUND_TRACK_COUNT 10
#define SOUND_DEFAULT_HOME_TRACK 7

const char *soundTrackTitle(uint8_t slot);
bool soundCatalogInit(void);
bool soundCatalogReady(void);
bool soundCatalogOpen(uint8_t slot, const uint8_t **file, size_t *length);
void soundCatalogClose(void);

#endif
