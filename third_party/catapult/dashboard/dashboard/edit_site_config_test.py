# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import webapp2
import webtest

from google.appengine.api import users

from dashboard import edit_site_config
from dashboard import namespaced_stored_object
from dashboard import stored_object
from dashboard import testing_common
from dashboard import xsrf


class EditSiteConfigTest(testing_common.TestCase):

  def setUp(self):
    super(EditSiteConfigTest, self).setUp()
    app = webapp2.WSGIApplication(
        [('/edit_site_config', edit_site_config.EditSiteConfigHandler)])
    self.testapp = webtest.TestApp(app)
    testing_common.SetInternalDomain('internal.org')
    self.SetCurrentUser('foo@internal.org', is_admin=True)

  def testGet_NoKey_ShowsPageWithNoTextArea(self):
    response = self.testapp.get('/edit_site_config')
    self.assertEqual(0, len(response.html('textarea')))

  def testGet_WithNonNamespacedKey_ShowsPageWithCurrentValue(self):
    stored_object.Set('foo', 'XXXYYY')
    response = self.testapp.get('/edit_site_config?key=foo')
    self.assertEqual(1, len(response.html('form')))
    self.assertIn('XXXYYY', response.body)

  def testGet_WithNamespacedKey_ShowsPageWithBothVersions(self):
    namespaced_stored_object.Set('foo', 'XXXYYY')
    namespaced_stored_object.SetExternal('foo', 'XXXinternalYYY')
    response = self.testapp.get('/edit_site_config?key=foo')
    self.assertEqual(1, len(response.html('form')))
    self.assertIn('XXXYYY', response.body)
    self.assertIn('XXXinternalYYY', response.body)

  def testPost_NoXsrfToken_ReturnsErrorStatus(self):
    self.testapp.post('/edit_site_config', {
        'key': 'foo',
        'value': '[1, 2, 3]',
    }, status=403)

  def testPost_ExternalUser_ShowsErrorMessage(self):
    self.SetCurrentUser('foo@external.org')
    response = self.testapp.post('/edit_site_config', {
        'key': 'foo',
        'value': '[1, 2, 3]',
        'xsrf_token': xsrf.GenerateToken(users.get_current_user()),
    })
    self.assertIn('Only internal users', response.body)

  def testPost_WithKey_UpdatesNonNamespacedValues(self):
    self.testapp.post('/edit_site_config', {
        'key': 'foo',
        'value': '[1, 2, 3]',
        'xsrf_token': xsrf.GenerateToken(users.get_current_user()),
    })
    self.assertEqual([1, 2, 3], stored_object.Get('foo'))

  def testPost_WithSomeInvalidJSON_ShowsErrorAndDoesNotModify(self):
    stored_object.Set('foo', 'XXX')
    response = self.testapp.post('/edit_site_config', {
        'key': 'foo',
        'value': '[1, 2, this is not json',
        'xsrf_token': xsrf.GenerateToken(users.get_current_user()),
    })
    self.assertIn('Invalid JSON', response.body)
    self.assertEqual('XXX', stored_object.Get('foo'))

  def testPost_WithKey_UpdatesNamespacedValues(self):
    namespaced_stored_object.Set('foo', 'XXXinternalYYY')
    namespaced_stored_object.SetExternal('foo', 'XXXYYY')
    self.testapp.post('/edit_site_config', {
        'key': 'foo',
        'external_value': '{"x": "y"}',
        'internal_value': '{"x": "yz"}',
        'xsrf_token': xsrf.GenerateToken(users.get_current_user()),
    })
    self.assertEqual({'x': 'yz'}, namespaced_stored_object.Get('foo'))
    self.assertEqual({'x': 'y'}, namespaced_stored_object.GetExternal('foo'))

  def testPost_SendsNotificationEmail(self):
    self.testapp.post('/edit_site_config', {
        'key': 'foo',
        'external_value': '{"x": "y"}',
        'internal_value': '{"x": "yz"}',
        'xsrf_token': xsrf.GenerateToken(users.get_current_user()),
    })
    messages = self.mail_stub.get_sent_messages()
    self.assertEqual(1, len(messages))
    self.assertEqual('gasper-alerts@google.com', messages[0].sender)
    self.assertEqual('chrome-perf-dashboard-alerts@google.com', messages[0].to)
    self.assertEqual(
        'Config "foo" changed by foo@internal.org', messages[0].subject)
    self.assertIn('{"x": "y"}', str(messages[0].body))
    self.assertIn('{"x": "yz"}', str(messages[0].body))


if __name__ == '__main__':
  unittest.main()
