if [ -z "$1" ]
then
    echo "Usage: ./scripts/das6_run.sh [WORKER THREAD COUNT] [WORKER RECEIVE COUNT] [WORKER MEMORY COUNT] [KVPAIR COUNT] [MASTER] [WORKER 1] [WORKER 2] ..."
    exit
fi


echo "NUMBEROFWORKERS = $(($# - 5))" > ~/minutesort/master.CONFIG
echo "NUMBEROFKVPAIRS = $4" >> ~/minutesort/master.CONFIG
echo "INPUTFILENAME = /local/$USER" >> ~/minutesort/master.CONFIG
echo "OUTPUTFILENAME = /local/$USER" >> ~/minutesort/master.CONFIG
echo "WORKERTHREADCOUNT = $1" >> ~/minutesort/master.CONFIG
echo "WORKERRECEIVEMERGECOUNT = $2" >> ~/minutesort/master.CONFIG
echo "WORKERMEMORYCOUNT = $3" >> ~/minutesort/master.CONFIG

cd ..
./master &
sleep 1
nodeID="${5:4}"
withoutZeros=$((10#$nodeID))
echo "MASTERIP = 10.149.0.$withoutZeros" >> ./worker.CONFIG
./scripts/das6_start_workers.sh "${@:6}"

wait
