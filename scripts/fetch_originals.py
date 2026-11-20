#!/usr/bin/env python3
"""Fetch the reference catalog and validate the content of each downloaded or cached file."""
import argparse
import hashlib
from html.parser import HTMLParser
from io import BytesIO
import json
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import urllib.request
from zipfile import BadZipFile, ZipFile

REFERENCE_DIR = Path(__file__).resolve().parents[1] / 'docs' / 'reference'
MANIFEST = REFERENCE_DIR / 'source_links' / 'sources.json'
OUT = REFERENCE_DIR / 'originals'
USER_AGENT = 'SEN059x-research-archive/1.0 (personal engineering reference)'
MAX_BYTES = 30 * 1024 * 1024


class PageText(HTMLParser):
    """Look for the actual document body, even when the site also embeds app data."""

    def __init__(self):
        super().__init__()
        self.hidden = False
        self.parts = []

    def handle_starttag(self, tag, attrs):
        if tag in ('script', 'style'):
            self.hidden = True

    def handle_endtag(self, tag):
        if tag in ('script', 'style'):
            self.hidden = False

    def handle_data(self, data):
        if not self.hidden:
            self.parts.append(data)


def validate(item, body):
    kind = item['kind']
    if not 50 <= len(body) <= MAX_BYTES:
        raise ValueError(f'unexpected length ({len(body)} bytes)')
    if kind == 'html':
        if b'<html' not in body[:4096].lower():
            raise ValueError('missing HTML document')
        page = PageText()
        page.feed(body.decode('utf-8'))
        page.close()
        text = ' '.join(' '.join(page.parts).split()).casefold()
        for phrase in item['contains']:
            if phrase.casefold() not in text:
                raise ValueError(f'missing document text {phrase!r}')
    elif kind == 'github-zip':
        with ZipFile(BytesIO(body)) as archive:
            if archive.comment.decode('ascii') != item['revision'] or archive.testzip() is not None:
                raise ValueError('unexpected Git revision or corrupt ZIP entry')
    elif kind == 'pdf':
        if not body.startswith(b'%PDF-') or b'%%EOF' not in body[-1024:]:
            raise ValueError('incomplete PDF')
    elif kind == 'jpg':
        if not body.startswith(b'\xff\xd8') or not body.endswith(b'\xff\xd9'):
            raise ValueError('incomplete JPEG')
    elif kind == 'webp':
        if body[:4] != b'RIFF' or body[8:12] != b'WEBP' or int.from_bytes(body[4:8], 'little') + 8 != len(body):
            raise ValueError('incomplete WebP')
    elif kind == 'png':
        if not body.startswith(b'\x89PNG\r\n\x1a\n') or not body.endswith(b'IEND\xae\x42\x60\x82'):
            raise ValueError('incomplete PNG')
    else:
        raise ValueError(f'unknown resource kind {kind!r}')


def check_snapshot(item, body, source='local file'):
    if 'sha256' in item and (len(body) != item['bytes'] or hashlib.sha256(body).hexdigest() != item['sha256']):
        raise ValueError(f'{source} differs from the recorded snapshot for {item["filename"]}')


def obtain(item, target, timeout):
    if target.is_file():
        with target.open('rb') as cached:
            body = cached.read(MAX_BYTES + 1)
        try:
            validate(item, body)
            check_snapshot(item, body)
            return 'already_exists', body
        except (ValueError, BadZipFile):
            pass  # Replace a damaged cache entry only after the download validates.

    request = urllib.request.Request(item['url'], headers={'User-Agent': USER_AGENT})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        body = response.read(MAX_BYTES + 1)
    validate(item, body)
    check_snapshot(item, body, 'publisher response')
    target.parent.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(dir=target.parent) as staging:
        temporary = Path(staging) / 'download'
        temporary.write_bytes(body)
        temporary.replace(target)
    return 'downloaded', body


def fetch(item, timeout=35, verify=False):
    target = OUT / item['filename']
    report = dict(id=item['id'], url=item['url'], path=item['filename'])
    try:
        if verify:
            body = target.read_bytes()
            check_snapshot(item, body)
            status = 'verified'
        else:
            status, body = obtain(item, target, timeout)
        return dict(report, status=status, bytes=len(body), sha256=hashlib.sha256(body).hexdigest())
    except Exception as error:
        return dict(report, status='FAILED', error=str(error))


def main():
    parser = argparse.ArgumentParser(description='Fetch official SEN059x references and third-party libraries.')
    action = parser.add_mutually_exclusive_group()
    action.add_argument('--dry-run', action='store_true')
    action.add_argument('--verify', action='store_true',
                        help='Check the bundled snapshots against the recorded size and SHA-256 offline.')
    parser.add_argument('--required-only', action='store_true')
    parser.add_argument('--timeout', type=float, default=35)
    args = parser.parse_args()
    items = json.loads(MANIFEST.read_text(encoding='utf-8'))
    if args.required_only:
        items = [i for i in items if i['priority'] == 'required']
    if args.verify:
        items = [i for i in items if 'sha256' in i]
    results = []
    for item in items:
        if args.dry_run:
            print(f"{item['priority']:13} {item['kind']:10} {item['id']:31} {item['url']}")
            continue
        print(f"{item['id']}: {item['url']}", flush=True)
        result = fetch(item, timeout=args.timeout, verify=args.verify)
        results.append(result)
        print(f" -> {result['status']} {result.get('error', '')}".rstrip(), flush=True)
    if args.dry_run:
        return 0
    if args.verify:
        good = sum(x['status'] == 'verified' for x in results)
        print(f'Verified {good}/{len(results)} bundled snapshots.')
        return 0 if good == len(results) else 2
    OUT.mkdir(parents=True, exist_ok=True)
    report = OUT / 'download_results.json'
    with TemporaryDirectory(dir=OUT) as staging:
        temporary = Path(staging) / report.name
        temporary.write_text(json.dumps(results, indent=2, ensure_ascii=False), encoding='utf-8')
        temporary.replace(report)
    good = sum(x['status'] in ('downloaded', 'already_exists') for x in results)
    print(f'Obtained {good}/{len(results)}; details: {report}')
    return 0 if good == len(results) else 2


if __name__ == '__main__':
    sys.exit(main())
