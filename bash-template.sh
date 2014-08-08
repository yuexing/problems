#!/bin/bash

echo "hello ";

EXTRA=""
while [ $# -gt 1 ]; do      #use gt/lt/eq
    EXTRA+=" $1 ";          #+= works for bash, so it's better use "$foo$bar"
    echo $EXTRA;
    shift;
done

# (...): for subshell
# { ... }: for grouping
# ((...)): arithmetic 
# [[ ... ]]: conditional expression -n number -e exists
# stop using [] now!
i=$((1+2));
echo $i;

if [[ $i = 1 || $i = 3 ]]; then
    echo "yes!"
else
    echo "hmmm..."
fi

if [[ $i > 1 ]]; then
    echo "definitely should use [[]]";
fi

LIST=$(ls);
echo $LIST;

# use egrep for extended-regex-expression, including ?/+/(/etc.
# use fgrep for literally match
