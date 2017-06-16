#!/usr/bin/bash
# Determine the data file
case $1 in
4096)
	data_file="graph4096"
	;;
128M)
	data_file="graph128M"
	;;
*)
	data_file="graph16M"
	;;
esac
version="ins-exe-vec"
bin_addr="/home/zpeng/benchmarks/test/openmp_ins_exe_vec"
data_addr="/home/zpeng/benchmarks/rodinia_3.1/data/bfs"
no_core=64
#power_max=16
result_file="result_${version}_${data_file}_$(date +%Y%m%d-%H%M%S).txt"

touch $result_file
echo "Threads I/E-Vec" >> $result_file
#echo "Buffer_Size Time" >> $result_file
tno=1
#power=4
#size=$((2 ** $power))
while [ $tno -le $no_core ]
#while [ $power -le $power_max ]
do
	${bin_addr}/bfs ${tno} ${data_addr}/${data_file}.txt 4096 >> $result_file
#${bin_addr}/bfs 1 ${data_addr}/${data_file}.txt ${size} >> $result_file
	echo -n .
	tno=$((tno*2))
#power=$((power + 1))
#size=$((2 ** $power))
done
echo done.