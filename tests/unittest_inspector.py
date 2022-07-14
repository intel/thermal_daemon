#! /usr/bin/env python3
# Copyright © 2020, Canonical Ltd
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
#       Marco Trevisan <marco.trevisan@canonical.com>

import argparse
import importlib.util
import inspect
import os
import unittest

def list_tests(module):
    tests = []
    for name, obj in inspect.getmembers(module):
        if inspect.isclass(obj) and issubclass(obj, unittest.TestCase):
            cases = unittest.defaultTestLoader.getTestCaseNames(obj)
            tests += [ (obj, '{}.{}'.format(name, t)) for t in cases ]
    return tests


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('unittest_source', type=argparse.FileType('r'))

    args = parser.parse_args()
    source_path = args.unittest_source.name
    spec = importlib.util.spec_from_file_location(
        os.path.basename(source_path), source_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    for machine, human in list_tests(module):
        print(human)
