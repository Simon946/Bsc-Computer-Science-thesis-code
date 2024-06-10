#1 == number of workers. 2 == pairs per worker

if [ -z "$1" ]
then
    echo "Usage: ./generate.sh [NUMBER OF WORKERS] [PAIRS PER WORKER]"
    exit
fi

if [ -z "$2" ]
then
    echo "Usage: ./generate.sh [NUMBER OF WORKERS] [PAIRS PER WORKER]"
    exit
fi

cd ..
mkdir ./data

for ((i=0; i<$1; i++)); do 
    offset=$(($2 * i))
    mkdir ./data/$i/
    ./scripts/gensort-linux-1.5/64/gensort "-b$offset" $2 ./data/$i/random.bin
    echo "generated for worker: $i"
done
