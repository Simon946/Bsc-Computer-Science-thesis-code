if [ -z "$1" ]
then
    echo "Usage: ./scripts/start_workers.sh [WORKER 1] [WORKER 2] ..."
    exit
fi


for worker in "$@"
do
    ssh $worker "./minutesort/scripts/das6_start_workers_helper.sh $worker" &
    sleep 1
    echo "Started worker: $worker"
done

wait
