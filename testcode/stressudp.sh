z=0
echo "start"
while [ 1 ]
do
    ../stunclient localhost 3478 > /dev/null
    let z=$z+1
    echo $z
done

