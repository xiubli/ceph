// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2011 New Dream Network
 * Copyright (C) 2016 Red Hat
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#include "gtest/gtest.h"
#include "include/cephfs/libcephfs.h"

static int update_root_mode()
{
  struct ceph_mount_info *admin;
  std::cout << __func__ << " " << __LINE__ << std::endl;
  int r = ceph_create(&admin, NULL);
  std::cout << __func__ << " " << __LINE__ << std::endl;
  if (r < 0)
    return r;
  std::cout << __func__ << " " << __LINE__ << std::endl;
  ceph_conf_read_file(admin, NULL);
  std::cout << __func__ << " " << __LINE__ << std::endl;
  ceph_conf_parse_env(admin, NULL);
  std::cout << __func__ << " " << __LINE__ << std::endl;
  ceph_conf_set(admin, "client_permissions", "false");
  std::cout << __func__ << " " << __LINE__ << std::endl;
  r = ceph_mount(admin, "/");
  std::cout << __func__ << " " << __LINE__ << std::endl;
  if (r < 0)
    goto out;
  std::cout << __func__ << " " << __LINE__ << std::endl;
  r = ceph_chmod(admin, "/", 01777);
  std::cout << __func__ << " " << __LINE__ << std::endl;
out:
  ceph_shutdown(admin);
  return r;
}


int main(int argc, char **argv)
{
  std::cout << __func__ << " " << __LINE__ << std::endl;
  int r = update_root_mode();
  if (r < 0)
    exit(1);

  std::cout << __func__ << " " << __LINE__ << std::endl;
  ::testing::InitGoogleTest(&argc, argv);

  std::cout << __func__ << " " << __LINE__ << std::endl;
  srand(getpid());

  std::cout << __func__ << " " << __LINE__ << std::endl;
  return RUN_ALL_TESTS();
}
