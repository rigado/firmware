var utils = require('./utils')
var serialport = require('serialport')
var SerialPort = serialport.SerialPort

function open(port, options, callback) {
	var serial = new SerialPort(port, options)
	serial.on('open', function(err) {
		callback(err)
	})
	return serial
}

function send(port, data, callback) {
	port.write(data, function(err) {
		utils.checkError(err)
		callback()
	})
}

module.exports = {
	open: open,
	send: send
}

