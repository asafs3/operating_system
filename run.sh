compare() {
    
    counter=`cat counter$1.txt`
    if [ $counter -ne $2 ]
        then
            echo "problem in counter$1 : 
            supposed to be $2, but received $counter"
    fi
}

make clean
make
for i in {1..1}
do 
    ./hw2 new_test.txt 4096 100 1
    if [ $? -ne "0" ]
    then
        echo "error in test"
        exit
    fi


    compare "00" 1
    compare "01" 2
    compare "02" 3
    compare "03" -4
    compare "04" 4
    compare "05" 0
    compare "06" -5
    compare "07" 1
    compare "08" 5
    compare "09" 5
    compare "10" 5
    compare "11" 5
    compare "12" 5
    compare "13" 5
    compare "14" 5

    echo "################### comapre finished ###################"
done

exit
