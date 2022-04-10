/******************************************************************************
 * Copyright (c) 2011 IBM Corporation
 * All rights reserved.
 * This program and the accompanying materials
 * are made available under the terms of the BSD License
 * which accompanies this distribution, and is available at
 * http://www.opensource.org/licenses/bsd-license.php
 *
 * Contributors:
 *     IBM Corporation - initial implementation
 *****************************************************************************/

#include <stdio.h>
#include <stdbool.h>
#include <cpu.h>
#include <helpers.h>
#include <byteorder.h>
#include "virtio.h"
#include "virtio-blk.h"
#include "virtio-internal.h"

#define DEFAULT_SECTOR_SIZE 512
//#define DRIVER_FEATURE_SUPPORT  (VIRTIO_BLK_F_BLK_SIZE | VIRTIO_F_VERSION_1)
#define DRIVER_FEATURE_SUPPORT  (VIRTIO_F_VERSION_1)

/**
 * Initialize virtio-block device.
 * @param  dev  pointer to virtio device information
 */
int
virtioblk_init(struct virtio_device *dev)
{
	struct vqs *vq;
	int blk_size = DEFAULT_SECTOR_SIZE;
	uint64_t features;
	int status = VIRTIO_STAT_ACKNOWLEDGE;

	/* Reset device */
	virtio_reset_device(dev);

	/* Acknowledge device. */
	virtio_set_status(dev, status);

	/* Tell HV that we know how to drive the device. */
	status |= VIRTIO_STAT_DRIVER;
	virtio_set_status(dev, status);

	if (dev->features & VIRTIO_F_VERSION_1) {
		/* Negotiate features and sets FEATURES_OK if successful */
		if (virtio_negotiate_guest_features(dev, DRIVER_FEATURE_SUPPORT))
			goto dev_error;

		virtio_get_status(dev, &status);
	} else {
		/* Device specific setup - we support F_BLK_SIZE */
		virtio_set_guest_features(dev,  VIRTIO_BLK_F_BLK_SIZE);
	}

	vq = virtio_queue_init_vq(dev, 0);
	if (!vq)
		goto dev_error;

	/* Tell HV that setup succeeded */
	status |= VIRTIO_STAT_DRIVER_OK;
	virtio_set_status(dev, status);

	features = virtio_get_host_features(dev);
	if (features & VIRTIO_BLK_F_BLK_SIZE) {
		blk_size = virtio_get_config(dev,
					     offset_of(struct virtio_blk_cfg, blk_size),
					     sizeof(blk_size));
	}

	return blk_size;
dev_error:
	printf("%s: failed\n", __func__);
	status |= VIRTIO_STAT_FAILED;
	virtio_set_status(dev, status);
	return 0;
}


/**
 * Shutdown the virtio-block device.
 * @param  dev  pointer to virtio device information
 */
void
virtioblk_shutdown(struct virtio_device *dev)
{
	/* Quiesce device */
	virtio_set_status(dev, VIRTIO_STAT_FAILED);

	/* Reset device */
	virtio_reset_device(dev);
}

static void fill_blk_hdr(struct virtio_blk_req *blkhdr, bool is_modern,
                         uint32_t type, uint32_t ioprio, uint32_t sector)
{
	if (is_modern) {
		blkhdr->type = cpu_to_le32(type);
		//blkhdr->ioprio = cpu_to_le32(ioprio);
		blkhdr->sector = cpu_to_le64(sector);
	} else {
		blkhdr->type = type;
		//blkhdr->ioprio = ioprio;
		blkhdr->sector = sector;
	}
}

/**
 * Read / write blocks
 * @param  reg  pointer to "reg" property
 * @param  buf  pointer to destination buffer
 * @param  blocknum  block number of the first block that should be transfered
 * @param  cnt  amount of blocks that should be transfered
 * @param  type  VIRTIO_BLK_T_OUT for write, VIRTIO_BLK_T_IN for read transfers
 * @return number of blocks that have been transfered successfully
 */
int
virtioblk_transfer(struct virtio_device *dev, struct virtio_blk_req_data *data, char *buf, uint64_t blocknum,
                   long cnt, unsigned int type)
{
	int id;
	uint64_t capacity;
	struct vqs *vq = &dev->vq[0];
	uint16_t avail_idx;
	int blk_size = DEFAULT_SECTOR_SIZE;

	/* Check whether request is within disk capacity */
	capacity = virtio_get_config(dev,
			offset_of(struct virtio_blk_cfg, capacity),
			sizeof(capacity));
	if (blocknum + cnt - 1 > capacity) {
		puts("virtioblk_transfer: Access beyond end of device!");
		return 0;
	}

	blk_size = virtio_get_config(dev,
			offset_of(struct virtio_blk_cfg, blk_size),
			sizeof(blk_size));
	if (blk_size % DEFAULT_SECTOR_SIZE) {
		fprintf(stderr, "virtio-blk: Unaligned sector size %d\n", blk_size);
		return 0;
	}
	avail_idx = virtio_modern16_to_cpu(dev, vq->avail->idx) % vq->size;

	/* Set up header */
	fill_blk_hdr(data->blkhdr, dev->features, type,
		     1, blocknum * blk_size / DEFAULT_SECTOR_SIZE);

	/* Determine descriptor index */
	id = (avail_idx * 3) % vq->size;

	/* Set up virtqueue descriptor for header */
	virtio_fill_desc(vq, id, dev->features,  (uint64_t)data->blkhdr_pa,
			 sizeof(struct virtio_blk_req),
			 VRING_DESC_F_NEXT, id + 1);

	/* Set up virtqueue descriptor for data */
	virtio_fill_desc(vq, id + 1, dev->features, (uint64_t)buf,
			 cnt * blk_size,
			 VRING_DESC_F_NEXT | ((type & 1) ? 0 : VRING_DESC_F_WRITE),
			 id + 2);

	/* Set up virtqueue descriptor for status */
	virtio_fill_desc(vq, id + 2, dev->features,
			 (uint64_t)data->status_pa, 1,
			 VRING_DESC_F_WRITE, 0);

	vq->avail->ring[avail_idx % vq->size] = virtio_cpu_to_modern16 (dev, id);
	mb();
	vq->avail->idx = virtio_cpu_to_modern16(dev, avail_idx + 1);

	/* Tell HV that the queue is ready */
	virtio_queue_notify(dev, 0);

	return 0;
}
