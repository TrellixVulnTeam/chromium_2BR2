#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a simple script to run a java "binary".

This creates a script that sets up the java command line for running a java
jar. This includes correctly setting the classpath and the main class.
"""

import optparse
import os
import sys

from util import build_utils

# The java command must be executed in the current directory because there may
# be user-supplied paths in the args. The script receives the classpath relative
# to the directory that the script is written in and then, when run, must
# recalculate the paths relative to the current directory.
script_template = """\
#!/usr/bin/env python
#
# This file was generated by build/android/gyp/create_java_binary_script.py

import os
import sys

self_dir = os.path.dirname(__file__)
classpath = [{classpath}]
extra_java_args = {extra_java_args}
extra_program_args = {extra_program_args}
if os.getcwd() != self_dir:
  offset = os.path.relpath(self_dir, os.getcwd())
  classpath = [os.path.join(offset, p) for p in classpath]
java_cmd = ["java"]
java_cmd.extend(extra_java_args)
java_cmd.extend(
    ["-classpath", ":".join(classpath), "-enableassertions", \"{main_class}\"])
java_cmd.extend(extra_program_args)
java_cmd.extend(sys.argv[1:])
os.execvp("java", java_cmd)
"""

def main(argv):
  argv = build_utils.ExpandFileArgs(argv)
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)
  parser.add_option('--output', help='Output path for executable script.')
  parser.add_option('--jar-path', help='Path to the main jar.')
  parser.add_option('--main-class',
      help='Name of the java class with the "main" entry point.')
  parser.add_option('--classpath', action='append',
      help='Classpath for running the jar.')
  parser.add_option('--extra-java-args',
      help='Extra args passed to the "java" cmd')
  options, extra_program_args = parser.parse_args(argv)

  classpath = [options.jar_path]
  for cp_arg in options.classpath:
    classpath += build_utils.ParseGypList(cp_arg)

  if options.extra_java_args:
    extra_java_args = build_utils.ParseGypList(options.extra_java_args)
  else:
    extra_java_args = []

  run_dir = os.path.dirname(options.output)
  classpath = [os.path.relpath(p, run_dir) for p in classpath]

  with open(options.output, 'w') as script:
    script.write(script_template.format(
      extra_java_args=repr(extra_java_args),
      classpath=('"%s"' % '", "'.join(classpath)),
      main_class=options.main_class,
      extra_program_args=repr(extra_program_args)))

  os.chmod(options.output, 0750)

  if options.depfile:
    build_utils.WriteDepfile(
        options.depfile,
        build_utils.GetPythonDependencies())


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
