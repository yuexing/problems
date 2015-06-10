#!/bin/bash

#Black        0;30     Dark Gray     1;30
#Blue         0;34     Light Blue    1;34
#Green        0;32     Light Green   1;32
#Cyan         0;36     Light Cyan    1;36
#Red          0;31     Light Red     1;31
#Purple       0;35     Light Purple  1;35
#Brown/Orange 0;33     Yellow        1;33
#Light Gray   0;37     White         1;37


GREEN='\033[0;32m'
RED='\033[0;31m'
ORG='\033[0;33m'
NO_COLOR='\033[0m'

echo "${RED} red ${GREEN} green ${ORG} orange ${NO_COLOR}";

case "$@" in
    "slow")
        ;;
    *)
    ;;
esac

# set variable c
# when referring to a field, use the prefix '$' (the positional variable); otherwise, use c
awk '{print $c}' c=${1:-1}
# eg.
awk "/$regex/"'{print FILENAME, ":", NR, ":", $0, ":", comment}' comment="$comment" $file
# awk begin can be used to write toy program
awk 'BEGIN{x =1; print x++, " ", ++x}'
# start an awk file with for the interpreter
#!/bin/awk -f

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
    echo "yes!"                             #!
else
    echo "hmmm..."
fi

if [[ $i > 1 ]]; then
    echo "definitely should use [[]]";      #!
fi

if [[ "abc" = a* ]]; then
    echo "wildcard glob works";             #!
fi

if [[ 10 > 8 ]]; then
    echo "arithmetic comparison"
else
    echo "Ah... string comparison"          #!
fi

if [[ 10 -gt 8 ]]; then
    echo "arithmetic comparison"            #!
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

# find 
# -exec: each command ends with ';', need to be \; also use sh -c "cmd && cmd"
# -execdir: go into the dir and exec
find . -name "* *"  -execdir rename -v 's/ /_/' {} \;

# file read/write
cat > temp << END
    app1    path1
    app2    path2
END
while read app path; do
done < temp

# loop through numbers
for i in $(seq 1 5); do echo $i; done

for i in {1..5}; do echo $i; done
# run || FAILED="FAILED $app"

# string substitution
a="string.txt.bak"; 
echo '${a%.*}' ${a%.*}; # string.txt
echo '${a%%.*}' ${a%%.*} # string
