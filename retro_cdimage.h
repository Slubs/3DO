#ifndef LIBRETRO_RETRO_CDIMAGE_H_INCLUDED
#define LIBRETRO_RETRO_CDIMAGE_H_INCLUDED

#include "cuefile.h"

#include <streams/interface_stream.h>

#define RETRO_CDIMAGE_MAX_DISKS 8

struct cdimage_s
{
  intfstream_t *fp;
  int           sector_size;
  int           sector_offset;
};

typedef struct cdimage_s cdimage_t;

struct retro_cdimage
{
  cdimage_t discs[RETRO_CDIMAGE_MAX_DISKS];
  int       num_discs;
  int       current_disc;
  char      disc_paths[RETRO_CDIMAGE_MAX_DISKS][4096];
  char      disc_labels[RETRO_CDIMAGE_MAX_DISKS][256];
};

typedef struct retro_cdimage retro_cdimage_t;

int
retro_cdimage_open_chd(const char *path_,
                       cdimage_t  *cdimage_);
int
retro_cdimage_open_iso(const char *path_,
                       cdimage_t  *cdimage_);
int
retro_cdimage_open_bin(const char *path_,
                       cdimage_t  *cdimage_);
int
retro_cdimage_open_cue(const char *path_,
                       cdimage_t  *cdimage_);
int
retro_cdimage_open(const char *path_,
                   cdimage_t  *cdimage_);

int
retro_cdimage_close(cdimage_t *cdimage_);

ssize_t
retro_cdimage_read(cdimage_t *cdimage_,
                   size_t     sector_,
                   void      *buf_,
                   size_t     bufsize_);

ssize_t
retro_cdimage_get_number_of_logical_blocks(cdimage_t *cdimage_);

/* Multi-disc support functions */
int
retro_cdimage_multi_open(retro_cdimage_t *rcd_,
                         const char *path_);

int
retro_cdimage_multi_close(retro_cdimage_t *rcd_);

int
retro_cdimage_set_disc_index(retro_cdimage_t *rcd_,
                             size_t index_);

size_t
retro_cdimage_get_disc_index(retro_cdimage_t *rcd_);

size_t
retro_cdimage_get_num_discs(retro_cdimage_t *rcd_);

const char*
retro_cdimage_get_disc_label(retro_cdimage_t *rcd_,
                             size_t index_);

const char*
retro_cdimage_get_disc_path(retro_cdimage_t *rcd_,
                            size_t index_);

cdimage_t*
retro_cdimage_get_current_cdimage(retro_cdimage_t *rcd_);

#endif