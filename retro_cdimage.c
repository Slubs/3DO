#include "retro_cdimage.h"

#include "retro_miscellaneous.h"

#include "file/file_path.h"
#include "streams/chd_stream.h"
#include "streams/interface_stream.h"

#include "endianness.h"

#include <stdlib.h>
#include <string.h>

static
void
cdimage_set_size_and_offset(cdimage_t *cd_,
                            const int  size_,
                            const int  offset_)
{
  cd_->sector_size   = size_;
  cd_->sector_offset = offset_;
}

int
retro_cdimage_open_chd(const char *path_,
                       cdimage_t  *cdimage_)
{
  uint8_t buf[8];
  uint8_t pattern[8] = { 0x01, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a, 0x01, 0x00 };

  cdimage_->fp = intfstream_open_chd_track(path_,
                                           RETRO_VFS_FILE_ACCESS_READ,
                                           RETRO_VFS_FILE_ACCESS_HINT_NONE,
                                           CHDSTREAM_TRACK_PRIMARY);
  if(cdimage_->fp == NULL)
    return -1;

  intfstream_seek(cdimage_->fp,0,RETRO_VFS_SEEK_POSITION_START);
  intfstream_read(cdimage_->fp,buf,8);
  intfstream_seek(cdimage_->fp,0,RETRO_VFS_SEEK_POSITION_START);

  /* MODE1 */
  if(!memcmp(buf,pattern,sizeof(pattern)))
    cdimage_set_size_and_offset(cdimage_,2448,0);
  else /* MODE1_RAW */
    cdimage_set_size_and_offset(cdimage_,2352,16);

  return 0;
}

int
retro_cdimage_open_iso(const char *path_,
                       cdimage_t  *cdimage_)
{
  int size;

  cdimage_->fp = intfstream_open_file(path_,
                                      RETRO_VFS_FILE_ACCESS_READ,
                                      RETRO_VFS_FILE_ACCESS_HINT_NONE);
  if(cdimage_->fp == NULL)
    return -1;

  size = intfstream_get_size(cdimage_->fp);
  if((size % 2048) == 0)
    cdimage_set_size_and_offset(cdimage_,2048,0);
  else if((size % 2352) == 0)
    cdimage_set_size_and_offset(cdimage_,2352,16);
  else
    cdimage_set_size_and_offset(cdimage_,2048,0);

  return 0;
}

int
retro_cdimage_open_bin(const char *path_,
                       cdimage_t  *cdimage_)
{
  return retro_cdimage_open_iso(path_,cdimage_);
}

int
retro_cdimage_open_cue(const char *path_,
                       cdimage_t  *cdimage_)
{
  int rv;
  const char *ext;
  cueFile *cue_file;
  intfstream_t *stream;

  cue_file = cue_get(path_);
  if(cue_file == NULL)
    return -1;

  ext = path_get_extension(cue_file->cd_image);
  if(!strcasecmp(ext,"iso"))
    rv = retro_cdimage_open_iso(cue_file->cd_image,cdimage_);
  else if(!strcasecmp(ext,"bin"))
    rv = retro_cdimage_open_bin(cue_file->cd_image,cdimage_);
  else if(!strcasecmp(ext,"img"))
    rv = retro_cdimage_open_bin(cue_file->cd_image,cdimage_);
  else
    rv = -1;

  if(rv == -1)
    {
      free(cue_file);
      return -1;
    }

  switch(cue_file->cd_format)
    {
    case MODE1_2048:
      cdimage_set_size_and_offset(cdimage_,2048,0);
      break;
    case MODE1_2352:
      cdimage_set_size_and_offset(cdimage_,2352,16);
      break;
    case MODE2_2352:
      cdimage_set_size_and_offset(cdimage_,2352,24);
      break;
    default:
    case CUE_MODE_UNKNOWN:
      cdimage_set_size_and_offset(cdimage_,2048,0);
      break;
    }

  free(cue_file);

  return 0;
}

int
retro_cdimage_open(const char *path_,
                   cdimage_t  *cdimage_)
{
  const char *ext;

  ext = path_get_extension(path_);
  if(ext == NULL)
    return -1;

  if(!strcasecmp(ext,"chd"))
    return retro_cdimage_open_chd(path_,cdimage_);
  if(!strcasecmp(ext,"cue"))
    return retro_cdimage_open_cue(path_,cdimage_);
  if(!strcasecmp(ext,"iso"))
    return retro_cdimage_open_iso(path_,cdimage_);
  if(!strcasecmp(ext,"bin"))
    return retro_cdimage_open_bin(path_,cdimage_);

  return -1;
}

int
retro_cdimage_close(cdimage_t *cdimage_)
{
  int rv;

  rv = 0;
  if(cdimage_->fp)
    rv = intfstream_close(cdimage_->fp);

  cdimage_->fp            = NULL;
  cdimage_->sector_size   = 0;
  cdimage_->sector_offset = 0;

  return rv;
}

ssize_t
retro_cdimage_read(cdimage_t *cdimage_,
                   size_t     sector_,
                   void      *buf_,
                   size_t     bufsize_)
{
  int rv;
  size_t pos;

  bufsize_ = MIN(bufsize_, cdimage_->sector_size);
  pos      = ((sector_ * cdimage_->sector_size) + cdimage_->sector_offset);

  rv = intfstream_seek(cdimage_->fp,pos,RETRO_VFS_SEEK_POSITION_START);
  if(rv == -1)
    return -1;

  return intfstream_read(cdimage_->fp,buf_,bufsize_);
}

ssize_t
retro_cdimage_get_number_of_logical_blocks(cdimage_t *cdimage_)
{
  int rv;
  size_t pos;
  uint32_t blocks;

  pos = (cdimage_->sector_offset + 80);
  rv = intfstream_seek(cdimage_->fp,pos,RETRO_VFS_SEEK_POSITION_START);
  if(rv == -1)
    return -1;

  rv = intfstream_read(cdimage_->fp,&blocks,sizeof(blocks));
  if(rv == -1)
    return -1;

  return swap32_if_little_endian(blocks);
}

/* Multi-disc support functions */

/* Parse a CUE file and extract track information including multiple files */
static int parse_cue_file(const char *path_, retro_cdimage_t *rcd_)
{
  char line[4096];
  int files_found = 0;
  RFILE *cue_file = NULL;
  char base_path[4096];
  char *last_separator = NULL;
#ifdef _WIN32
  char slash = '\\';
#else
  char slash = '/';
#endif

  strcpy(base_path, path_);
  last_separator = strrchr(base_path, slash);
  if (last_separator)
    *last_separator = '\0';
  else
    base_path[0] = '\0';

  cue_file = filestream_open(path_, RETRO_VFS_FILE_ACCESS_READ, 0);
  if (!cue_file)
    return -1;

  rcd_->num_discs = 0;
  rcd_->current_disc = 0;

  while ((filestream_gets(cue_file, line, sizeof(line))) && rcd_->num_discs < RETRO_CDIMAGE_MAX_DISKS)
  {
    char *file_line = strstr(line, "FILE");
    if (file_line && files_found < RETRO_CDIMAGE_MAX_DISKS)
    {
      char file[4096];
      char *file_name_start = strstr(file_line, "\"");
      
      if (file_name_start)
      {
        file_name_start++;
        char *file_name_end = strstr(file_name_start, "\"");
        if (file_name_end)
        {
          size_t len = file_name_end - file_name_start;
          if (len < sizeof(file))
          {
            strncpy(file, file_name_start, len);
            file[len] = '\0';
            
            /* Build full path */
            if (base_path[0])
              snprintf(rcd_->disc_paths[files_found], sizeof(rcd_->disc_paths[0]), "%s%c%s", base_path, slash, file);
            else
              strncpy(rcd_->disc_paths[files_found], file, sizeof(rcd_->disc_paths[0]));
            
            /* Extract label from filename */
            char *label = strrchr(file, '/');
            if (!label)
              label = strrchr(file, '\\');
            if (label)
              label++;
            else
              label = file;
            
            char *dot = strrchr(label, '.');
            if (dot)
            {
              size_t label_len = dot - label;
              if (label_len < sizeof(rcd_->disc_labels[0]))
              {
                strncpy(rcd_->disc_labels[files_found], label, label_len);
                rcd_->disc_labels[files_found][label_len] = '\0';
              }
            }
            else
            {
              strncpy(rcd_->disc_labels[files_found], label, sizeof(rcd_->disc_labels[0]) - 1);
            }
            
            files_found++;
          }
        }
      }
    }
  }
  
  filestream_close(cue_file);
  rcd_->num_discs = files_found;
  
  /* If only one file, use original path */
  if (rcd_->num_discs == 1 && rcd_->disc_paths[0][0] == '\0')
    strncpy(rcd_->disc_paths[0], path_, sizeof(rcd_->disc_paths[0]) - 1);
  
  return (rcd_->num_discs > 0) ? 0 : -1;
}

int
retro_cdimage_multi_open(retro_cdimage_t *rcd_,
                         const char *path_)
{
  const char *ext;
  int i;
  
  if (!rcd_ || !path_)
    return -1;
  
  memset(rcd_, 0, sizeof(retro_cdimage_t));
  rcd_->current_disc = 0;
  
  ext = path_get_extension(path_);
  if (ext == NULL)
    return -1;
  
  /* For CUE files with multiple tracks, parse the CUE file */
  if (!strcasecmp(ext, "cue"))
  {
    if (parse_cue_file(path_, rcd_) == 0)
    {
      /* Open the first disc */
      for (i = 0; i < rcd_->num_discs; i++)
      {
        if (rcd_->disc_paths[i][0] != '\0')
        {
          if (retro_cdimage_open(rcd_->disc_paths[i], &rcd_->discs[i]) != 0)
          {
            /* Failed to open this disc, try to continue with what we have */
            rcd_->discs[i].fp = NULL;
          }
        }
      }
      
      /* Open the first valid disc as current */
      if (rcd_->discs[0].fp == NULL)
      {
        /* Fall back to opening the CUE file directly */
        return retro_cdimage_open(path_, &rcd_->discs[0]);
      }
      
      rcd_->current_disc = 0;
      return 0;
    }
  }
  
  /* For single-file formats (ISO, BIN, CHD), treat as single disc */
  rcd_->num_discs = 1;
  strncpy(rcd_->disc_paths[0], path_, sizeof(rcd_->disc_paths[0]) - 1);
  
  /* Extract label from path */
  const char *label = strrchr(path_, '/');
  if (!label)
    label = strrchr(path_, '\\');
  if (label)
    label++;
  else
    label = path_;
  
  char *dot = strrchr(label, '.');
  if (dot)
  {
    size_t label_len = dot - label;
    if (label_len < sizeof(rcd_->disc_labels[0]))
    {
      strncpy(rcd_->disc_labels[0], label, label_len);
      rcd_->disc_labels[0][label_len] = '\0';
    }
  }
  else
  {
    strncpy(rcd_->disc_labels[0], label, sizeof(rcd_->disc_labels[0]) - 1);
  }
  
  return retro_cdimage_open(path_, &rcd_->discs[0]);
}

int
retro_cdimage_multi_close(retro_cdimage_t *rcd_)
{
  int i;
  
  if (!rcd_)
    return -1;
  
  for (i = 0; i < rcd_->num_discs; i++)
  {
    if (rcd_->discs[i].fp)
      retro_cdimage_close(&rcd_->discs[i]);
  }
  
  memset(rcd_, 0, sizeof(retro_cdimage_t));
  
  return 0;
}

int
retro_cdimage_set_disc_index(retro_cdimage_t *rcd_,
                             size_t index_)
{
  if (!rcd_ || index_ >= (size_t)rcd_->num_discs)
    return -1;
  
  rcd_->current_disc = (int)index_;
  return 0;
}

size_t
retro_cdimage_get_disc_index(retro_cdimage_t *rcd_)
{
  if (!rcd_)
    return 0;
  
  return (size_t)rcd_->current_disc;
}

size_t
retro_cdimage_get_num_discs(retro_cdimage_t *rcd_)
{
  if (!rcd_)
    return 1;
  
  return (rcd_->num_discs > 0) ? (size_t)rcd_->num_discs : 1;
}

const char*
retro_cdimage_get_disc_label(retro_cdimage_t *rcd_,
                             size_t index_)
{
  if (!rcd_ || index_ >= (size_t)rcd_->num_discs)
    return NULL;
  
  return rcd_->disc_labels[index_];
}

const char*
retro_cdimage_get_disc_path(retro_cdimage_t *rcd_,
                            size_t index_)
{
  if (!rcd_ || index_ >= (size_t)rcd_->num_discs)
    return NULL;
  
  return rcd_->disc_paths[index_];
}

cdimage_t*
retro_cdimage_get_current_cdimage(retro_cdimage_t *rcd_)
{
  if (!rcd_ || rcd_->current_disc < 0 || rcd_->current_disc >= rcd_->num_discs)
    return NULL;
  
  return &rcd_->discs[rcd_->current_disc];
}