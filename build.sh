#! /bin/bash

P=/tmp/MyMultipleKeyboards.dst
rm -rf "$P" && xcodebuild install -alltargets CHOWN=/usr/bin/true && pkgbuild --root "$P" --scripts installer-scripts MyMultipleKeyboards.pkg

