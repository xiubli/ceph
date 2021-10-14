// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *

 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#ifndef CEPHFS_FSCRYPT_H
#define CEPHFS_FSCRYPT_H

struct ceph_fscrypt_last_block_header {
        __u8  ver;
        __u8  compat;
       /* length of sizeof(file_offset + block_size + BLOCK SIZE) */
        uint32_t data_len;

        uint64_t objver;
        uint64_t file_offset;
        uint32_t block_size;
};

#endif
