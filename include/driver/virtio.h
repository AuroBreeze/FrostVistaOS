#ifndef __DRIVER_VIRTIO_H__
#define __DRIVER_VIRTIO_H__

#include "kernel/types.h"

#define VIRTQ_AVAIL_F_NO_INTERRUPT 1

/* This marks a buffer as continuing via the next field. */
#define VIRTQ_DESC_F_NEXT 1

/* This marks a buffer as device write-only (otherwise device read-only). */
#define VIRTQ_DESC_F_WRITE 2

/* This means the buffer contains a list of buffer descriptors. */
#define VIRTQ_DESC_F_INDIRECT 4
#define VIRTIO_RING_F_INDIRECT_DESC                                            \
	28 // Negotiating this feature indicates that the driver can use
	   // descriptors with the VIRTQ_DESC_F_INDIRECT flag set

// NOTE: The VIRTIO_F_RING_EVENT_IDX (bit 29) in the transport layer was not
// cleared, causing this bit to remain set to 1; as a result, the device assumes
// that the driver supports EventIdx interrupt suppression.
#define VIRTIO_RING_F_EVENT_IDX                                                \
	29 // This feature enables the used_event and the avail_event fields

#endif
