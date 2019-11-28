#!/bin/bash 



for input in $1/*.txt
do
	out=$input
	IFS='.'
	read -a out <<< "$out"
	IFS='/'
	read -a out2 <<< "${out[0]}"

	printf 'InputFile='$1/${out2[1]}.txt
	printf ' NumThreads=1\n'

	./tecnicofs-nosync $1/${out2[1]}.txt $2/${out2[1]}-1.txt 1 1 | tail -n 1	
done


for ((i=2; i<=$3; i++))
do 
	for input in $1/*.txt
	do
		out=$input
		IFS='.'
		read -a out <<< "$out"
		IFS='/'
		read -a out2 <<< "${out[0]}"

		printf 'InputFile='$1/${out2[1]}.txt
		printf ' NumThreads='$i'\n'
		
		./tecnicofs-mutex $1/${out2[1]}.txt $2/${out2[1]}-$i.txt $i $4 | tail -n 1
	done
done









