#!/bin/sh
exec iconv -c -f utf-16 -t utf-8 < "$1" | tr -d '\r'
