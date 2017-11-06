#!/bin/bash
#-------------------------------------------------------------------------------
#  File: gpio_write_test.sh
#  Description:	BMDware BLE control port test
#				Run a series of write tests
#-------------------------------------------------------------------------------
#  Disclosure
#
#  Copyright (c) 2017 Rigado, LLC
#  All rights reserved.
#
#  Licensees are granted free, non-transferable use of the information. NO
#  WARRANTY of ANY KIND is provided. This heading must NOT be removed from
#  the file.
#-------------------------------------------------------------------------------
device=94:54:93:06:AF:15

#-------------------------------------------------------------------------------
#	Set all pins to output
#-------------------------------------------------------------------------------
	for i in {0..22}
	do
		printf "Testing pin (output): %d\n" $i
		var=$(printf "50%02x0100" $i)
		./test_wr.sh $device $var
	done

#-------------------------------------------------------------------------------
#	Test all possible pin writes (value is zero)
#-------------------------------------------------------------------------------
	for i in {0..255}
	do
		printf "Testing pin write (zero): %d\n" $i
		var=$(printf "51%02x00" $i)
		./test_wr.sh $device $var
	done

#-------------------------------------------------------------------------------
#	Test all possible pin writes (value is one)
#-------------------------------------------------------------------------------
	for i in {0..255}
	do
		printf "Testing pin write (one): %d\n" $i
		var=$(printf "51%02x01" $i)
		./test_wr.sh $device $var
	done


#-------------------------------------------------------------------------------
#	Test all possible pin write values
#-------------------------------------------------------------------------------
	for i in {0..255}
	do
		printf "Testing pin write values: %d\n" $i
		var=$(printf "5100%02x" $i)
		./test_wr.sh $device $var
	done


#-------------------------------------------------------------------------------
#	Reset all valid pins to inputs (no pull)
#-------------------------------------------------------------------------------
	for i in {0..22}
	do
		printf "reset to input: %d\n" $i
		var=$(printf "50%02x0000" $i)
		./test_wr.sh $device $var
	done
