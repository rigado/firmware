#!/usr/bin/env nodejs

var ble = require('../support/ble')
var utils = require('../support/utils')
var bmdware = require('../support/bmdware')
var bmdware_at = require('../support/bmdware_at')
var common = require('../support/common')
var serial = require('../support/serial')
var async = require('async')
var commander = require('commander')
var SerialPort = require("serialport")

var testConfig
var testResult = 'FAIL'
var testNote = ''
var testShouldContinue = true
var testTimer
var test_status_count = 0

var expected_results = []
var results = []

var onDataCompleteCallback
var macValidationCallback

var target_port
var setup_port

function atDataReceivecallback(data) {
    if(onDataCompleteCallback) {
        // validate the mac?
        if(macValidationCallback)
        {
            macValidationCallback(data)
        }
        else
        {
            // compare with expected
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
        }

        onDataCompleteCallback()
    }
}

function CheckMacFormatCallback(data) {
    utils.log(5, "AT MAC Validation")
    utils.log(5, 'received data: ' + data)
    var re = /^([0-9A-F]{2}:){5}[0-9A-F]{2}$/
    var match = re.exec(data)
    var result = (match != null)

    results.push(result)
    utils.log(5, 'regex match: ' + result)
}
 
function testSetup(setupCompleteCallback) {	
    if(!testShouldContinue) {
        return setupCompleteCallback()
    }

    common.init_at_mode(target_port, setupCompleteCallback)
    bmdware_at.configureAtReceiveNotifications(target_port, atDataReceivecallback)
}

function testAtMac(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }
    async.series([
        function(callback) {
            utils.log(5, "AT Command MAC: get MAC address")
            
            if(testConfig.device_mac != "")
            {
                expected_results.push(new Buffer(testConfig.device_mac, 'ascii'))    
            }
            else
            {
                utils.log(5,"warning: no device mac specified in config, only checking format...")
                macValidationCallback = CheckMacFormatCallback
            }
            
            onDataCompleteCallback = callback
            bmdware_at.getMac(target_port, null)
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
            testAtMac(callback)
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
        return 'AT MAC Test'
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
