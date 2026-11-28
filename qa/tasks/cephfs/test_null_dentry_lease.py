"""
Leases on null dentries (mds_null_dentry_lease_duration).

A client that looked up a missing name answers later lookups of that name
itself while it holds Fs on the parent directory, or while it holds a
lease on the null dentry.  The MDS only gives the lease when the client
does not hold Fs at the time of the lookup.  That is the case while
another kernel client creates and unlinks files in the directory with
async dirops: it holds Fx on the directory then.  These tests run such a
writer on mount_b, so mount_a depends on the lease alone.
"""

import contextlib
import logging
import threading
import time

from tasks.cephfs.cephfs_test_case import CephFSTestCase
from tasks.cephfs.kernel_mount import KernelMount

log = logging.getLogger(__name__)

# client_lease_durations[1]: what every dentry lease lasts by default
DEFAULT_LEASE = 30


class TestNullDentryLease(CephFSTestCase):
    CLIENTS_REQUIRED = 2
    MDSS_REQUIRED = 1

    NAMES = 20
    DIR = "probe"

    def setUp(self):
        super().setUp()
        if not isinstance(self.mount_b, KernelMount):
            # a fuse client neither takes Fx on a directory, nor keeps
            # it from the other clients for long
            self.skipTest("the writer needs a kernel client (async dirops)")
        # made by mount_b: mount_a must not know the directory's full
        # contents, or it answers ENOENT itself without any lease
        self.mount_b.run_shell(["mkdir", self.DIR])
        self.mount_b.run_shell(["touch"] + ["%s/exists-%d" % (self.DIR, i) for i in range(5)])
        self.mount_a.stat(self.DIR)

    def tearDown(self):
        self.config_rm("mds", "mds_null_dentry_lease_duration")
        super().tearDown()

    def _set_duration(self, secs):
        self.config_set("mds", "mds_null_dentry_lease_duration", secs)

    def _lookups(self):
        perf = self.fs.rank_asok(["perf", "dump", "mds_server"])
        return perf["mds_server"]["req_lookup_latency"]["avgcount"]

    def _names(self, prefix="missing", n=None):
        return ["%s-%d" % (prefix, i) for i in range(n or self.NAMES)]

    def _probe(self, names):
        """stat the missing names on mount_a, return how many lookups the MDS served"""
        before = self._lookups()
        out = self.mount_a.run_python(f"""
import errno, os
enoent = 0
for n in {names!r}:
    try:
        os.stat(os.path.join("{self.mount_a.mountpoint}", "{self.DIR}", n))
    except OSError as e:
        assert e.errno == errno.ENOENT, e
        enoent += 1
print(enoent)
""")
        self.assertEqual(int(out.strip()), len(names))
        n = self._lookups() - before
        log.info("probe of %d missing names: %d lookups", len(names), n)
        return n

    @contextlib.contextmanager
    def _writer(self, secs):
        """
        For secs seconds, mount_b creates and unlinks files in the directory,
        so mount_a does not get Fs on it.  mount_b knows the whole directory
        and holds Fx on it, so it sends no lookups, which would be counted
        with mount_a's.
        """
        path = "%s/%s" % (self.mount_b.mountpoint, self.DIR)
        t = threading.Thread(target=self.mount_b.run_python, daemon=True, args=(f"""
import os, time
end = time.monotonic() + {secs}
k = 0
while time.monotonic() < end:
    p = os.path.join("{path}", "tmp-%d" % k)
    os.close(os.open(p, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644))
    os.unlink(p)
    k += 1
    time.sleep(0.2)
""",))
        t.start()
        try:
            time.sleep(3)
            yield
        finally:
            t.join()

    def _num_leases(self, mount):
        gid = int(mount.get_global_id())
        for s in self.fs.rank_tell(["session", "ls"]):
            if s["id"] == gid:
                return s["num_leases"]
        raise RuntimeError("no session for client.%d" % gid)

    def _probe_twice(self, wait, names=None):
        """probe, wait, probe again, with the writer running; lookups of both probes"""
        names = names or self._names()
        with self._writer(wait + 10):
            first = self._probe(names)
            time.sleep(wait)
            again = self._probe(names)
        self.assertGreaterEqual(first, len(names))
        return first, again

    def test_default_lease_expires(self):
        """
        Without the option a null dentry lease lasts 30 s: 30 s later every
        name is looked up again.
        """
        _, again = self._probe_twice(DEFAULT_LEASE + 5)
        self.assertGreaterEqual(again, self.NAMES)

    def test_long_lease_saves_lookups(self):
        """
        With a long null dentry lease, the client keeps answering for the
        missing names itself after 30 s.
        """
        self._set_duration(300)
        _, again = self._probe_twice(DEFAULT_LEASE + 5)
        self.assertLessEqual(again, self.NAMES // 4)

    def test_long_lease_expires(self):
        """
        The null dentry lease ends after the configured time: the names are
        looked up again, and the MDS drops the leases from the session.
        """
        dur = 10
        self._set_duration(dur)
        with self._writer(dur + 25):
            self._probe(self._names())
            leases = self._num_leases(self.mount_a)
            self.assertGreaterEqual(leases, self.NAMES)
            # trim_client_leases() runs from the MDS tick (5 s)
            self.wait_until_true(lambda: self._num_leases(self.mount_a) <= leases - self.NAMES,
                                 timeout=dur + 15)
            self.assertGreaterEqual(self._probe(self._names()), self.NAMES)

    def test_create_revokes_long_lease(self):
        """
        Creating a name revokes the long null dentry lease other clients hold
        on it: they see the new file at once.
        """
        self._set_duration(300)
        names = self._names()
        with self._writer(15):
            self._probe(names)
            for n in names[:5]:
                self.mount_b.run_shell(["touch", "%s/%s" % (self.DIR, n)])
                # no retry: the create waits for the lease to be revoked
                self.mount_a.stat("%s/%s" % (self.DIR, n))
            # the other names are still answered from their leases
            self.assertLessEqual(self._probe(names[5:]), len(names[5:]) // 4)

    def test_lease_duration_runtime_change(self):
        """
        Setting the option back to 0 at runtime gives the default 30 s leases
        again for new lookups.
        """
        self._set_duration(300)
        with self._writer(6):
            self._probe(self._names("other"))
        self._set_duration(0)
        _, again = self._probe_twice(DEFAULT_LEASE + 5)
        self.assertGreaterEqual(again, self.NAMES)

    def test_long_lease_renewed(self):
        """
        A kernel client renews a lease that it uses in the second half of its
        time; the renewed lease has the long duration again.
        """
        if not isinstance(self.mount_a, KernelMount):
            self.skipTest("only the kernel client renews dentry leases")
        dur = 12
        self._set_duration(dur)
        names = self._names()
        lookups = 0
        with self._writer(dur * 3 + 10):
            self.assertGreaterEqual(self._probe(names), self.NAMES)
            # use the names at 2/3 of the lease, for 3 lease lengths
            for _ in range(4):
                time.sleep(dur * 2 / 3)
                lookups += self._probe(names)
        self.assertLessEqual(lookups, self.NAMES // 2)
