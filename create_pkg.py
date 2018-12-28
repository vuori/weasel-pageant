#
# weasel-pageant packaging script.
# 
# Copyright 2017, 2018  Valtteri Vuorikoski
#
# This file is part of weasel-pageant, and is free software: you can
# redistribute it and/or modify it under the terms of the GNU General
# Public License as published by the Free Software Foundation, either
# version 3 of the License, or (at your option) any later version.
#
 
import sys
import zipfile
import re

CURRENT_VERSION = 'unknown'

try:
  reltype = sys.argv[1]
except IndexError:
  reltype = 'release'

if reltype not in ('release', 'debug'):
  print('usage: create_pkg release|debug', file=sys.stderr)
  sys.exit(1)

# Autodetect version from the source
with open('linux/main.c', 'r') as fp:
  for line in fp:
    m = re.match(r'#define VERSION "([^"]+)', line)
    if m:
      CURRENT_VERSION = m.group(1)

manifest = [
  'COPYING',
  'COPYING.PuTTY',
  'README.md'
]

if reltype == 'release':
  dirname = 'weasel-pageant-%s' % CURRENT_VERSION
  manifest.extend([
    'x64/Release/helper.exe',
    'linux/bin/x64/Release/weasel-pageant'
  ])
else:
  dirname = 'weasel-pageant-%s-%s' % (reltype, CURRENT_VERSION)
  manifest.extend([
    'x64/Debug/helper.exe',
    'linux/bin/x64/Debug/weasel-pageant'
  ])

outname = dirname + '.zip'
zipf = zipfile.ZipFile(outname, 'w', compression=zipfile.ZIP_DEFLATED)

def basename(x):
  return x.rsplit('/')[-1]

for fname in manifest:
  zipf.write(fname, '%s/%s' % (dirname, basename(fname)))

zipf.close()
sys.exit(0)
