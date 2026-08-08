#!/bin/sh

set -eu

busctl=$1
response_file=$2
stopped_file=$3

"${busctl}" --user get-property \
    org.hyprshelld.Config1 \
    /org/hyprshelld/Config1 \
    org.hyprshelld.Config1 \
    BarHeight >"${response_file}"

set -- $("${busctl}" --user call \
    org.freedesktop.DBus \
    /org/freedesktop/DBus \
    org.freedesktop.DBus \
    GetConnectionUnixProcessID \
    s \
    org.hyprshelld.Config1)

test "$1" = "u"
service_pid=$2
kill "${service_pid}"

attempt=0
while kill -0 "${service_pid}" 2>/dev/null; do
    attempt=$((attempt + 1))
    if test "${attempt}" -ge 200; then
        kill -KILL "${service_pid}" 2>/dev/null || true
        exit 1
    fi
    sleep 0.01
done

printf '%s\n' "${service_pid}" >"${stopped_file}"
