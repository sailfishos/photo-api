/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PHOTO_CONTROL_TYPES_H
#define PHOTO_CONTROL_TYPES_H

#include "generated/ctl-ids.h"

typedef enum _PhotoControlType {
    PHOTO_CONTROL_TYPE_INVALID,
    PHOTO_CONTROL_TYPE_AREAS,
    PHOTO_CONTROL_TYPE_FLOAT,
    PHOTO_CONTROL_TYPE_ENUM,
    PHOTO_CONTROL_TYPE_INT,
} PhotoControlType;

typedef enum _PhotoControlLockId {
    PHOTO_CONTROL_LOCK_ID_AE,
    PHOTO_CONTROL_LOCK_ID_AF,
    PHOTO_CONTROL_LOCK_ID_AWB,
    PHOTO_CONTROL_LOCK_ID_NUM
} PhotoControlLockId;

#define PHOTO_CONTROL_LOCK_ALL 0xffffffff
typedef unsigned int PhotoControlLockMask;
typedef enum _PhotoControlLockBit {
    PHOTO_CONTROL_LOCK_BIT_AE   = (1 << PHOTO_CONTROL_LOCK_ID_AE),
    PHOTO_CONTROL_LOCK_BIT_AF   = (1 << PHOTO_CONTROL_LOCK_ID_AF),
    PHOTO_CONTROL_LOCK_BIT_AWB  = (1 << PHOTO_CONTROL_LOCK_ID_AWB),
} PhotoControlLockBit;

typedef enum _PhotoControlLockState {
    PHOTO_CONTROL_STATE_DISABLED,
    PHOTO_CONTROL_STATE_UNLOCKED,
    PHOTO_CONTROL_STATE_PENDING,
    PHOTO_CONTROL_STATE_SEARCHING,
    PHOTO_CONTROL_STATE_LOCKED,
    PHOTO_CONTROL_STATE_FORCE_LOCKING,
    PHOTO_CONTROL_STATE_FORCE_LOCKED,
    PHOTO_CONTROL_STATE_UNLOCKING,
} PhotoControlLockState;

typedef struct _PhotoControlMeteringRect {
    float left;
    float top;
    float right;
    float bottom;
    int weight;
} PhotoControlMeteringRect;

typedef struct _PhotoControlFloatRange {
    float min;
    float max;
    float step;
} PhotoControlFloatRange;

typedef struct _PhotoMetadataRequest {
    PhotoControlId id;
    union {
        float *f;
        unsigned int *e;
        int *i32;
        void *d;
    } dest;
    bool out_valid;
} PhotoMetadataRequest;

#define PHOTO_BITS_PER_LONG             (sizeof(long) * 8)
#define PHOTO_BITMASK_SIZE(num_bits)    ((num_bits + PHOTO_BITS_PER_LONG - 1) / PHOTO_BITS_PER_LONG)
typedef struct _PhotoBitMask {
    unsigned long data;
} PhotoBitMask;

static inline bool photo_check_bit(const PhotoBitMask *mask, unsigned int idx)
{
    unsigned int word = idx / (sizeof(long) * 8);
    unsigned int bit = idx % (sizeof(long) * 8);

    return (mask[word].data >> bit) & 1;
}

static inline void photo_set_bit(PhotoBitMask *mask, unsigned int idx)
{
    unsigned int word = idx / (sizeof(long) * 8);
    unsigned int bit = idx % (sizeof(long) * 8);

    mask[word].data |= (1UL << bit);
}

static inline void photo_clear_bit(PhotoBitMask *mask, unsigned int idx)
{
    unsigned int word = idx / (sizeof(long) * 8);
    unsigned int bit = idx % (sizeof(long) * 8);

    mask[word].data &= ~(1UL << bit);
}

#endif
