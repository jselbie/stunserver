z=0
echo "start"
while [ 1 ]
do
    ../stunclient $1 $2 > /dev/null
    let z=$z+1
    echo $z
done

