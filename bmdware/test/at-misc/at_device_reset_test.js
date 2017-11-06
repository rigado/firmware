#!/usr/bin/env nodejs

var noble = require('noble');
var ble = require('../support/ble')
var utils = require('../support/utils')
var bmdware = require('../support/bmdware')
var bmdware_at = require('../support/bmdware_at')
var common = require('../support/common')
var serial = require('../support/serial')
var async = require('async')
var commander = require('commander')
var SerialPort = require("serialport")

var callbackAfterBeaconDiscovery
var beaconUnderTestId

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

function onData(data, isNotification) {
    utils.log(5, "Returned Code error: %d", data[0])
    utils.log(5, "Returned Code String: " + bmdware.returnCodeStr[data[0]])
    utils.log(1, "data = " + data.toString('hex'))

    if(onDataCompleteCallback) {
        utils.log(5, "onDataCompleteCallback")
        var expected = expected_results.pop()
        if(expected != null) {
            var result = false
            if (data.length > expected.length) {
                var actual = new Buffer(data.slice(0, expected.length))
                result = utils.compareBuffers(expected, actual)
            }
            else {
                result = utils.compareBuffers(expected, data)
            }
            results.push(result)
        }
        onDataCompleteCallback()
    } else {
        utils.log(5, 'onDataCompleteCallback is null?!?!')
    }
}

function testAtDeviceReset(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }
    async.series([
        function(callback) {
            utils.log(5, "AT Command Device Reset: configure gpio")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setGpioConfig(target_port, bmdware_at.AT_P0_04, bmdware_at.AT_DIRECTION_OUT, bmdware_at.AT_PULL_UP, null)
        },
        function(callback) {
            utils.log(5, "AT Command Device Reset: get configuration")
            expected_results.push(new Buffer(bmdware_at.AT_P0_04 + ' ' + bmdware_at.AT_DIRECTION_OUT + ' ' + bmdware_at.AT_PULL_UP, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getGpioConfig(target_port, bmdware_at.AT_P0_04, null)
        },
        function(callback) {
            utils.log(5, "AT Command Device Reset: devrst")
            // wait for 3 seconds after starting the boot loader
            utils.log(5, "Wait for bootloader...")
            onDataCompleteCallback = null
            bmdware_at.reset(target_port, null)
            setTimeout(function() {
                callback()
            }, 3000)
        },
        function(callback) {
            utils.log(5, "AT Command Device Reset: check for default config")
            expected_results.push(new Buffer(bmdware_at.AT_P0_04 + ' ' + bmdware_at.AT_DIRECTION_IN + ' ' + bmdware_at.AT_PULL_NONE, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getGpioConfig(target_port, bmdware_at.AT_P0_04, null)
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

            utils.log(5, "AT command test done")
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
                parser: SerialPort.parsers.readline("\n")
            }, callback)
        },
        function(callback) {
            testSetup(callback)
        },
        function(callback) {
            testAtDeviceReset(callback)
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
        return 'AT Device Reset Test'
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
