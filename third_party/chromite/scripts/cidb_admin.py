# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for administering the Continuous Integration Database."""

from __future__ import print_function

import os

from chromite.lib import cidb
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging


MIGRATE = 'migrate'
WIPE = 'wipe'

COMMANDS = [MIGRATE, WIPE]


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  # Put options that control the mode of script into mutually exclusive group.

  parser.add_argument('command', action='store', choices=COMMANDS,
                      help='The action to execute.')
  parser.add_argument('cred_dir', action='store',
                      metavar='CIDB_CREDENTIALS_DIR',
                      help='Database credentials directory with certificates '
                           'and other connection information.')
  parser.add_argument('--migrate-version', action='store', default=None,
                      help='Maximum schema version to migrate to.')

  return parser


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)

  logging.getLogger('sqlalchemy.engine').setLevel(logging.INFO)

  if options.command == MIGRATE:
    positive_confirmation = 'please modify my database'
    warn = ('This option will apply schema changes to your existing database. '
            'You should not run this against the production database unless '
            'your changes are thoroughly tested, and those tests included '
            'in cidb_integration_test.py (including tests that old data is '
            'sanely migrated forward). Database corruption could otherwise '
            'result. Are you sure you want to proceed? If so, type "%s" '
            'now.\n') % positive_confirmation
  elif options.command == WIPE:
    positive_confirmation = 'please delete my data'
    warn = ('This operation will wipe (i.e. DELETE!) the entire contents of '
            'the database pointed at by %s. Are you sure you want to proceed? '
            'If so, type "%s" now.\n') % (
                os.path.join(options.cred_dir, 'host.txt'),
                positive_confirmation)
  else:
    print('No command or unsupported command. Exiting.')
    exit()

  print(warn)
  conf_string = cros_build_lib.GetInput('(%s)?: ' % positive_confirmation)
  if conf_string != positive_confirmation:
    print('You changed your mind. Aborting.')
    exit()

  if options.command == MIGRATE:
    print('OK, applying migrations...')
    db = cidb.CIDBConnection(options.cred_dir)
    db.ApplySchemaMigrations(maxVersion=options.migrate_version)
  elif options.command == WIPE:
    print('OK, wiping database...')
    db = cidb.CIDBConnection(options.cred_dir)
    db.DropDatabase()
    print('Done.')


