#!/usr/bin/env python3
"""Collects the built plugins into dist/ and zips them: cdaw-plugins-<name>.zip.

    python scripts/package.py <build dir> <config> <name>
"""
import glob
import os
import shutil
import sys

build, config, name = sys.argv[1:4]
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
stage = os.path.join(root, "dist", "cdaw-plugins-" + name)
shutil.rmtree(stage, ignore_errors=True)
os.makedirs(stage)

found = 0
for pattern, folder in (("*.vst3", "VST3"), ("*.component", "AU")):
    for bundle in glob.glob(os.path.join(build, "VST3", "*", "*_artefacts", config, folder, pattern)):
        target = os.path.join(stage, folder, os.path.basename(bundle))
        shutil.copytree(bundle, target, symlinks=True)
        found += 1

if found == 0:
    sys.exit("No plugins found under " + build)

for doc in ("LICENSE", "THIRD_PARTY.md", "README.md"):
    shutil.copy2(os.path.join(root, doc), stage)

archive = shutil.make_archive(stage, "zip", os.path.dirname(stage), os.path.basename(stage))
print(found, "plugins in", archive)
