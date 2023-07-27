#!/bin/bash

echo "Make sure that thermald is not started with --adaptive option"

source rapl.sh
source intel_pstate.sh
source powerclamp.sh
source processor.sh
#source cpufreq.sh
source default.sh
source exec_config_tests.sh
