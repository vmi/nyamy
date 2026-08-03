#!/bin/bash

cat IDS_version.txt |
  tr -d '\r'        |
  tr '\n' '\0'      |
  sed -E 's@\x00+$@@; s@\x00@\\r\\n@g' |
  tee /dev/clipboard
echo
