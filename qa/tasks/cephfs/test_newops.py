import logging
from tasks.cephfs.cephfs_test_case import CephFSTestCase

log = logging.getLogger(__name__)

class TestNewOps(CephFSTestCase):
    def test_newops_getvxattr(self):
        """
        For nautilus it will crash the MDSs when receive unknown OPs, as a workaround
        the clients should avoid sending them to nautilus
        """

        log.info("Test for unrecognized vxattr...")
        self.mount_a.run_shell(["mkdir", "newop_getvxattr_dir"])

        # ceph.dir.pin.random has been removed, expect ENODATA
        try:
            self.mount_a.getfattr("./newop_getvxattr_dir", "ceph.dir.pin.random")
        except CommandFailedError:
            pass
        log.info("Test for unrecognized vxattr succeeds")
