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

if [[ "abc" = a* ]]; then
    echo "wildcard glob works";             # yeah!
fi

if [[ 10 > 8 ]]; then
    echo "arithmetic comparison"
else
    echo "Ah... string comparison"          # sucks!
fi

if [[ 10 -gt 8 ]]; then
    echo "arithmetic comparison"            #yeah!
fi

LIST=$(ls);
echo $LIST;

# A safe way for executing commands
options=( "-a" "-d" "-l" )
options+=("foo")
options+=("bar")                            #array append
# If the index number is @ or *, all members of an array are referenced
# array size
echo ${#options[@]};
# element size
echo ${#options[1]};
# slice
echo ${options[@]:3:2};
# replace
echo ${options[@]/foo/fooo};
# loop
for i in ${options[@]}; do
    echo $i;
done

# use egrep for extended-regex-expression, including ?/+/(/etc.
# use fgrep for literally match
