#include "file/file_path.h"
#include "libretro.h"
#include "libretro_core_options.h"
#include "retro_miscellaneous.h"
#include "streams/file_stream.h"

#include "lr_input.h"
#include "lr_input_crosshair.h"
#include "lr_input_descs.h"
#include "opera_lr_callbacks.h"
#include "opera_lr_dsp.h"
#include "opera_lr_nvram.h"
#include "opera_lr_opts.h"
#include "retro_cdimage.h"

#include "libopera/hack_flags.h"
#include "libopera/opera_3do.h"
#include "libopera/opera_arm.h"
#include "libopera/opera_bios.h"
#include "libopera/opera_cdrom.h"
#include "libopera/opera_clock.h"
#include "libopera/opera_core.h"
#include "libopera/opera_log.h"
#include "libopera/opera_madam.h"
#include "libopera/opera_mem.h"
#include "libopera/opera_nvram.h"
#include "libopera/opera_pbus.h"
#include "libopera/opera_region.h"
#include "libopera/opera_vdlp.h"
#include "libopera/prng16.h"
#include "libopera/prng32.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * CD image sector size in bytes.
 * Standard Mode1 CD-ROM sector size (2048 bytes of user data).
 */
#define CDIMAGE_SECTOR_SIZE 2048

/*
 * Global state variables.
 * Note: These are global for performance reasons in an emulator context.
 * For better encapsulation, consider refactoring into a state structure.
 */
static retro_cdimage_t CDIMAGE;        /* Current CD image (multi-disc support) */
static uint32_t        CDIMAGE_SECTOR; /* Current sector position */
static char           *g_GAME_INFO_PATH; /* Path to loaded game */

/* Disk control interface callbacks */
static retro_environment_t retro_environment_cb_static = NULL;

static
void
retro_environment_set_support_no_game(void)
{
  bool support_no_game = true;

  retro_environment_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME,&support_no_game);
}

static
void
retro_environment_set_controller_info(void)
{
  static const struct retro_controller_description port[] =
    {
     { "3DO Joypad",        RETRO_DEVICE_JOYPAD },
     { "3DO Flightstick",   RETRO_DEVICE_FLIGHTSTICK },
     { "3DO Mouse",         RETRO_DEVICE_MOUSE  },
     { "3DO Lightgun",      RETRO_DEVICE_LIGHTGUN },
     { "Arcade Lightgun",   RETRO_DEVICE_ARCADE_LIGHTGUN },
     { "Orbatak Trackball", RETRO_DEVICE_ORBATAK_TRACKBALL },
    };

  static const struct retro_controller_info ports[LR_INPUT_MAX_DEVICES+1] =
    {
     {port, 6},
     {port, 6},
     {port, 6},
     {port, 6},
     {port, 6},
     {port, 6},
     {port, 6},
     {port, 6},
     {NULL, 0}
    };

  retro_environment_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO,(void*)ports);
}

static
void
retro_vfs_initialize(void)
{
  struct retro_vfs_interface_info vfs_info;

  vfs_info.required_interface_version = 1;
  vfs_info.iface                      = NULL;

  if(retro_environment_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE,&vfs_info))
    filestream_vfs_init(&vfs_info);
}

void
retro_set_environment(retro_environment_t cb_)
{
  retro_environment_cb_static = cb_;
  opera_lr_callbacks_set_environment(cb_);

  retro_vfs_initialize();
  retro_environment_set_controller_info();
  libretro_init_core_options();
  libretro_set_core_options();
  retro_environment_set_support_no_game();
}

void
retro_set_video_refresh(retro_video_refresh_t cb_)
{
  opera_lr_callbacks_set_video_refresh(cb_);
}

void
retro_set_audio_sample(retro_audio_sample_t cb_)
{
  opera_lr_callbacks_set_audio_sample(cb_);
}

void
retro_set_audio_sample_batch(retro_audio_sample_batch_t cb_)
{
  opera_lr_callbacks_set_audio_sample_batch(cb_);
}

void
retro_set_input_poll(retro_input_poll_t cb_)
{
  opera_lr_callbacks_set_input_poll(cb_);
}

void
retro_set_input_state(retro_input_state_t cb_)
{
  opera_lr_callbacks_set_input_state(cb_);
}

static
uint32_t
cdimage_get_size(void)
{
  cdimage_t *current = retro_cdimage_get_current_cdimage(&CDIMAGE);
  if (current == NULL)
    return 0;
  return retro_cdimage_get_number_of_logical_blocks(current);
}

static
void
cdimage_set_sector(const uint32_t sector_)
{
  CDIMAGE_SECTOR = sector_;
}

static
void
cdimage_read_sector(void *buf_)
{
  cdimage_t *current = retro_cdimage_get_current_cdimage(&CDIMAGE);
  if (current != NULL)
    retro_cdimage_read(current, CDIMAGE_SECTOR, buf_, CDIMAGE_SECTOR_SIZE);
}

static
void*
libopera_callback(int   cmd_,
                  void *data_)
{
  switch(cmd_)
    {
    case EXT_DSP_TRIGGER:
      opera_lr_dsp_process();
      break;
    default:
      break;
    }

  return NULL;
}

#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
void
retro_get_system_info(struct retro_system_info *info_)
{
  memset(info_,0,sizeof(*info_));

  info_->library_name     = "Opera";
  info_->library_version  = "1.0.0" GIT_VERSION;
  info_->need_fullpath    = true;
  info_->valid_extensions = "iso|bin|chd|cue";
}

size_t
retro_serialize_size(void)
{
  return opera_3do_state_size();
}

bool
retro_serialize(void   *data_,
                size_t  size_)
{
  uint32_t size;

  size = opera_3do_state_save(data_,size_);

  return (size == size_);
}

bool
retro_unserialize(void const *data_,
                  size_t      size_)
{
  uint32_t size;
  uint32_t expected_size;
  void *backup_state;

  /* Validate input parameters */
  if (data_ == NULL || size_ == 0)
  {
    retro_log_printf_cb(RETRO_LOG_ERROR,
                        "[Opera]: retro_unserialize called with invalid parameters\n");
    return false;
  }

  expected_size = retro_serialize_size();
  
  /* Validate size parameter */
  if (size_ != expected_size)
  {
    retro_log_printf_cb(RETRO_LOG_WARN,
                        "[Opera]: savestate size mismatch (expected %zu, got %zu)\n",
                        expected_size, size_);
    /* Continue anyway - some size differences may be acceptable */
  }

  backup_state = malloc(expected_size);
  if(backup_state == NULL)
  {
    retro_log_printf_cb(RETRO_LOG_ERROR,
                        "[Opera]: failed to allocate memory for savestate backup\n");
    return false;
  }
  
  /* Create backup of current state */
  size = retro_serialize(backup_state, expected_size);
  if(size == 0 || size != expected_size)
  {
    retro_log_printf_cb(RETRO_LOG_ERROR,
                        "[Opera]: failed to create savestate backup\n");
    free(backup_state);
    return false;
  }
  
  /* Attempt to load new state */
  size = opera_3do_state_load(data_, size_);
  if(size != size_)
  {
    /* Load failed, restore backup */
    retro_log_printf_cb(RETRO_LOG_WARN,
                        "[Opera]: failed to load savestate, restoring previous state\n");
    if (opera_3do_state_load(backup_state, expected_size) != expected_size)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[Opera]: failed to restore savestate backup!\n");
    }
    free(backup_state);
    return false;
  }

  free(backup_state);
  return true;
}

/*
 * Cheat support is not implemented in this core.
 * The 3DO platform does not have a standard cheat system.
 */
void
retro_cheat_reset(void)
{
  /* Not implemented - 3DO has no standard cheat system */
  retro_log_printf_cb(RETRO_LOG_DEBUG,
                      "[Opera]: retro_cheat_reset not implemented\n");
}

void
retro_cheat_set(unsigned    index_,
                bool        enabled_,
                const char *code_)
{
  /* Not implemented - 3DO has no standard cheat system */
  (void)index_;
  (void)enabled_;
  (void)code_;
  retro_log_printf_cb(RETRO_LOG_DEBUG,
                      "[Opera]: retro_cheat_set not implemented\n");
}

void
retro_set_controller_port_device(unsigned port_,
                                 unsigned device_)
{
  lr_input_device_set_with_descs(port_,device_);
}

static
enum retro_pixel_format
vdlp_pixel_format_to_libretro(vdlp_pixel_format_e pf_)
{
  switch (pf_)
    {
    case VDLP_PIXEL_FORMAT_0RGB1555:
      return RETRO_PIXEL_FORMAT_0RGB1555;
    case VDLP_PIXEL_FORMAT_RGB565:
      return RETRO_PIXEL_FORMAT_RGB565;
    case VDLP_PIXEL_FORMAT_XRGB8888:
      return RETRO_PIXEL_FORMAT_XRGB8888;
    }

  return RETRO_PIXEL_FORMAT_XRGB8888;
}

static
int
set_pixel_format(void)
{
  int rv;
  enum retro_pixel_format fmt;

  fmt = vdlp_pixel_format_to_libretro(g_OPTS.vdlp_pixel_format);
  rv  = retro_environment_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,&fmt);
  if(rv == 0)
    {
      retro_log_printf_cb(RETRO_LOG_ERROR,
                          "[Opera]: pixel format is not supported.\n");
      return -1;
    }

  return 0;
}

static
int
print_cdimage_open_fail(const char *path_)
{
  retro_log_printf_cb(RETRO_LOG_ERROR,
                      "[Opera]: failure opening image - %s\n",
                      path_);
  return -1;
}

static
int
check_bios_file(const opera_bios_t *bios_)
{
  char bios_path[4096];
  char *system_dir = NULL;
  RFILE *fp = NULL;
  long file_size;
  
  /* Try to get system directory from frontend */
  if (retro_environment_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
  {
    snprintf(bios_path, sizeof(bios_path), "%s/%s", system_dir, bios_->filename);
    fp = filestream_open(bios_path, RETRO_VFS_FILE_ACCESS_READ, 0);
  }
  
  if (fp == NULL)
  {
    retro_log_printf_cb(RETRO_LOG_WARN,
                        "[Opera]: BIOS file not found: %s\n",
                        bios_->filename);
    return -1;
  }
  
  file_size = filestream_get_size(fp);
  filestream_close(fp);
  
  if (file_size != (long)bios_->size)
  {
    retro_log_printf_cb(RETRO_LOG_WARN,
                        "[Opera]: BIOS file %s has wrong size (expected %lu, got %ld)\n",
                        bios_->filename,
                        (unsigned long)bios_->size,
                        file_size);
    return -1;
  }
  
  retro_log_printf_cb(RETRO_LOG_INFO,
                      "[Opera]: Found BIOS: %s (%s) - %s\n",
                      bios_->filename,
                      bios_->name,
                      bios_->version);
  
  return 0;
}

static
int
open_cdimage_if_needed(const struct retro_game_info *info_)
{
  int rv;
  const opera_bios_t *bios;
  int bios_found = 0;

  if(!info_)
    return 0;

  rv = retro_cdimage_multi_open(&CDIMAGE, info_->path);
  if(rv == -1)
    return print_cdimage_open_fail(info_->path);

  /* Check for BIOS files */
  for (bios = opera_bios_begin(); bios != opera_bios_end(); bios++)
  {
    if (bios->filename == NULL)
      break;
    
    if (check_bios_file(bios) == 0)
    {
      bios_found = 1;
      break;  /* Found a valid BIOS */
    }
  }
  
  if (!bios_found)
  {
    retro_log_printf_cb(RETRO_LOG_ERROR,
                        "[Opera]: No valid BIOS file found!\n"
                        "[Opera]: Please place one of the following BIOS files in the system directory:\n"
                        "[Opera]:   - panafz1.bin (Panasonic FZ-1 U)\n"
                        "[Opera]:   - panafz1j.bin (Panasonic FZ-1 J)\n"
                        "[Opera]:   - panafz1j-norsa.bin (Panasonic FZ-1 J No RSA)\n"
                        "[Opera]:   - panafz10.bin (Panasonic FZ-10 U)\n"
                        "[Opera]:   - panafz10-norsa.bin (Panasonic FZ-10 U No RSA)\n"
                        "[Opera]:   - panafz10e-anvil.bin (Panasonic FZ-10 E)\n"
                        "[Opera]:   - panafz10e-anvil-norsa.bin (Panasonic FZ-10 E No RSA)\n"
                        "[Opera]:   - goldstar.bin (Goldstar GDO-101M)\n"
                        "[Opera]:   - sanyotry.bin (Sanyo Try IMP-21J)\n"
                        "[Opera]:   - 3do_arcade_saot.bin (3DO Arcade)\n");
    return -1;
  }

  return 0;
}

static
void
game_info_path_free(void)
{
  if(g_GAME_INFO_PATH == NULL)
    return;

  free(g_GAME_INFO_PATH);
  g_GAME_INFO_PATH = NULL;
}

static
void
game_info_path_save(const struct retro_game_info *info_)
{
  size_t len;

  game_info_path_free();

  if((info_ == NULL) || (info_->path == NULL))
    return;

  g_GAME_INFO_PATH = strdup(info_->path);
}

static
const
char*
game_info_path_get(void)
{
  return g_GAME_INFO_PATH;
}

/* Disk control ext interface implementation */
static size_t disk_get_num_images(void)
{
  return retro_cdimage_get_num_discs(&CDIMAGE);
}

static bool disk_set_image_index(unsigned index)
{
  cdimage_t *old_disc = retro_cdimage_get_current_cdimage(&CDIMAGE);
  
  if (retro_cdimage_set_disc_index(&CDIMAGE, index) != 0)
    return false;
  
  cdimage_t *new_disc = retro_cdimage_get_current_cdimage(&CDIMAGE);
  if (new_disc == NULL || new_disc->fp == NULL)
  {
    /* Failed to switch, restore old disc */
    retro_cdimage_set_disc_index(&CDIMAGE, retro_cdimage_get_disc_index(&CDIMAGE));
    return false;
  }
  
  /* Reset sector to 0 on disc change */
  CDIMAGE_SECTOR = 0;
  
  retro_log_printf_cb(RETRO_LOG_INFO,
                      "[Opera]: Disk changed to index %u (%s)\n",
                      index,
                      retro_cdimage_get_disc_label(&CDIMAGE, index));
  
  return true;
}

static unsigned disk_get_image_index(void)
{
  return (unsigned)retro_cdimage_get_disc_index(&CDIMAGE);
}

static const char* disk_get_image_path(unsigned index)
{
  return retro_cdimage_get_disc_path(&CDIMAGE, index);
}

static const char* disk_get_image_label(unsigned index)
{
  return retro_cdimage_get_disc_label(&CDIMAGE, index);
}

static bool disk_set_eject_state(bool ejected)
{
  /* Eject state is handled by the frontend, we just acknowledge it */
  return true;
}

static bool disk_get_eject_state(void)
{
  /* We don't track eject state internally */
  return false;
}

static bool disk_insert_image(const char *image_path, unsigned index)
{
  /* For now, we don't support hot-inserting new images */
  retro_log_printf_cb(RETRO_LOG_WARN,
                      "[Opera]: Hot-inserting images is not supported\n");
  return false;
}

static bool disk_replace_image(const char *image_path, bool persistent)
{
  /* For now, we don't support replacing images */
  retro_log_printf_cb(RETRO_LOG_WARN,
                      "[Opera]: Replacing images is not supported\n");
  return false;
}

static bool disk_add_image(void)
{
  /* For now, we don't support adding images dynamically */
  retro_log_printf_cb(RETRO_LOG_WARN,
                      "[Opera]: Adding images dynamically is not supported\n");
  return false;
}

static bool disk_remove_image(void)
{
  /* For now, we don't support removing images dynamically */
  retro_log_printf_cb(RETRO_LOG_WARN,
                      "[Opera]: Removing images dynamically is not supported\n");
  return false;
}

static bool disk_clear_image(void)
{
  /* For now, we don't support clearing images */
  retro_log_printf_cb(RETRO_LOG_WARN,
                      "[Opera]: Clearing images is not supported\n");
  return false;
}

/*
 * Disk control ext interface implementation.
 * Note: Function order must match retro_disk_control_ext_callback structure.
 */
static struct retro_disk_control_ext_callback disk_control_cb = {
  disk_set_eject_state,      /* set_eject_state */
  disk_get_eject_state,      /* get_eject_state */
  disk_get_num_images,       /* get_num_images */
  disk_set_image_index,      /* set_image_index */
  disk_get_image_index,      /* get_image_index */
  disk_get_image_path,       /* get_image_path */
  disk_get_image_label,      /* get_image_label */
  NULL,                      /* set_initial_image - not implemented */
  disk_insert_image,         /* insert_image */
  disk_replace_image,        /* replace_image */
  disk_add_image,            /* add_image */
  disk_remove_image,         /* remove_image */
  disk_clear_image           /* clear_image */
};

bool
retro_load_game(const struct retro_game_info *info_)
{
  int rv;
  unsigned disk_version = 0;

  game_info_path_save(info_);

  rv = open_cdimage_if_needed(info_);
  if(rv == -1)
    return false;

  opera_lr_opts_process();
  opera_3do_init(libopera_callback);
  cdimage_set_sector(0);

  rv = set_pixel_format();
  if(rv < 0)
    return false;

  opera_lr_nvram_load(game_info_path_get(),
                      g_OPTS.nvram_shared,
                      g_OPTS.nvram_version);

  /* Register disk control interface if available */
  if (retro_environment_cb(RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION, &disk_version))
  {
    if (disk_version >= 1)
    {
      retro_environment_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE, &disk_control_cb);
      retro_log_printf_cb(RETRO_LOG_INFO,
                          "[Opera]: Disk control interface registered (version %u, %u discs)\n",
                          disk_version,
                          (unsigned)retro_cdimage_get_num_discs(&CDIMAGE));
    }
  }

  return true;
}

bool
retro_load_game_special(unsigned                      game_type_,
                        const struct retro_game_info *info_,
                        size_t                        num_info_)
{
  return false;
}

void
retro_unload_game(void)
{
  opera_lr_nvram_save(game_info_path_get(),
                      g_OPTS.nvram_shared,
                      g_OPTS.nvram_version);
  game_info_path_free();

  opera_3do_destroy();

  retro_cdimage_multi_close(&CDIMAGE);

  opera_lr_opts_reset();
}

void
retro_get_system_av_info(struct retro_system_av_info *info_)
{
  memset(info_,0,sizeof(*info_));

  info_->timing.fps            = opera_region_field_rate();
  info_->timing.sample_rate    = 44100;
  info_->geometry.base_width   = opera_region_min_width();
  info_->geometry.base_height  = opera_region_min_height();
  info_->geometry.max_width    = (opera_region_max_width()  * 2);
  info_->geometry.max_height   = (opera_region_max_height() * 2);
  info_->geometry.aspect_ratio = 4.0 / 3.0;
}

unsigned
retro_get_region(void)
{
  switch(opera_region_get())
    {
    case OPERA_REGION_PAL1:
    case OPERA_REGION_PAL2:
      return RETRO_REGION_PAL;
    case OPERA_REGION_NTSC:
    default:
      break;
    }

  return RETRO_REGION_NTSC;
}

unsigned
retro_api_version(void)
{
  return RETRO_API_VERSION;
}

void*
retro_get_memory_data(unsigned id_)
{
  switch(id_)
    {
    case RETRO_MEMORY_SAVE_RAM:
      return NULL;
    case RETRO_MEMORY_SYSTEM_RAM:
      return DRAM;
    case RETRO_MEMORY_VIDEO_RAM:
      return VRAM;
    }

  return NULL;
}

size_t
retro_get_memory_size(unsigned id_)
{
  switch(id_)
    {
    case RETRO_MEMORY_SAVE_RAM:
      return 0;
    case RETRO_MEMORY_SYSTEM_RAM:
      return DRAM_SIZE;
    case RETRO_MEMORY_VIDEO_RAM:
      return VRAM_SIZE;
    }

  return 0;
}

void
retro_init(void)
{
  struct retro_log_callback log;
  unsigned level;
  uint64_t serialization_quirks;
  unsigned int seed;

  level = 5;
  serialization_quirks = (RETRO_SERIALIZATION_QUIRK_ENDIAN_DEPENDENT |
                          RETRO_SERIALIZATION_QUIRK_PLATFORM_DEPENDENT);

  if(retro_environment_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE,&log))
    {
      opera_lr_callbacks_set_log_printf(log.log);
      opera_log_set_func(log.log);
    }

  retro_environment_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL,&level);
  retro_environment_cb(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS,&serialization_quirks);

  opera_cdrom_set_callbacks(cdimage_get_size,
                            cdimage_set_sector,
                            cdimage_read_sector);

  /*
   * Initialize PRNG with time-based seed.
   * Note: time(NULL) provides limited entropy and is predictable.
   * For better randomness, consider using platform-specific entropy sources.
   */
  seed = (unsigned int)time(NULL);
  srand(seed);
  prng16_seed(seed);
  prng32_seed(seed);
  
  retro_log_printf_cb(RETRO_LOG_DEBUG,
                      "[Opera]: initialized with seed %u\n", seed);
}

void
retro_deinit(void)
{
  retro_cdimage_multi_close(&CDIMAGE);
  game_info_path_free();
}

void
retro_reset(void)
{
  opera_lr_nvram_save(game_info_path_get(),
                      g_OPTS.nvram_shared,
                      g_OPTS.nvram_version);


  opera_3do_destroy();
  opera_lr_opts_reset();

  opera_lr_opts_process();
  opera_3do_init(libopera_callback);
  cdimage_set_sector(0);

  opera_lr_nvram_load(game_info_path_get(),
                      g_OPTS.nvram_shared,
                      g_OPTS.nvram_version);
}

static
bool
variable_updated()
{
  bool updated;

  updated = false;
  if(!retro_environment_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE,&updated))
    return false;
  return updated;
}

static
void
process_opts_if_updated()
{
  if(!variable_updated())
    return;

  opera_lr_opts_process();
}

static
void
draw_crosshairs_if_enabled()
{
  if(g_OPTS.hide_lightgun_crosshairs)
    return;

  lr_input_crosshairs_draw(g_OPTS.video_buffer,
                           g_OPTS.video_width,
                           g_OPTS.video_height);
}

void
retro_run(void)
{
  process_opts_if_updated();

  lr_input_update(g_OPTS.active_devices);

  opera_3do_process_frame();

  draw_crosshairs_if_enabled();

  opera_lr_dsp_upload();

  retro_video_refresh_cb(g_OPTS.video_buffer,
                         g_OPTS.video_width,
                         g_OPTS.video_height,
                         g_OPTS.video_width << g_OPTS.video_pitch_shift);
}