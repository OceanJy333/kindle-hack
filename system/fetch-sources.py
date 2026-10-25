#!/usr/bin/env python3
# input: pinned upstream metadata; output: verified archives in build/independent-os; pos: public source acquisition.
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import tempfile
import urllib.request
from sources import SOURCES

WORK = Path(__file__).resolve().parent.parent / 'build/independent-os'


def fetch(name):
    source = SOURCES[name]
    destination = WORK / source['archive']
    if destination.exists():
        if hashlib.file_digest(destination.open('rb'), 'sha256').hexdigest() == source['sha256']:
            print(f'{name}: verified {destination}')
            return
        raise ValueError(f'{destination}: SHA-256 mismatch')
    WORK.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=WORK) as temporary:
        request = urllib.request.Request(source['url'], headers={'User-Agent': 'kindle-hack-source-fetch'})
        with urllib.request.urlopen(request, timeout=60) as response:
            shutil.copyfileobj(response, temporary)
        temporary.flush()
        if hashlib.file_digest(Path(temporary.name).open('rb'), 'sha256').hexdigest() != source['sha256']:
            raise ValueError(f'{name}: downloaded SHA-256 mismatch')
        shutil.copyfile(temporary.name, destination)
    print(f'{name}: downloaded {destination}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('components', nargs='*', choices=list(SOURCES))
    parser.add_argument('--list', action='store_true', help='print pinned URLs and checksums')
    options = parser.parse_args()
    if options.list:
        print(json.dumps(SOURCES, indent=2))
    else:
        for name in options.components or SOURCES:
            fetch(name)
