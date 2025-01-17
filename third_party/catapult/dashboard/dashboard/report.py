# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides the web interface for reporting a graph of traces."""

import json
import os

from google.appengine.ext import ndb

from dashboard import chart_handler
from dashboard import list_tests
from dashboard import short_uri
from dashboard import update_test_suites
from dashboard.models import page_state


class ReportHandler(chart_handler.ChartHandler):
  """URL endpoint for /report page."""

  def get(self):
    """Renders the UI for selecting graphs."""

    query_string = self._GetQueryStringForOldUri()
    if query_string:
      self.redirect('/report?' + query_string)
      return

    dev_version = ('Development' in os.environ['SERVER_SOFTWARE'] or
                   self.request.host == 'chrome-perf.googleplex.com')

    self.RenderHtml('report.html', {
        'dev_version': dev_version,
        'test_suites': json.dumps(update_test_suites.FetchCachedTestSuites()),
    })

  def _GetQueryStringForOldUri(self):
    """Gets a new query string if old URI parameters are present.

    SID is a hash string generated from a page state dictionary which is
    created here from old URI request parameters.

    Returns:
      A query string if request parameters are from old URI, otherwise None.
    """
    masters = self.request.get('masters')
    bots = self.request.get('bots')
    tests = self.request.get('tests')
    checked = self.request.get('checked')

    if not (masters and bots and tests):
      return None

    # Page state is a list of chart state.  Chart state is
    # a list of pair of test path and selected series which is used
    # to generate a chart on /report page.
    state = _CreatePageState(masters, bots, tests, checked)

    # Replace default separators to remove whitespace.
    state_json = json.dumps(state, separators=(',', ':'))
    state_id = short_uri.GenerateHash(state_json)

    # Save page state.
    if not ndb.Key(page_state.PageState, state_id).get():
      page_state.PageState(id=state_id, value=state_json).put()

    query_string = 'sid=' + state_id
    if self.request.get('start_rev'):
      query_string += '&start_rev=' + self.request.get('start_rev')
    if self.request.get('end_rev'):
      query_string += '&end_rev=' + self.request.get('end_rev')
    if self.request.get('rev'):
      query_string += '&rev=' + self.request.get('rev')
    return query_string


def _CreatePageState(masters, bots, tests, checked):
  """Creates a page state dictionary for old URI parameters.

  Based on original /report page, each combination of masters, bots, and
  tests is a chart; therefor we create a list of chart states for those
  combinations.

  Args:
    masters: A string with comma separated list of masters.
    bots: A string with comma separated list of bots.
    tests: A string with comma separated list of tests.
    checked: A string with comma separated list of checked series.

  Returns:
    Page state dictionary.
  """
  selected_series = []
  if checked:
    if checked == 'all':
      selected_series = ['all']
    else:
      selected_series = checked.split(',')

  masters = masters.split(',')
  bots = bots.split(',')
  tests = tests.split(',')
  test_paths = []
  for master in masters:
    for bot in bots:
      for test in tests:
        test_parts = test.split('/')
        if len(test_parts) == 1:
          first_test = _GetFirstTest(test, master + '/' + bot)
          if first_test:
            test += '/' + first_test
            if not selected_series:
              selected_series.append(first_test)
        test_paths.append(master + '/' + bot + '/' + test)

  chart_states = []
  for path in test_paths:
    chart_states.append([[path, selected_series]])

  return {
      'charts': chart_states
  }


def _GetFirstTest(test_suite, bot_path):
  """Gets the first test.

  Args:
    test_suite: Test suite name.
    bot_path: Master and bot name separated by a slash.

  Returns:
    The first test that has rows, otherwise returns None.
  """
  sub_test_tree = list_tests.GetSubTests(test_suite, [bot_path])
  test_parts = []
  while sub_test_tree:
    first_test = sorted(sub_test_tree.keys())[0]
    test_parts.append(first_test)
    if sub_test_tree[first_test]['has_rows']:
      return '/'.join(test_parts)
    sub_test_tree = sub_test_tree[first_test]['sub_tests']
  return None
