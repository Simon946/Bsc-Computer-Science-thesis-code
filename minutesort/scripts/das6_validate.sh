#1 == number of workers. 2 == pairs per worker

if [ -z "$1" ]
then
    echo "Usage: ./scripts/das6_validate.sh [WORKER 1] [WORKER 2] ..."
    exit
fi

for ((i=1; i<=$#; i+=1)); do
    worker=${!i}
    ssh $worker "./minutesort/scripts/das6_validate_helper.sh $worker /local/$USER/$((i - 1))/sorted.bin"
    echo "Done validating for worker: $worker"
done

./gensort-linux-1.5/64/valsort -s all.sum
rm all.sum
