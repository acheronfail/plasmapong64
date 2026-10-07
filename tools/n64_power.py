#!/usr/bin/env python3
"""Control the N64's ESPHome outlet with explicit, verified on/off commands."""
import argparse
import json
import os
import pathlib
import time
import urllib.parse
import urllib.request


def configured_power_url(env_path=None):
    """Read only the outlet setting; environment overrides the local .env file."""
    if os.environ.get('N64_POWER_URL'):
        return os.environ['N64_POWER_URL']
    path = pathlib.Path(env_path) if env_path is not None else pathlib.Path(__file__).resolve().parent.parent / '.env'
    if path.is_file():
        for line in path.read_text().splitlines():
            key, separator, value = line.strip().partition('=')
            if separator and key.strip() == 'N64_POWER_URL':
                value = value.strip()
                if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
                    value = value[1:-1]
                if value:
                    return value
    return None


class PowerSwitch:
    def __init__(self, url, entity='switch', timeout=5):
        self.endpoint = url.rstrip('/') + '/switch/' + urllib.parse.quote(entity, safe='')
        self.timeout = timeout

    def request(self, suffix='', method='GET'):
        request = urllib.request.Request(self.endpoint + suffix, method=method)
        with urllib.request.urlopen(request, timeout=self.timeout) as response:
            body = response.read()
        return json.loads(body) if body else None

    def state(self):
        result = self.request()
        if not isinstance(result, dict) or not isinstance(result.get('value'), bool):
            raise RuntimeError('Power switch did not return a boolean state')
        return result['value']

    def set(self, on):
        self.request('/turn_on' if on else '/turn_off', method='POST')
        for attempt in range(10):
            if self.state() == on:
                return
            time.sleep(.2)
        raise RuntimeError('Power switch did not reach the requested state')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('status', 'on', 'off'))
    parser.add_argument('--url', default=configured_power_url(), help='outlet URL; defaults to N64_POWER_URL in environment or .env')
    parser.add_argument('--entity', default='switch')
    args = parser.parse_args()
    if not args.url:
        parser.error('set N64_POWER_URL in .env or the environment, or provide --url')
    switch = PowerSwitch(args.url, args.entity)
    if args.action != 'status':
        switch.set(args.action == 'on')
    print('N64 power: ' + ('ON' if switch.state() else 'OFF'))


if __name__ == '__main__':
    main()
