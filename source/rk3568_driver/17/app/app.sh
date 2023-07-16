#!/bin/bash
taskset -c 0 ./app /dev/device_test topeet &
taskset -c 1 ./app /dev/device_test topeet &
taskset -c 2 ./app /dev/device_test topeet &
taskset -c 3 ./app /dev/device_test topeet &
taskset -c 0 ./app /dev/device_test topeet &
taskset -c 1 ./app /dev/device_test topeet &
taskset -c 2 ./app /dev/device_test topeet &
taskset -c 3 ./app /dev/device_test topeet &
