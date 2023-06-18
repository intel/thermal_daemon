#!/bin/bash
BRANCH=$(git branch | grep '^\*' | sed 's/^..\(.*\)/\1/')
HASH=$(git rev-parse ${BRANCH})
TAG=$(git describe --tags --abbrev=0)
RELEASE=$(git describe --tags | cut -d- -f2 | tr - _)
SHORT_COMMIT=$(git rev-parse HEAD | cut -c 1-7)

sed -i "s,^%global commit.*,%global commit  ${HASH}," thermal-daemon.spec
sed -i "s,^Version:.*,Version: ${TAG}," thermal-daemon.spec
sed -i "s,^Release.*,Release: ${RELEASE}%{?dist}," thermal-daemon.spec

rsync -a --exclude='rpms' --exclude='.git' ../../thermal_daemon .
mv thermal_daemon thermal_daemon-${SHORT_COMMIT}
tar czf ~/rpmbuild/SOURCES/thermal_daemon-${TAG}-${SHORT_COMMIT}.tar.gz thermal_daemon-${SHORT_COMMIT}
rpmbuild -bs thermal-daemon.spec
# /usr/bin/mock -r fedora-21-x86_64 ~/SRPMS/thermal-daemon-*.src.rpm
rpmbuild -ba thermal-daemon.spec
rpmlint thermal-daemon.spec ~/rpmbuild/SRPMS/thermal*
rm -rf thermal_daemon-${SHORT_COMMIT}
