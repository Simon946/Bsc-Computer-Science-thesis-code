mkdir /local/$USER
cp ~/minutesort/das6_worker/worker /local/$USER/worker
cp ~/minutesort/worker.CONFIG /local/$USER/worker.CONFIG
echo "doing copy now"

python3 ~/minutesort/sysmeasure/measure.py $1 & { sleep 3; cd /local/$USER/; ./worker > log.txt; }
#1 == worker name
