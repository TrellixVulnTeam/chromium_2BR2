# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import cProfile
import pstats
import StringIO
import inspect
import sys
import os

import tracing_project
from py_vulcanize import generate
from py_vulcanize import project


class Bench(object):

  def SetUp(self):
    pass

  def Run(self):
    pass

  def TearDown(self):
    pass


def Main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--repeat-count', type=int, default=10)
  parser.add_argument('bench_name')
  args = parser.parse_args(args)

  benches = [g for g in globals().values()
             if g != Bench and inspect.isclass(g) and
             Bench in inspect.getmro(g)]

  # pylint: disable=undefined-loop-variable
  b = [b for b in benches if b.__name__ == args.bench_name]
  if len(b) != 1:
    sys.stderr.write('Bench %r not found.' % args.bench_name)
    return 1

  bench = b[0]()
  bench.SetUp()
  try:
    pr = cProfile.Profile()
    pr.enable(builtins=False)
    for _ in range(args.repeat_count):
      bench.Run()
    pr.disable()
    s = StringIO.StringIO()

    sortby = 'cumulative'
    ps = pstats.Stats(pr, stream=s).sort_stats(sortby)
    ps.print_stats()
    print s.getvalue()
    return 0
  finally:
    bench.TearDown()


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
