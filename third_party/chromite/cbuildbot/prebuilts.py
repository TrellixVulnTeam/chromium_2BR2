# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cbuildbot logic for uploading prebuilts and managing binhosts."""

from __future__ import print_function

import glob
import os

from chromite.cbuildbot import commands
from chromite.cbuildbot import config_lib
from chromite.cbuildbot import constants
from chromite.lib import cros_logging as logging
from chromite.lib import portage_util

_PREFLIGHT_BINHOST = 'PREFLIGHT_BINHOST'
_CHROME_BINHOST = 'CHROME_BINHOST'
_FULL_BINHOST = 'FULL_BINHOST'
_BINHOST_PACKAGE_FILE = ('/usr/share/dev-install/portage/make.profile/'
                         'package.installable')
_PRIVATE_BINHOST_CONF_DIR = ('src/private-overlays/chromeos-partner-overlay/'
                             'chromeos/binhost')
_PUBLIC_BINHOST_CONF_DIR = 'src/third_party/chromiumos-overlay/chromeos/binhost'


def _AddPackagesForPrebuilt(filename):
  """Add list of packages for upload.

  Process a file that lists all the packages that can be uploaded to the
  package prebuilt bucket and generates the command line args for
  upload_prebuilts.

  Args:
    filename: file with the package full name (category/name-version), one
              package per line.

  Returns:
    A list of parameters for upload_prebuilts. For example:
    ['--packages=net-misc/dhcp', '--packages=app-admin/eselect-python']
  """
  try:
    cmd = []
    with open(filename) as f:
      # Get only the package name and category as that is what upload_prebuilts
      # matches on.
      for line in f:
        atom = line.split('#', 1)[0].strip()
        try:
          cpv = portage_util.SplitCPV(atom)
        except ValueError:
          logging.warning('Could not split atom %r (line: %r)', atom, line)
          continue
        if cpv:
          cmd.extend(['--packages=%s/%s' % (cpv.category, cpv.package)])
    return cmd
  except IOError as e:
    logging.warning('Problem with package file %s' % filename)
    logging.warning('Skipping uploading of prebuilts.')
    logging.warning('ERROR(%d): %s' % (e.errno, e.strerror))
    return None


def UploadPrebuilts(category, chrome_rev, private_bucket, buildroot,
                    version=None, **kwargs):
  """Upload Prebuilts for non-dev-installer use cases.

  Args:
    category: Build type. Can be [binary|full|chrome|chroot|paladin].
    chrome_rev: Chrome_rev of type constants.VALID_CHROME_REVISIONS.
    private_bucket: True if we are uploading to a private bucket.
    buildroot: The root directory where the build occurs.
    version: Specific version to set.
    board: Board type that was built on this machine.
    extra_args: Extra args to pass to prebuilts script.
  """
  extra_args = ['--prepend-version', category]
  extra_args.extend(['--upload', 'gs://chromeos-prebuilt'])
  if private_bucket:
    extra_args.extend(['--private', '--binhost-conf-dir',
                       _PRIVATE_BINHOST_CONF_DIR])
  else:
    extra_args.extend(['--binhost-conf-dir', _PUBLIC_BINHOST_CONF_DIR])

  if version is not None:
    extra_args.extend(['--set-version', version])

  if category == constants.CHROOT_BUILDER_TYPE:
    extra_args.extend(['--sync-host',
                       '--upload-board-tarball'])
    tarball_location = os.path.join(buildroot, 'built-sdk.tar.xz')
    extra_args.extend(['--prepackaged-tarball', tarball_location])

    # Remaining artifacts get uploaded into <year>/<month>/ subdirs so we don't
    # start dumping even more stuff into the top level. Also, the following
    # code handles any tarball suffix (.tar.*). For each of the artifact types
    # below, we also generate a single upload path template to be filled by the
    # uploading script. This has placeholders for the version (substituted
    # first) and another qualifier (either board or target, substituted second
    # and therefore uses a quoted %% modifier).
    # TODO(garnold) Using a mix of quoted/unquoted template variables is
    # confusing and error-prone, we should get rid of it.
    # TODO(garnold) Be specific about matching file suffixes, like making sure
    # there's nothing past the compression suffix (for example, .tar.xz.log).
    subdir_prefix = os.path.join(*version.split('.')[0:2])

    # Find toolchain overlay tarballs of the form
    # built-sdk-overlay-toolchains-<toolchains_spec>.tar.* and create an upload
    # specification for each of them. The upload path template has the form
    # cros-sdk-overlay-toolchains-<toolchain_spec>-<version>.tar.*.
    toolchains_overlay_prefix = 'built-sdk-overlay-toolchains-'
    for tarball in glob.glob(os.path.join(
        buildroot, constants.DEFAULT_CHROOT_DIR,
        constants.SDK_OVERLAYS_OUTPUT, toolchains_overlay_prefix + '*.tar.*')):
      tarball_name, tarball_suffix = os.path.basename(tarball).split('.', 1)

      # Only add the upload path arg when processing the first tarball.
      if '--toolchains-overlay-upload-path' not in extra_args:
        subdir = os.path.join(
            subdir_prefix,
            'cros-sdk-overlay-toolchains-%%(toolchains)s-%(version)s.' +
            tarball_suffix)
        extra_args.extend(['--toolchains-overlay-upload-path', subdir])

      toolchains = tarball_name[len(toolchains_overlay_prefix):]
      extra_args.extend(['--toolchains-overlay-tarball',
                         '%s:%s' % (toolchains, tarball)])

    # Find toolchain package tarballs of the form <target>.tar.* and create an
    # upload specificion for each fo them. The upload path template has the
    # form <target>-<version>.tar.*.
    for tarball in glob.glob(os.path.join(
        buildroot, constants.DEFAULT_CHROOT_DIR,
        constants.SDK_TOOLCHAINS_OUTPUT, '*.tar.*')):
      tarball_target, tarball_suffix = os.path.basename(tarball).split('.', 1)

      # Only add the path arg when processing the first tarball.  We do
      # this to get access to the tarball suffix dynamically (so it can
      # change and this code will still work).
      if '--toolchain-upload-path' not in extra_args:
        subdir = os.path.join(subdir_prefix,
                              '%%(target)s-%(version)s.' + tarball_suffix)
        extra_args.extend(['--toolchain-upload-path', subdir])

      extra_args.extend(['--toolchain-tarball',
                         '%s:%s' % (tarball_target, tarball)])

  if category == constants.CHROME_PFQ_TYPE:
    assert chrome_rev
    key = '%s_%s' % (chrome_rev, _CHROME_BINHOST)
    extra_args.extend(['--key', key.upper()])
  elif config_lib.IsPFQType(category):
    extra_args.extend(['--key', _PREFLIGHT_BINHOST])
  else:
    assert category in (constants.BUILD_FROM_SOURCE_TYPE,
                        constants.CHROOT_BUILDER_TYPE)
    extra_args.extend(['--key', _FULL_BINHOST])

  if category == constants.CHROME_PFQ_TYPE:
    extra_args += ['--packages=%s' % x
                   for x in ([constants.CHROME_PN] +
                             constants.OTHER_CHROME_PACKAGES)]

  kwargs.setdefault('extra_args', []).extend(extra_args)
  return _UploadPrebuilts(buildroot=buildroot, **kwargs)


class PackageFileMissing(Exception):
  """Raised when the dev installer package file is missing."""


def UploadDevInstallerPrebuilts(binhost_bucket, binhost_key, binhost_base_url,
                                buildroot, board, **kwargs):
  """Upload Prebuilts for dev-installer use case.

  Args:
    binhost_bucket: bucket for uploading prebuilt packages. If it equals None
                    then the default bucket is used.
    binhost_key: key parameter to pass onto upload_prebuilts. If it equals
                 None, then chrome_rev is used to select a default key.
    binhost_base_url: base url for upload_prebuilts. If None the parameter
                      --binhost-base-url is absent.
    buildroot: The root directory where the build occurs.
    board: Board type that was built on this machine.
    extra_args: Extra args to pass to prebuilts script.
  """
  extra_args = ['--prepend-version', constants.CANARY_TYPE]
  extra_args.extend(['--binhost-base-url', binhost_base_url])
  extra_args.extend(['--upload', binhost_bucket])
  extra_args.extend(['--key', binhost_key])

  filename = os.path.join(buildroot, 'chroot', 'build', board,
                          _BINHOST_PACKAGE_FILE.lstrip('/'))
  cmd_packages = _AddPackagesForPrebuilt(filename)
  if cmd_packages:
    extra_args.extend(cmd_packages)
  else:
    raise PackageFileMissing()

  kwargs.setdefault('extra_args', []).extend(extra_args)
  return _UploadPrebuilts(buildroot=buildroot, board=board, **kwargs)


def _UploadPrebuilts(buildroot, board, extra_args):
  """Upload prebuilts.

  Args:
    buildroot: The root directory where the build occurs.
    board: Board type that was built on this machine.
    extra_args: Extra args to pass to prebuilts script.
  """
  cmd = ['upload_prebuilts', '--build-path', buildroot]
  if board:
    cmd.extend(['--board', board])
  cmd.extend(extra_args)
  commands.RunBuildScript(buildroot, cmd, chromite_cmd=True)
