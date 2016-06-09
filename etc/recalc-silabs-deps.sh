#!/bin/sh

cd `dirname "$0"`/..
find . -name '*.Plo' | xargs cat | grep third_party/silabs-thread-1.0 | sed 's:^.*/\(silabs-thread-1.0/\):\1:;s/[ ]*[:\]$//' | sort -u
