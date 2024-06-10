#1 == 2 == pairs per worker

if [ -z "$1" ]
then
    echo "Usage: ./das6_generate.sh [PAIRS PER WORKER] [WORKER 1] [WORKER 2] ..."
    exit
fi

if [ -z "$2" ]
then
    echo "Usage: ./das6_generate.sh [PAIRS PER WORKER] [WORKER 1] [WORKER 2] ..."
    exit
fi

for ((i = 2; i <= $#; i++)); do
    offset=$(((i - 2) * $1))
    ssh ${!i} "mkdir /local/sbr571; rm -r /local/sbr571/*; mkdir /local/sbr571/$((i - 2)); ./minutesort/scripts/das6_generate_helper.sh $offset, $1, /local/sbr571/$((i - 2))/random.bin" &
    sleep 1
    echo "Generated for worker: ${!i} "
done

wait
