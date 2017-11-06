#!/bin/bash

bauds=("1200" "115200" "921600")

cd ..

test_desc="passthrough big (flow ctrl) test"
test_script="uart/uart_parity_test.js"
results_dir="uart/results"
result_file_prefix="passthrough_big"
data_len=500000

if [ ! -d "$results_dir" ]; then
	mkdir $results_dir
fi

testcnt=0
passcnt=0

for baud in ${bauds[@]}; do
	
	data_len_actual=$data_len 

	if [ $baud -lt 115200 ]; then
		data_len_actual=10000
	fi

	echo "================================="
	echo "$test_desc @ $baud baud, $data_len_actual bytes"
	outfile=$results_dir/$result_file_prefix
	outfile+=_$baud
	outfile+=_$data_len_actual.txt
	echo "writing results to: $outfile"
	node $test_script -r -b $baud -l $data_len_actual > "$outfile"
	head $outfile
	echo "..."
	tail $outfile

	passstring=`grep -e "Test Result: PASS" "$outfile"`

	if [ -n "$passstring" ]
		then
		((passcnt++))
	fi

	((testcnt++))

	echo "$passcnt/$testcnt tests passed"

done

cd uart