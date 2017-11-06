#!/bin/bash
#-------------------------------------------------------------------------------
#  File: gpio_read_test.sh
#  Description:	BMDware BLE control port test
#				Runs a number of tests using gio read command (0x52)
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
#	Reset all valid pins to inputs (no pull up)
#-------------------------------------------------------------------------------
	for i in {0..22}
	do
		printf "configure to input (with pull-up): %d\n" $i
		var=$(printf "50%02x0003" $i)
		./test_wr.sh $device $var
	done

#-------------------------------------------------------------------------------
#	Read all pins
#-------------------------------------------------------------------------------
	for i in {0..22}
	do
		printf "Testing pin read: %d\n" $i
		var=$(printf "52%02x" $i)
		./test_wr.sh $device $var
	done


#-------------------------------------------------------------------------------
#	Read all pins (incorrect length)
#-------------------------------------------------------------------------------
	for i in {0..22}
	do
		printf "Testing pin read: %d\n" $i
		var=$(printf "52%02x00" $i)
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
