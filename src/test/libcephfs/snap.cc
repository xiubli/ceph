// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2021 Red Hat Inc.
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#include "include/rados/librados.hpp"
#include "include/encoding.h"
#include "include/err.h"
#include "include/scope_guard.h"
#include "test/librados/test_cxx.h"
#include "test/librados/testcase_cxx.h"
#include "include/compat.h"
#include "gtest/gtest.h"
#include "include/cephfs/libcephfs.h"
#include "mds/mdstypes.h"
#include "include/stat.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>

#include "gtest/gtest.h"

#include "include/rados.h"
#include "include/rados/librados.hpp"
#include "common/Clock.h"
#include "common/ceph_json.h"
#include "include/neorados/RADOS.hpp"
#include "include/rados/librados.hpp"
#include "common/ceph_mutex.h"
#include "common/hobject.h"
#include "librados/AioCompletionImpl.h"
#include "mon/error_code.h"
#include "osd/error_code.h"
#include "osd/osd_types.h"
#include "osdc/error_code.h"
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <functional>
#include <boost/system/system_error.hpp>
#include "include/rados/rados_types.hpp"

#ifdef __linux__
#include <limits.h>
#include <sys/xattr.h>
#endif

#include <fmt/format.h>
#include <map>
#include <vector>
#include <thread>
#include <regex>
#include <string>

#ifndef ALLPERMS
#define ALLPERMS (S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)
#endif

using namespace librados;
using namespace ceph;

int connect_cluster(Rados &cluster)
{
  char *id = getenv("CEPH_CLIENT_ID");
  if (id) std::cerr << "Client id is: " << id << std::endl;

  int ret;
  ret = cluster.init(id);
  if (ret) {
    std::cout << "cluster.init failed with error " << std::endl;
    return ret;
  }
  ret = cluster.conf_read_file(NULL);
  if (ret) {
    cluster.shutdown();
    std::cout << "cluster.conf_read_file failed with error " << std::endl;
    return ret;
  }
  cluster.conf_parse_env(NULL);

  ret = cluster.connect();
  if (ret) {
    cluster.shutdown();
    std::cout << "cluster.connect failed with error " << std::endl;
  }
  return ret;
}

TEST(LibCephFS, GetSnaps) {
  Rados cluster;
  IoCtx readioctx;

  ASSERT_EQ(0, connect_cluster(cluster));

  ASSERT_EQ(0, cluster.ioctx_create("cephfs.a.data", readioctx));

  snap_set_t ss;

  std::string oid = "10000000001.00000000";
  readioctx.snap_set_read(LIBRADOS_SNAP_DIR);
  ASSERT_EQ(0, readioctx.list_snaps(oid, &ss));
  std::cout << " oid: " << oid << std::endl;
  std::cout << "clons.size(): " << ss.clones.size() << "\n" << std::endl;
//  snap_t snapid = LIBRADOS_SNAP_DIR;
  for (auto& clone : ss.clones) {
    std::cout << "cloneid: " << clone.cloneid << " snaps: " << clone.snaps << " overlap: " << clone.overlap << " size: " << clone.size << "\n" << std::endl;
    for (auto &snapid: clone.snaps) {
      std::cout << " snapid: " << snapid << std::endl;
      readioctx.snap_set_read(snapid);
      std::map<std::string, bufferlist> attrs;
      ASSERT_EQ(0, readioctx.getxattrs(oid, attrs));
      for (auto &b : attrs) {
        std::string str(b.second.c_str(), b.second.length());
        std::cout << b.first << " " << str << std::endl;
      }
    }
  }
}
