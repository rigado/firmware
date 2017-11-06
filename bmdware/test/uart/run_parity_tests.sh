#!/bin/bash

bauds=("1200" "115200" "921600")

cd ..

test_desc="parity test"
test_script="uart/uart_parity_test.js"
results_dir="uart/results"
result_file_prefix="parity"
data_len=2048

if [ ! -d "$results_dir" ]; then
	mkdir $results_dir
fi

testcnt=0
passcnt=0

for baud in ${bauds[@]}; do 
	echo "================================="
	echo "$test_desc @ $baud baud, $data_len bytes"
	outfile=$results_dir/$result_file_prefix
	outfile+=_$baud
	outfile+=_$data_len.txt
	echo "writing results to: $outfile"
	node $test_script -r -b $baud -l $data_len > "$outfile"
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