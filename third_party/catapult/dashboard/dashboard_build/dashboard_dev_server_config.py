# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys

import dashboard_project

import webapp2
from webapp2 import Route


def _RelPathToUnixPath(p):
  return p.replace(os.sep, '/')


class TestListHandler(webapp2.RequestHandler):
  def get(self, *args, **kwargs):  # pylint: disable=unused-argument
    project = dashboard_project.DashboardProject()
    test_relpaths = ['/' + _RelPathToUnixPath(x)
                     for x in project.FindAllTestModuleRelPaths()]

    tests = {'test_relpaths': test_relpaths}
    tests_as_json = json.dumps(tests)
    self.response.content_type = 'application/json'
    return self.response.write(tests_as_json)


class DashboardDevServerConfig(object):
  def __init__(self):
    self.project = dashboard_project.DashboardProject()

  def GetName(self):
    return 'dashboard'

  def GetRunUnitTestsUrl(self):
    return '/dashboard/tests.html'

  def AddOptionstToArgParseGroup(self, g):
    g.add_argument('--dashboard-data-dir',
                   default=self.project.dashboard_test_data_path)

  def GetRoutes(self, args):  # pylint: disable=unused-argument
    return [
        Route('/dashboard/tests', TestListHandler),
    ]

  def GetSourcePaths(self, args):  # pylint: disable=unused-argument
    return list(self.project.source_paths)

  def GetTestDataPaths(self, args):  # pylint: disable=unused-argument
    return [
        ('/dashboard/test_data/', args.data_dir),
    ]
