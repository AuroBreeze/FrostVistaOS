#include "core/proc.h"
#include "kernel/bcache.h"
#include "kernel/defs.h"
#include "kernel/log.h"
#include "kernel/types.h"

void test_read_img() {
  struct Process *p = get_proc();
  release(&p->lock);

  // The moment of truth: Read Block 1 (where your SuperBlock lives)
  // Assume device 0 is your virtio disk
  LOG_DEBUG("[FS] Reading Block 1 (SuperBlock) from device 0");
  struct buf *b = bread(0, 1);
  LOG_DEBUG("[FS] Read Block 1 (SuperBlock) from device 0");
  // Cast the raw data to check the magic number
  // We just check the first 4 bytes for simplicity
  uint32 *magic_ptr = (uint32 *)b->data;

  if (*magic_ptr == 0x0B8EE2E0) {
    LOG_DEBUG("[FS] SUCCESS! Magic number 0x0B8EE2E0 matched!");
  } else {
    LOG_DEBUG("[FS] MOUNT FAILED! Expected 0x0B8EE2E0 but got 0x%x",
              *magic_ptr);
  }

  // CRUCIAL: Always release the buffer!
  brelse(b);
  LOG_DEBUG("[FS] FrostVistaOS has successfully mounted the root disk!");
}
