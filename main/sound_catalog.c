#include "sound_catalog.h"

#include <string.h>
#include "esp_partition.h"

typedef struct {
    uint32_t offset;
    uint32_t length;
} SoundEntry;

typedef struct {
    char magic[4];
    uint32_t version;
    uint32_t count;
    SoundEntry entries[SOUND_TRACK_COUNT];
} SoundHeader;

_Static_assert(sizeof(SoundHeader) == 92, "sounddata header layout changed");

static const esp_partition_t *partition;
static SoundHeader header;
static esp_partition_mmap_handle_t mapped_handle;
static bool ready;
static bool mapped;

// Titles use GB2312 bytes, matching the display's native font encoding.
static const char *const titles[SOUND_TRACK_COUNT] = {
    "\xD5\xE6\xD0\xC2\xD5\xF2",               // Pallet Town
    "\xB1\xA6\xBF\xC9\xC3\xCE\xD6\xD0\xD0\xC4", // Pokemon Center
    "\xD7\xCF\xD4\xB7\xD5\xF2",               // Lavender Town
    "\xD1\xB5\xC1\xB7\xBC\xD2\xB6\xD4\xD5\xBD", // Trainer Battle
    "\xC8\xF4\xD2\xB6\xD5\xF2",               // New Bark Town
    "\xD7\xD4\xC8\xBB\xB9\xAB\xD4\xB0",     // National Park
    "\xD4\xB2\xD6\xEC\xCA\xD0",               // Ecruteak City
    "\xCE\xB4\xB0\xD7\xD5\xF2",               // Littleroot Town
    "113\xBA\xC5\xB5\xC0\xC2\xB7",            // Route 113
    "\xBF\xAD\xC4\xC7\xCA\xD0",               // Slateport City
};

const char *soundTrackTitle(uint8_t slot)
{
    return slot < SOUND_TRACK_COUNT ? titles[slot] : "";
}

bool soundCatalogInit(void)
{
    partition = esp_partition_find_first(0x60, 0, "sounddata");
    ready = false;
    if (!partition ||
        esp_partition_read(partition, 0, &header, sizeof(header)) != ESP_OK ||
        memcmp(header.magic, "GMP3", 4) != 0 ||
        header.version != 1 || header.count != SOUND_TRACK_COUNT)
        return false;
    for (unsigned i = 0; i < SOUND_TRACK_COUNT; ++i) {
        const SoundEntry *entry = &header.entries[i];
        if (entry->offset < 4096 || entry->length < 4 ||
            entry->offset > partition->size ||
            entry->length > partition->size - entry->offset)
            return false;
    }
    ready = true;
    return true;
}

bool soundCatalogReady(void) { return ready; }

void soundCatalogClose(void)
{
    if (mapped) {
        esp_partition_munmap(mapped_handle);
        mapped = false;
    }
}

bool soundCatalogOpen(uint8_t slot, const uint8_t **file, size_t *length)
{
    soundCatalogClose();
    if (!ready || slot >= SOUND_TRACK_COUNT || !file || !length) return false;
    const SoundEntry *entry = &header.entries[slot];
    if (esp_partition_mmap(partition, entry->offset, entry->length,
                           ESP_PARTITION_MMAP_DATA, (const void **)file,
                           &mapped_handle) != ESP_OK)
        return false;
    mapped = true;
    *length = entry->length;
    if ((*file)[0] != 0xff || ((*file)[1] & 0xe6) != 0xe2) {
        soundCatalogClose();
        return false;
    }
    return true;
}
