if [ -z "$1" ]
then
    echo "Usage: ./scripts/validate_helper.sh [worker ID] [SORTED FILENAME]"
    exit
fi

#1== worker ID, 2 == sorted fileName

cd minutesort/scripts
./gensort-linux-1.5/64/valsort -o $1.sum $2
cat $1.sum >> all.sum
