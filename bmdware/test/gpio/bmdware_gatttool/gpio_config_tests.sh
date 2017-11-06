#!/bin/bash
#-------------------------------------------------------------------------------
#  File: gpio_config_test.sh
#  Description:	BMDware BLE control port test
#				Tests all possible values for each parameter in gpio config
#				command (0x50)
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
#	Set all possible pins to outputs
#-------------------------------------------------------------------------------
	for i in {0..255}
	do
		printf "Testing pin (output): %d\n" $i
		var=$(printf "50%02x0100" $i)
		./test_wr.sh $device $var
	done

#-------------------------------------------------------------------------------
#	Test all possible pin status
#-------------------------------------------------------------------------------
	for i in {0..255}
	do
		printf "Testing pin status: %d\n" $i
		var=$(printf "53%02x" $i)
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

#-------------------------------------------------------------------------------
#	Set all possible pins states
#-------------------------------------------------------------------------------
	for i in {0..255}
	do
		printf "Testing pin (output): %d\n" $i
		var=$(printf "5000%02x00" $i)
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

#-------------------------------------------------------------------------------
#	Set all possible pullup states
#-------------------------------------------------------------------------------
	for i in {0..255}
	do
		printf "Testing pin (output): %d\n" $i
		var=$(printf "500000%02x" $i)
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
