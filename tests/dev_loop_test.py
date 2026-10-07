"""Exercise outlet state verification without contacting or powering hardware."""
import importlib.util
import pathlib
import tempfile
import unittest
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('n64_power', ROOT / 'tools/n64_power.py')
power = importlib.util.module_from_spec(spec)
spec.loader.exec_module(power)


class PowerTests(unittest.TestCase):
    def test_local_env_and_environment_override(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / '.env'
            path.write_text('# local settings\nN64_POWER_URL="http://example.invalid"\n')
            with patch.dict(power.os.environ, {}, clear=True):
                self.assertEqual(power.configured_power_url(path), 'http://example.invalid')
            with patch.dict(power.os.environ, {'N64_POWER_URL': 'http://override.invalid'}):
                self.assertEqual(power.configured_power_url(path), 'http://override.invalid')

    def test_missing_env_has_no_hardcoded_endpoint(self):
        with tempfile.TemporaryDirectory() as directory, patch.dict(power.os.environ, {}, clear=True):
            self.assertIsNone(power.configured_power_url(pathlib.Path(directory) / '.env'))

    def test_malformed_state_cannot_authorize_upload(self):
        switch = power.PowerSwitch('http://example.invalid')
        for result in ({'state': 'OFF'}, {'value': 'false'}, None):
            with self.subTest(result=result), patch.object(switch, 'request', return_value=result):
                with self.assertRaises(RuntimeError):
                    switch.state()

    def test_command_success_does_not_mean_relay_changed(self):
        switch = power.PowerSwitch('http://example.invalid')
        with patch.object(switch, 'request', return_value={'value': True}), patch.object(power.time, 'sleep'):
            with self.assertRaises(RuntimeError):
                switch.set(False)

    def test_explicit_off_waits_for_confirmation(self):
        switch = power.PowerSwitch('http://example.invalid')
        with patch.object(switch, 'request', side_effect=[None, {'value': True}, {'value': False}]) as request, patch.object(power.time, 'sleep'):
            switch.set(False)
            self.assertEqual(request.call_args_list[0].args, ('/turn_off',))
            self.assertEqual(request.call_args_list[0].kwargs, {'method': 'POST'})
            self.assertEqual(request.call_count, 3)


if __name__ == '__main__':
    unittest.main()
