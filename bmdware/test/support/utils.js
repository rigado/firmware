#!/usr/bin/env nodejs

var logLevel = 9

function log(level, msg) {
	if(level > logLevel) {
		return
	}

	var out = []
	for(var i = 1; i < arguments.length; i++) {
		out[i-1] = arguments[i]
	}

	//process.stdout.write(func + ": ")
	console.log.apply(null, out)
}

function setLogLevel(level) {
	logLevel = level
}

function checkError(error) {
    if (error !== null && error !== undefined) {
		log(1, "Error: %s", error);
		return false
    }
    return true
}

function hexStringToBytes(input) {
	if(!input) {
		log(1, "Invalid input to hexStringToBytes!")
		process.exit(1)
	}

	var buf = new Buffer(16)
	for(var i = 0; i < input.length; i += 2) {
		var val = parseInt('0x' + input[i] + input[i+1])
		buf.writeUInt8(val, (i / 2))
	}

	return buf
}

function hexStringFromInt(input, pad) {
	var hexString = input.toString(16)
	if(pad == true) {
		var padding = 4 - hexString.length
		for(var i = 0; i < padding; i++) {
			hexString = '0' + hexString
		}
	}
	return hexString
}

function bytesToHexString(input) {
	if(!input) {
		log(1, "Invalid input to bytesToHexString")
		process.exit(1)
	}

	var lookup = [ '0', '1', '2', '3', '4', '5', '6', '7', 
				   '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' ]

	var out = ""
	for(var i = 0; i < input.length; i++)
	{
		var high = input[i] >> 4
		var low = input[i] & 0x0F
		out = out + lookup[high] + lookup[low]
	}

	return out
}

function compareBuffers(a, b) {
	if(a.length != b.length) {
		log(5, 'buffer lengths not equal; a: ' + a.length + ' b:' + b.length)
		return false
	}

	for(var i = 0; i < a.length; i++) {
		if(a[i] != b[i]) {
			log(5, 'buffer compare failure; i:' + i + ' a:' + a[i] + ' b:' + b[i])
			return false
		}
	}

	return true
}

function delay(delay_ms, callback)
{
	var delay_var = setTimeout(function() {
                delay_var = null;
                callback()
            }, delay_ms)
}

module.exports = {
	log: log,
	setLogLevel: setLogLevel,
	checkError: checkError,
	hexStringToBytes: hexStringToBytes,
	bytesToHexString: bytesToHexString,
	hexStringFromInt: hexStringFromInt,
	compareBuffers: compareBuffers,
	delay: delay
}
