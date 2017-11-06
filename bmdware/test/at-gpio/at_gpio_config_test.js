#!/usr/bin/env nodejs

var noble = require('noble');
var ble = require('../support/ble')
var utils = require('../support/utils')
var bmdware = require('../support/bmdware')
var bmdware_at = require('../support/bmdware_at')
var common = require('../support/common')
var async = require('async')
var commander = require('commander')
var serialPort = require('serialport')
var fs = require('fs')
var serial = require('../support/serial')

var testConfig
var testResult = 'FAIL'
var testNote = ''
var testShouldContinue = true
var testTimer
var test_status_count = 0

var expected_results = []
var results = []

var onDataCompleteCallback

var target_port
var setup_port

function atDataReceivecallback(data) {
    if(onDataCompleteCallback) {
        utils.log(5, "AT onDataCompleteCallback")
        var expected = expected_results.pop()
        if(expected != null) {
            utils.log(5, 'received data: ' + data)
            utils.log(5, 'expected: ' + expected)
            var result = false
            result = utils.compareBuffers(expected, data)
            if (!result) {
                for (var i=0; i < expected.length && i < data.length; i++) {
                    utils.log(5, 'expected[%d] = %d, data[%d] = %d', i, expected[i], i, data[i])
                }
            }
            results.push(result)
        } else {
            utils.log(2, "No expected result!")
        }
        onDataCompleteCallback()
    }
}

function testSetup(setupCompleteCallback) {
    if(!testShouldContinue) {
        return setupCompleteCallback()
    }

    common.init_at_mode(target_port, setupCompleteCallback)
    bmdware_at.configureAtReceiveNotifications(target_port, atDataReceivecallback)
}

function testGpioConfig(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }

    var hwConfig = JSON.parse(fs.readFileSync('hw_config.json', 'utf8'))
    hwConfig = hwConfig.nrf52
    async.series([
        function(callback) {
            utils.log(5, "Configure GPIO as output, no pull")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setGpioConfig(target_port, hwConfig.output_pins[0], bmdware_at.AT_DIRECTION_OUT, bmdware_at.AT_PULL_NONE)
        },
        function(callback) {
            utils.log(5, "Get configuration of output pin")
            expected_results.push(new Buffer(hwConfig.output_pins[0] + ' ' + bmdware_at.AT_DIRECTION_OUT + ' ' + bmdware_at.AT_PULL_NONE, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getGpioConfig(target_port, hwConfig.output_pins[0], null)
        },
        function(callback) {
            utils.log(5, "Configure GPIO input, no pull")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setGpioConfig(target_port, hwConfig.input_pins[0], bmdware_at.AT_DIRECTION_IN, bmdware_at.AT_PULL_NONE)
        },
        function(callback) {
            utils.log(5, "Get configuration of input pin")
            expected_results.push(new Buffer(hwConfig.input_pins[0] + ' ' + bmdware_at.AT_DIRECTION_IN + ' ' + bmdware_at.AT_PULL_NONE, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getGpioConfig(target_port, hwConfig.input_pins[0], null)
        },
        function(callback) {
            utils.log(5, "Set output pin high")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.writeGpio(target_port, hwConfig.output_pins[0], bmdware_at.AT_STATE_HIGH, null)
        },
        function(callback) {
            utils.log(5, "Read input pin state")
            expected_results.push(new Buffer(bmdware_at.AT_STATE_HIGH, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.readGpio(target_port, hwConfig.input_pins[0], null)
        },
        function(callback) {
            utils.log(5, "Set output pin low")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.writeGpio(target_port, hwConfig.output_pins[0], bmdware_at.AT_STATE_LOW, null)
        },
        function(callback) {
            utils.log(5, "Read input pin state")
            expected_results.push(new Buffer(bmdware_at.AT_STATE_LOW, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.readGpio(target_port, hwConfig.input_pins[0], null)
        },
        function(callback) {
            utils.log(5, "Configure GPIO as input, no pull")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setGpioConfig(target_port, hwConfig.output_pins[0], bmdware_at.AT_DIRECTION_IN, bmdware_at.AT_PULL_NONE)
        },
        function(callback) {
            utils.log(5, "Configure GPIO input, pull up")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setGpioConfig(target_port, hwConfig.input_pins[0], bmdware_at.AT_DIRECTION_IN, bmdware_at.AT_PULL_UP)
        },
        function(callback) {
            utils.log(5, "Get configuration of input pin")
            expected_results.push(new Buffer(hwConfig.input_pins[0] + ' ' + bmdware_at.AT_DIRECTION_IN + ' ' + bmdware_at.AT_PULL_UP, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getGpioConfig(target_port, hwConfig.input_pins[0], null)
        },
        function(callback) {
            utils.log(5, "Read input pin state")
            expected_results.push(new Buffer(bmdware_at.AT_STATE_HIGH, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.readGpio(target_port, hwConfig.input_pins[0], null)
        },
        function(callback) {
            utils.log(5, "Configure GPIO input, pull down")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setGpioConfig(target_port, hwConfig.input_pins[0], bmdware_at.AT_DIRECTION_IN, bmdware_at.AT_PULL_DOWN)
        },
        function(callback) {
            utils.log(5, "Get configuration of input pin")
            expected_results.push(new Buffer(hwConfig.input_pins[0] + ' ' + bmdware_at.AT_DIRECTION_IN + ' ' + bmdware_at.AT_PULL_DOWN, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getGpioConfig(target_port, hwConfig.input_pins[0], null)
        },
        function(callback) {
            utils.log(5, "Read input pin state")
            expected_results.push(new Buffer(bmdware_at.AT_STATE_LOW, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.readGpio(target_port, hwConfig.input_pins[0], null)
        },
        function(callback) {
            testResult = 'PASS'
            for (i = 0; i < results.length; i++) {
                if(results.pop() != true) {
                    testResult = 'FAIL'
                    break
                }
            }
            utils.log(5, "Test : " + testResult)

            utils.log(5, "AT GPIO Config test done")
            if(testCompleteCallback) {
                testCompleteCallback()
            }
        } 
    ]);
}

function testTearDown(tearDownCompleteCallback) {
    if(!testShouldContinue) {
        tearDownCompleteCallback()
        return
    }

    async.series([
        function(callback) {
            onDataCompleteCallback = callback
            bmdware_at.resetDefaultConfiguration(target_port, null)
        },
        function(callback) {
            target_port.close()
            utils.log(5, "TearDown done")
            if(tearDownCompleteCallback) {
                tearDownCompleteCallback()
            }
        }
    ]);
}

function testRunner(testCompleteCallback) {
    ble.loadConfiguration('test_config.json')
    testConfig = ble.getConfiguration()

    if (testConfig.target_uart != "") {
        target_uart = testConfig.target_uart
    }
    if (testConfig.setup_uart != "") {
        setup_uart = testConfig.setup_uart
    }
    if (testConfig.baudrate != "") {
        baudrate =  parseInt(testConfig.baudrate)
    }

    async.series([
        function(callback) {
            target_port = serial.open(target_uart, {
                baudrate: baudrate,
                // look for return and newline at the end of each data packet:
                parser: serialPort.parsers.readline("\n")
            }, callback)
        },
        function(callback) {
            testSetup(callback)
        },
        function(callback) {
            testGpioConfig(callback)
        },
        function(callback, peripheral) {
            testTearDown(callback)
        },
        function(callback) {
            utils.log(5, "Test Complete")
            if(testCompleteCallback) {
                testCompleteCallback(testResult, testNote)  
            } else {
                process.exit(0)
            }
        }
    ])
}

function getName() {
        return 'AT GPIO Config Test'
    }

//run all tests
module.exports = {
    testRunner: testRunner,
    getName: getName
}

commander.
    version('1.0.0').
    usage('[options]').
    option('-r, --run', 'Run as stand alone test').
    parse(process.argv);

if(commander.run) {
    utils.log(1, "Running test: " + getName())
    testRunner(function(testResult, testNote) {
        utils.log(1, "Test Result: " + testResult)
        if(testNote.length > 0) {
            utils.log(1, "Test Note: " + testNote)
        }
        process.exit(0)
    }) 
}
