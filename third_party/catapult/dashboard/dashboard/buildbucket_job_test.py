# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from dashboard import buildbucket_job
from dashboard import testing_common


class BuildbucketJobTest(testing_common.TestCase):

  def setUp(self):
    super(BuildbucketJobTest, self).setUp()
    self._args_base = {
        'bisect_director': 'linux_perf_bisector',
        'recipe_tester_name': 'linux_perf_bisect',
        'good_revision': '1',
        'bad_revision': '2',
        'test_command': 'tools/perf/dummy_command',
        'metric': 'dummy_metric',
        'repeats': '5',
        'truncate': '25',
        'timeout_minutes': '10',
        'bug_id': None,
        'gs_bucket': 'dummy_bucket',
        'builder_host': None,
        'builder_port': None,
    }

  def testCreateJob(self):
    job = buildbucket_job.BisectJob(**self._args_base)
    params = job.GetBuildParameters()
    assert isinstance(params, dict)
    self.assertIn('builder_name', params)
    properties = params['properties']
    assert isinstance(properties, dict)
    bisect_config = properties['bisect_config']
    assert isinstance(bisect_config, dict)
    self.assertIn('test_type', bisect_config)
    self.assertIn('command', bisect_config)
    self.assertIn('src', bisect_config['command'])
    self.assertIn('metric', bisect_config)
    self.assertIn('good_revision', bisect_config)
    self.assertIn('bad_revision', bisect_config)
    self.assertIn('repeat_count', bisect_config)
    self.assertIn('max_time_minutes', bisect_config)
    self.assertIn('truncate_percent', bisect_config)

  def testMissingRequiredArgs(self):
    self._args_base['test_command'] = None
    with self.assertRaises(ValueError):
      job = buildbucket_job.BisectJob(**self._args_base)
      _ = job.GetBuildParameters()

if __name__ == '__main__':
  unittest.main()
