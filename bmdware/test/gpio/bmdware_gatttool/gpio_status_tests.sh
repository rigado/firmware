#!/bin/bash
#-------------------------------------------------------------------------------
#  File: gpio_status_tests.sh
#  Description:	BMDware BLE control port test
#				Tests status commands
#				set status configuration 0x54
#				deconfigure 0x55
#				get status configuration 0x56
#				read status pin 0x57
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
#	Set all pins to status
#-------------------------------------------------------------------------------
	for i in {0..22}
	do
		printf "Testing pin (output): %d\n" $i
		var=$(printf "54%02x00" $i)
		./test_wr.sh $device $var
		./test_wr.sh $device 56
		./test_wr.sh $device 55

	done

	printf "Set status configuration to P0.17 (LED1)\n"
	./test_wr.sh $device 540900

	printf "Get status configuration\n"
	./test_wr.sh $device 56

	printf "Read status pin\n"
	./test_wr.sh $device 57

	printf "De-configure status pin\n"
	./test_wr.sh $device 55

#-------------------------------------------------------------------------------
#	Reset all valid pins to inputs (no pull)
#-------------------------------------------------------------------------------
	for i in {0..22}
	do
		printf "reset to input: %d\n" $i
		var=$(printf "50%02x0000" $i)
		./test_wr.sh $device $var
	done
