"""Read Cuebase Actions status or download run evidence using Git's configured auth.

Credentials remain in memory and are sent only to api.github.com. Artifact/log
redirects are fetched without authorization headers. This does not create runs,
change repository settings or publish releases.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import urllib.error
import urllib.request
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[1]
API = 'https://api.github.com/repos/bluecharles159-ctrl/Cuebase'

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None

def request(endpoint, authenticated=False):
    headers = {'User-Agent': 'Cuebase-build-monitor', 'Accept': 'application/vnd.github+json'}
    if authenticated:
        env = dict(os.environ, GIT_TERMINAL_PROMPT='0', GCM_INTERACTIVE='never')
        result = subprocess.run(['git', 'credential', 'fill'], input='protocol=https\nhost=github.com\n\n', text=True, capture_output=True, env=env, cwd=ROOT)
        values = dict(line.split('=', 1) for line in result.stdout.splitlines() if '=' in line)
        token = values.get('password')
        if result.returncode or not token:
            raise RuntimeError('GitHub authentication is required; sign in using Git Credential Manager.')
        headers['Authorization'] = 'Bearer '+token
    opener = urllib.request.build_opener(NoRedirect)
    try:
        with opener.open(urllib.request.Request(API+endpoint, headers=headers), timeout=45) as response:
            return response.read()
    except urllib.error.HTTPError as error:
        if error.code in (401, 403) and not authenticated:
            return request(endpoint, True)
        if error.code in (301, 302, 303, 307, 308):
            location = error.headers['Location']
            parsed = urlparse(location)
            host = parsed.hostname or ''
            if parsed.scheme != 'https' or not any(host.endswith(suffix) for suffix in ('.github.com', '.githubusercontent.com', '.blob.core.windows.net')):
                raise RuntimeError('Unexpected GitHub download redirect host')
            with urllib.request.urlopen(location, timeout=90) as response:
                return response.read()
        raise RuntimeError(f'GitHub returned HTTP {error.code} for {endpoint}') from None

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=('status', 'logs', 'artifacts', 'download'))
    parser.add_argument('id', type=int)
    args = parser.parse_args()
    if args.action == 'status':
        run = json.loads(request(f'/actions/runs/{args.id}'))
        jobs = json.loads(request(f'/actions/runs/{args.id}/jobs'))
        print(json.dumps({'url':run['html_url'], 'sha':run['head_sha'], 'status':run['status'], 'conclusion':run['conclusion'], 'jobs':[{'id':job['id'],'status':job['status'],'conclusion':job['conclusion'],'steps':[{key:step.get(key) for key in ('name','status','conclusion')} for step in job['steps']]} for job in jobs['jobs']]}, indent=2))
    elif args.action == 'artifacts':
        items = json.loads(request(f'/actions/runs/{args.id}/artifacts'))
        print(json.dumps([{key:item[key] for key in ('id','name','size_in_bytes','expired')} for item in items['artifacts']], indent=2))
    else:
        endpoint = f'/actions/runs/{args.id}/logs' if args.action == 'logs' else f'/actions/artifacts/{args.id}/zip'
        directory = ROOT/'artifacts'/'github'
        directory.mkdir(parents=True, exist_ok=True)
        destination = directory/f'{args.action}-{args.id}.zip'
        destination.write_bytes(request(endpoint))
        print(destination)

if __name__ == '__main__':
    main()
