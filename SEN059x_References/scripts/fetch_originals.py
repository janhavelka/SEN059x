#!/usr/bin/env python3
"""Download actual third-party sources listed in sources.json (run on user's internet-connected PC).

Saves original resources without modification and records verified local SHA-256.
No source file is marked as successfully fetched unless actual bytes were received.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / 'source_links' / 'sources.json'
OUT = ROOT / 'originals'
USER_AGENT = 'SEN059x-research-archive/1.0 (personal engineering reference)'


def fetch(item, timeout=35):
    target = OUT / item['filename']
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists() and target.stat().st_size > 0:
        return dict(id=item['id'], status='already_exists', bytes=target.stat().st_size,
                    sha256=hashlib.sha256(target.read_bytes()).hexdigest(), path=str(target.relative_to(ROOT)))
    req = urllib.request.Request(item['url'], headers={'User-Agent': USER_AGENT, 'Accept': '*/*'})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as response:
            body = response.read(30 * 1024 * 1024 + 1)
            content_type = response.headers.get('Content-Type', '')
            if len(body) > 30 * 1024 * 1024:
                raise ValueError('resource exceeds 30 MiB limit')
            if len(body) < 50:
                raise ValueError('suspiciously small response')
            if item['kind'] == 'pdf' and not body.startswith(b'%PDF-'):
                raise ValueError(f'not PDF: content-type {content_type}')
            if item['kind'] == 'github-zip' and not body.startswith(b'PK\x03\x04'):
                raise ValueError(f'not ZIP: content-type {content_type}')
            if item['kind'] == 'jpg' and not body.startswith(b'\xff\xd8'):
                raise ValueError(f'not JPEG: content-type {content_type}')
            if item['kind'] == 'html' and b'<html' not in body[:4096].lower() and b'<!doctype html' not in body[:4096].lower():
                raise ValueError(f'not HTML: content-type {content_type}')
            target.write_bytes(body)
            return dict(id=item['id'], status='downloaded', bytes=len(body),
                        sha256=hashlib.sha256(body).hexdigest(), path=str(target.relative_to(ROOT)))
    except Exception as e:
        if target.exists() and target.stat().st_size == 0:
            target.unlink()
        return dict(id=item['id'], status='FAILED', url=item['url'], error=str(e))


def main():
    parser = argparse.ArgumentParser(description='Fetch official SEN059x references and third-party libraries.')
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--required-only', action='store_true')
    parser.add_argument('--timeout', type=float, default=35)
    args = parser.parse_args()
    items = json.loads(MANIFEST.read_text(encoding='utf-8'))
    if args.required_only:
        items = [i for i in items if i['priority'] == 'required']
    results = []
    for item in items:
        if args.dry_run:
            print(f"{item['priority']:13} {item['kind']:10} {item['id']:31} {item['url']}")
            continue
        print(f"Fetching {item['id']}: {item['url']}", flush=True)
        result = fetch(item, timeout=args.timeout)
        results.append(result)
        print(f" -> {result['status']}", flush=True)
    if args.dry_run:
        return 0
    (OUT / 'download_results.json').write_text(json.dumps(results, indent=2, ensure_ascii=False), encoding='utf-8')
    good = sum(x['status'] in ('downloaded', 'already_exists') for x in results)
    print(f'Obtained {good}/{len(results)}; details: originals/download_results.json')
    return 0 if good == len(results) else 2


if __name__ == '__main__':
    sys.exit(main())
