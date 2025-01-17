# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import mock
import webapp2
import webtest

from dashboard import alerts
from dashboard import testing_common
from dashboard import utils
from dashboard.models import anomaly
from dashboard.models import bug_data
from dashboard.models import graph_data
from dashboard.models import sheriff
from dashboard.models import stoppage_alert


class AlertsTest(testing_common.TestCase):

  # TODO(qyearsley): Simplify this unit test.

  def setUp(self):
    super(AlertsTest, self).setUp()
    app = webapp2.WSGIApplication([('/alerts', alerts.AlertsHandler)])
    self.testapp = webtest.TestApp(app)

  def _AddAlertsToDataStore(self):
    """Adds sample data, including triaged and non-triaged alerts."""
    key_map = {}

    sheriff_key = sheriff.Sheriff(
        id='Chromium Perf Sheriff', email='sullivan@google.com').put()
    testing_common.AddTests(['ChromiumGPU'], ['linux-release'], {
        'scrolling-benchmark': {
            'first_paint': {},
            'mean_frame_time': {},
        }
    })
    first_paint = utils.TestKey(
        'ChromiumGPU/linux-release/scrolling-benchmark/first_paint')
    mean_frame_time = utils.TestKey(
        'ChromiumGPU/linux-release/scrolling-benchmark/mean_frame_time')

    # By default, all Test entities have an improvement_direction of UNKNOWN,
    # meaning that neither direction is considered an improvement.
    # Here we set the improvement direction so that some anomalies are
    # considered improvements.
    for test_key in [first_paint, mean_frame_time]:
      test = test_key.get()
      test.improvement_direction = anomaly.DOWN
      test.put()

    # Add some (12) non-triaged alerts.
    for end_rev in range(10000, 10120, 10):
      test_key = first_paint if end_rev % 20 == 0 else mean_frame_time
      anomaly_entity = anomaly.Anomaly(
          start_revision=end_rev - 5, end_revision=end_rev, test=test_key,
          median_before_anomaly=100, median_after_anomaly=200,
          sheriff=sheriff_key)
      anomaly_entity.SetIsImprovement()
      anomaly_key = anomaly_entity.put()
      key_map[end_rev] = anomaly_key.urlsafe()

    # Add some (2) already-triaged alerts.
    for end_rev in range(10120, 10140, 10):
      test_key = first_paint if end_rev % 20 == 0 else mean_frame_time
      bug_id = -1 if end_rev % 20 == 0 else 12345
      anomaly_entity = anomaly.Anomaly(
          start_revision=end_rev - 5, end_revision=end_rev, test=test_key,
          median_before_anomaly=100, median_after_anomaly=200,
          bug_id=bug_id, sheriff=sheriff_key)
      anomaly_entity.SetIsImprovement()
      anomaly_key = anomaly_entity.put()
      key_map[end_rev] = anomaly_key.urlsafe()
      if bug_id > 0:
        bug_data.Bug(id=bug_id).put()

    # Add some (6) non-triaged improvements.
    for end_rev in range(10140, 10200, 10):
      test_key = mean_frame_time
      anomaly_entity = anomaly.Anomaly(
          start_revision=end_rev - 5, end_revision=end_rev, test=test_key,
          median_before_anomaly=200, median_after_anomaly=100,
          sheriff=sheriff_key)
      anomaly_entity.SetIsImprovement()
      anomaly_key = anomaly_entity.put()
      self.assertTrue(anomaly_entity.is_improvement)
      key_map[end_rev] = anomaly_key.urlsafe()

    return key_map

  def testGet_NoParametersSet_UntriagedAlertsListed(self):
    key_map = self._AddAlertsToDataStore()
    response = self.testapp.get('/alerts')
    anomaly_list = self.GetEmbeddedVariable(response, 'ANOMALY_LIST')
    self.assertEqual(12, len(anomaly_list))
    # The test below depends on the order of the items, but the order is not
    # guaranteed; it depends on the timestamps, which depend on put order.
    anomaly_list.sort(key=lambda a: -a['end_revision'])
    expected_end_rev = 10110
    for alert in anomaly_list:
      self.assertEqual(expected_end_rev, alert['end_revision'])
      self.assertEqual(expected_end_rev - 5, alert['start_revision'])
      self.assertEqual(key_map[expected_end_rev], alert['key'])
      self.assertEqual('ChromiumGPU', alert['master'])
      self.assertEqual('linux-release', alert['bot'])
      self.assertEqual('scrolling-benchmark', alert['testsuite'])
      if expected_end_rev % 20 == 0:
        self.assertEqual('first_paint', alert['test'])
      else:
        self.assertEqual('mean_frame_time', alert['test'])
      self.assertEqual('100.0%', alert['percent_changed'])
      self.assertIsNone(alert['bug_id'])
      expected_end_rev -= 10
    self.assertEqual(expected_end_rev, 9990)

  def testGet_TriagedParameterSet_TriagedListed(self):
    self._AddAlertsToDataStore()
    response = self.testapp.get('/alerts', {'triaged': 'true'})
    anomaly_list = self.GetEmbeddedVariable(response, 'ANOMALY_LIST')
    # The alerts listed should contain those added above, including alerts
    # that have a bug ID that is not None.
    self.assertEqual(14, len(anomaly_list))
    expected_end_rev = 10130
    # The test below depends on the order of the items, but the order is not
    # guaranteed; it depends on the timestamps, which depend on put order.
    anomaly_list.sort(key=lambda a: -a['end_revision'])
    for alert in anomaly_list:
      if expected_end_rev == 10130:
        self.assertEqual(12345, alert['bug_id'])
      elif expected_end_rev == 10120:
        self.assertEqual(-1, alert['bug_id'])
      else:
        self.assertIsNone(alert['bug_id'])
      expected_end_rev -= 10
    self.assertEqual(expected_end_rev, 9990)

  def testGet_ImprovementsParameterSet_ListsImprovements(self):
    self._AddAlertsToDataStore()
    response = self.testapp.get('/alerts', {'improvements': 'true'})
    anomaly_list = self.GetEmbeddedVariable(response, 'ANOMALY_LIST')
    self.assertEqual(18, len(anomaly_list))

  def testGet_SheriffParameterSet_OtherSheriffAlertsListed(self):
    self._AddAlertsToDataStore()
    # Add another sheriff to the mock datastore, and set the sheriff of some
    # anomalies to be this new sheriff.
    sheriff2_key = sheriff.Sheriff(
        id='Sheriff2', email='sullivan@google.com').put()
    mean_frame_time = utils.TestKey(
        'ChromiumGPU/linux-release/scrolling-benchmark/mean_frame_time')
    anomalies = anomaly.Anomaly.query(
        anomaly.Anomaly.test == mean_frame_time).fetch()
    for anomaly_entity in anomalies:
      anomaly_entity.sheriff = sheriff2_key
      anomaly_entity.put()

    response = self.testapp.get('/alerts', {'sheriff': 'Sheriff2'})
    anomaly_list = self.GetEmbeddedVariable(response, 'ANOMALY_LIST')
    sheriff_list = self.GetEmbeddedVariable(response, 'SHERIFF_LIST')
    for alert in anomaly_list:
      self.assertEqual('mean_frame_time', alert['test'])
    self.assertEqual(2, len(sheriff_list))
    self.assertEqual('Chromium Perf Sheriff', sheriff_list[0])
    self.assertEqual('Sheriff2', sheriff_list[1])

  def testGet_StoppageAlerts_EmbedsStoppageAlertListAndOneTable(self):
    sheriff.Sheriff(id='Sheriff', patterns=['M/b/*/*']).put()
    testing_common.AddTests(['M'], ['b'], {'foo': {'bar': {}}})
    test_key = utils.TestKey('M/b/foo/bar')
    rows = testing_common.AddRows('M/b/foo/bar', {9800, 9802})
    for row in rows:
      stoppage_alert.CreateStoppageAlert(test_key.get(), row).put()
    response = self.testapp.get('/alerts?sheriff=Sheriff')
    stoppage_alert_list = self.GetEmbeddedVariable(
        response, 'STOPPAGE_ALERT_LIST')
    self.assertEqual(2, len(stoppage_alert_list))
    self.assertEqual(1, len(response.html('alerts-table')))

  @mock.patch('logging.error')
  def testGet_StoppageAlertWithBogusRow_LogsErrorAndShowsTable(
      self, mock_logging_error):
    sheriff.Sheriff(id='Sheriff', patterns=['M/b/*/*']).put()
    testing_common.AddTests(['M'], ['b'], {'foo': {'bar': {}}})
    test_key = utils.TestKey('M/b/foo/bar')
    row_parent = utils.GetTestContainerKey(test_key)
    row = graph_data.Row(parent=row_parent, id=1234)
    stoppage_alert.CreateStoppageAlert(test_key.get(), row).put()
    response = self.testapp.get('/alerts?sheriff=Sheriff')
    stoppage_alert_list = self.GetEmbeddedVariable(
        response, 'STOPPAGE_ALERT_LIST')
    self.assertEqual(1, len(stoppage_alert_list))
    self.assertEqual(1, len(response.html('alerts-table')))
    self.assertEqual(1, mock_logging_error.call_count)

  def testGet_WithNoAlerts_HasImageAndNoAlertsTable(self):
    response = self.testapp.get('/alerts')
    self.assertEqual(1, len(response.html('img')))
    self.assertEqual(0, len(response.html('alerts-table')))


if __name__ == '__main__':
  unittest.main()
