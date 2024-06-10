if [ -z "$1" ]
then
    echo "Usage: ./scripts/start_workers.sh [NUMBER OF WORKERS]"
    exit
fi


for ((i=0; i<$1; i++)); do 
    cd worker$i #for relative file locations
    ./worker > ./log.txt &
    cd ..
    sleep 1
    echo "Started worker: $i"
done


wait

pkill -P $$
echo "all programs finished"

