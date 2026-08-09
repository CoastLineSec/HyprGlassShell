#!/bin/sh

set -eu

busctl=$1
list_response_file=$2
property_response_file=$3
stopped_file=$4

"${busctl}" --user call \
    org.hyprshelld.ComponentManager1 \
    /org/hyprshelld/ComponentManager1 \
    org.hyprshelld.ComponentManager1 \
    ListComponents >"${list_response_file}"

"${busctl}" --user get-property \
    org.hyprshelld.ComponentManager1 \
    /org/hyprshelld/ComponentManager1 \
    org.hyprshelld.ComponentManager1 \
    CatalogDigest >"${property_response_file}"

set -- $("${busctl}" --user call \
    org.freedesktop.DBus \
    /org/freedesktop/DBus \
    org.freedesktop.DBus \
    GetConnectionUnixProcessID \
    s \
    org.hyprshelld.ComponentManager1)

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
