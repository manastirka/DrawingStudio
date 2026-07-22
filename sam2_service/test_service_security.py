#!/usr/bin/env python3
"""Security contract tests for the local DrawingStudio SAM2 service."""

import os
import sys
import unittest
import importlib.util


SERVICE_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SERVICE_DIR)
os.environ['DRAWINGSTUDIO_SAM2_TOKEN'] = 'drawingstudio-python-test-token-32-bytes'

spec = importlib.util.spec_from_file_location(
    'drawingstudio_sam2_service', os.path.join(SERVICE_DIR, 'sam2_service.py'))
service = importlib.util.module_from_spec(spec)
spec.loader.exec_module(service)


class ServiceSecurityTest(unittest.TestCase):
    def setUp(self):
        self.client = service.app.test_client()
        self.authorization = {
            'Authorization': 'Bearer drawingstudio-python-test-token-32-bytes'
        }

    def test_health_requires_bearer_token(self):
        self.assertEqual(self.client.get('/health').status_code, 401)
        self.assertEqual(
            self.client.get('/health', headers={'Authorization': 'Bearer wrong'}).status_code,
            401,
        )

        response = self.client.get('/health', headers=self.authorization)
        self.assertEqual(response.status_code, 200)
        self.assertTrue(response.get_json()['auth_required'])
        self.assertEqual(response.headers['X-DrawingStudio-SAM2-Protocol'], '2')
        self.assertNotIn('Access-Control-Allow-Origin', response.headers)
        self.assertEqual(response.headers['Cache-Control'], 'no-store')

    def test_request_size_is_bounded_before_processing(self):
        original_limit = service.app.config['MAX_CONTENT_LENGTH']
        service.app.config['MAX_CONTENT_LENGTH'] = 32
        try:
            response = self.client.post(
                '/segment_all',
                headers=self.authorization,
                data=b'x' * 33,
                content_type='application/json',
            )
            self.assertEqual(response.status_code, 413)
        finally:
            service.app.config['MAX_CONTENT_LENGTH'] = original_limit


if __name__ == '__main__':
    unittest.main()
