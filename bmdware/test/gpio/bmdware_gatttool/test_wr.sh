#!/bin/bash
#-------------------------------------------------------------------------------
#	file test_wr.sh
#	write values to specified gpio characteristics
#
#	script parameters:
#	$1 - serial number
#	$2 - value to write
#
#-------------------------------------------------------------------------------

	rb_sn=$1
	rb_val=$2

	gatttool --device=$rb_sn --char-write-req --handle=0x0010 --value=$rb_val
	gatttool --device=$rb_sn --char-read --handle=0x0010
