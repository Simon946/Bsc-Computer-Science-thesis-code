if [ -z "$1" ]
then
    echo "Usage: ./scripts/validate.sh [NUMBER OF WORKERS]"
    exit
fi


#1 == number of workers. 

cd ..

for ((i=0; i<$1; i++)); do 
    ./scripts/gensort-linux-1.5/64/valsort -o out$i.sum ./data/$i/sorted.bin
    cat out$i.sum >> all.sum
    rm out$i.sum
done

./scripts/gensort-linux-1.5/64/valsort -s all.sum
rm all.sum
