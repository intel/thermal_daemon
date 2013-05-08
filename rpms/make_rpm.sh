#!/bin/bash

rsync -av --exclude='rpms' ../../thermal_daemon .
mv thermal_daemon thermal_daemon-1.0
tar cvzf thermal_daemon-1.0.tar.gz thermal_daemon-1.0
cp thermal_daemon-1.0.tar.gz ~/rpmbuild/SOURCES
rpmbuild -ba thermal_daemon.spec
rpmlint thermal_daemon.spec ~/rpmbuild/SRPMS/thermal*
rm -rf thermal_daemon-1.0
