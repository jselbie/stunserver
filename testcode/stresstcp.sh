z=0
while [ 1 ]
do
    nc $1 $2 < packet.bin > /dev/null
    let z=$z+1
    echo $z
done

