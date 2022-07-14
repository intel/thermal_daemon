#! /usr/bin/env python3
# Copyright © 2020, RedHat Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library. If not, see <http://www.gnu.org/licenses/>.
# Authors:
#       Benjamin Berg <bberg@redhat.com>

import os
import sys
import fcntl
import io
import re
import time
import threading
import select
import errno

class OutputChecker(object):

    def __init__(self, out=sys.stdout):
        self._output = out
        self._pipe_fd_r, self._pipe_fd_w = os.pipe()
        self._partial_buf = b''
        self._lines_sem = threading.Semaphore()
        self._lines = []
        self._reader_io = io.StringIO()

        # Just to be sure, shouldn't be a problem even if we didn't set it
        fcntl.fcntl(self._pipe_fd_r, fcntl.F_SETFL,
                    fcntl.fcntl(self._pipe_fd_r, fcntl.F_GETFL) | os.O_CLOEXEC | os.O_NONBLOCK)
        fcntl.fcntl(self._pipe_fd_w, fcntl.F_SETFL,
                    fcntl.fcntl(self._pipe_fd_w, fcntl.F_GETFL) | os.O_CLOEXEC)

        # Start copier thread
        self._thread = threading.Thread(target=self._copy, daemon=True)
        self._thread.start()

    def _copy(self):
        p = select.poll()
        p.register(self._pipe_fd_r)
        while True:
            try:
                # Be lazy and wake up occasionally in case _pipe_fd_r became invalid
                # The reason to do this is because os.read() will *not* return if the
                # FD is forcefully closed.
                p.poll(0.1)

                r = os.read(self._pipe_fd_r, 1024)
                if not r:
                    os.close(self._pipe_fd_r)
                    self._pipe_fd_r = -1
                    self._lines_sem.release()
                    return
            except OSError as e:
                if e.errno == errno.EWOULDBLOCK:
                    continue

                # We get a bad file descriptor error when the outside closes the FD
                if self._pipe_fd_r >= 0:
                    os.close(self._pipe_fd_r)
                self._pipe_fd_r = -1
                self._lines_sem.release()
                return

            l = r.split(b'\n')
            l[0] = self._partial_buf + l[0]
            self._lines.extend(l[:-1])
            self._partial_buf = l[-1]

            self._lines_sem.release()

            os.write(self._output.fileno(), r)

    def check_line_re(self, needle_re, timeout=0, failmsg=None):
        deadline = time.time() + timeout

        if isinstance(needle_re, str):
            needle_re = needle_re.encode('ascii')

        r = re.compile(needle_re)
        ret = []

        while True:
            try:
                l = self._lines.pop(0)
            except IndexError:
                # EOF, throw error
                if self._pipe_fd_r == -1:
                    if failmsg:
                        raise AssertionError("No further messages: " % failmsg) from None
                    else:
                        raise AssertionError('No client waiting for needle %s' % (str(needle_re))) from None

                # Check if should wake up
                if not self._lines_sem.acquire(timeout = deadline - time.time()):
                    if failmsg:
                        raise AssertionError(failmsg) from None
                    else:
                        raise AssertionError('Timed out waiting for needle %s (timeout: %0.2f)' % (str(needle_re), timeout)) from None
                continue

            ret.append(l)
            if r.search(l):
                return ret

    def check_line(self, needle, timeout=0, failmsg=None):
        if isinstance(needle, str):
            needle = needle.encode('ascii')

        needle_re = re.escape(needle)

        return self.check_line_re(needle_re, timeout=timeout, failmsg=failmsg)

    def check_no_line_re(self, needle_re, wait=0, failmsg=None):
        deadline = time.time() + wait

        if isinstance(needle_re, str):
            needle_re = needle_re.encode('ascii')

        r = re.compile(needle_re)
        ret = []

        while True:
            try:
                l = self._lines.pop(0)
            except IndexError:
                # EOF, so everything good
                if self._pipe_fd_r == -1:
                    break

                # Check if should wake up
                if not self._lines_sem.acquire(timeout = deadline - time.time()):
                    # Timed out, so everything is good
                    break
                continue

            ret.append(l)
            if r.search(l):
                if failmsg:
                    raise AssertionError(failmsg)
                else:
                    raise AssertionError('Found needle %s but shouldn\'t have been there (timeout: %0.2f)' % (str(needle_re), wait))

        return ret

    def check_no_line(self, needle, wait=0, failmsg=None):
        if isinstance(needle, str):
            needle = needle.encode('ascii')

        needle_re = re.escape(needle)

        return self.check_no_line_re(needle_re, wait=wait, failmsg=failmsg)

    def clear(self):
        ret = self._lines
        self._lines = []
        return ret

    def assert_closed(self, timeout=1):
        self._thread.join(timeout)

        if self._thread.is_alive() != False:
            raise AssertionError("OutputCheck: Write side has not been closed yet!")

    def force_close(self):

        fd = self._pipe_fd_r
        self._pipe_fd_r = -1
        if fd >= 0:
            os.close(fd)

        self._thread.join()

    @property
    def fd(self):
        return self._pipe_fd_w

    def writer_attached(self):
        os.close(self._pipe_fd_w)
        self._pipe_fd_w = -1

    def __del__(self):
        if self._pipe_fd_r >= 0:
            os.close(self._pipe_fd_r)
            self._pipe_fd_r = -1
        if self._pipe_fd_w >= 0:
            os.close(self._pipe_fd_w)
            self._pipe_fd_w = -1
