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
var serialPort = require("serialport")
var SerialPort = serialPort.SerialPort

var callbackAfterBeaconDiscovery
var beaconUnderTestId

var testConfig
var testResult = 'FAIL'
var testNote = ''
var testShouldContinue = true
var testTimer
var test_status_count = 0

var test_password = 'th1sisap@$$w0rd'

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
    
    //common.configure('nrf52', bmdware_at.AT_STATE_LOW, bmdware_at.AT_STATE_HIGH, setupCompleteCallback)
}

function testLockUnlock(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }
    async.series([
        function(callback) {
            utils.log(5, "* Set Default settings")
            expected_results.push(new Buffer(bmdware_at.RC_OK, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.resetDefaultConfiguration(target_port, null)
        },
        function(callback) {
            utils.log(5, "* Set Password")
            expected_results.push(new Buffer(bmdware_at.RC_OK, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setPassword(target_port, test_password, null)
        },
        function(callback) {
            utils.log(5, "* Set BaudRate (locked)")
            expected_results.push(new Buffer(bmdware_at.RC_LOCKED, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setUartBaudRate(target_port, '115200', null)
        },
        function(callback) {
            utils.log(5, "* Unlock")
            expected_results.push(new Buffer(bmdware_at.RC_OK, 'ascii'))
            onDataCompleteCallback = callback 
            bmdware_at.unlockDevice(target_port, test_password, null)
        },
        function(callback) {
            utils.log(5, "* Set BaudRate Again")
            expected_results.push(new Buffer(bmdware_at.RC_OK, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setUartBaudRate(target_port, '115200', null)
        },
        function(callback) {
            utils.log(5, "* Set New BaudRate (locked)")
            expected_results.push(new Buffer(bmdware_at.RC_LOCKED, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setUartBaudRate(target_port, '38400', null)
        },
        //Check to ensure everything can be read while locked
        function(callback) {
            utils.log(5, "* Get BaudRate")
            expected_results.push(new Buffer('0001c200', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getUartBaudRate(target_port, null)
        },
        function(callback) {
            utils.log(5, "* Get Flow Control")
            expected_results.push(new Buffer('00', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getUartFlowControlEnable(target_port, null)
        },
        function(callback) {
            utils.log(5, "* Get UART Enable")
            expected_results.push(new Buffer('00', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getUartEnable(target_port, null)
        },
        function(callback) {
            utils.log(5, "* Get Parity")
            expected_results.push(new Buffer('00', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getUartParityEnable(target_port, null)
        },
        function(callback) {
            utils.log(5, "* Get Beacon Major")
            expected_results.push(new Buffer('0000', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getBeaconMajor(target_port, null)
        },
        function(callback) {
            utils.log(5, "* Get Beacon Minor")
            expected_results.push(new Buffer('0000', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getBeaconMinor(target_port, null)
        },
        function(callback) {
            utils.log(5, "* Get Beacon Uuiid")
            expected_results.push(new Buffer('00112233445566778899AABBCCDDEEFF', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getBeaconUuid(target_port, null)
        },
        function(callback) {
            utils.log(5, "* Get Custom Beacon")
            expected_results.push(new Buffer('', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getCustomBeaconData(target_port, null)
        },
        function(callback) {
            utils.log(5, "* Get Beacon TX Power")
            expected_results.push(new Buffer('fc', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getBeaconTxPower(target_port, null)
        },
        function(callback) {
            utils.log(5, "* Get Beacon Adv interval")
            expected_results.push(new Buffer('0064', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getBeaconAdvertisingInterval(target_port, null)
        },
        function(callback) {
            utils.log(5, "* Get Connectable TX Power")
            expected_results.push(new Buffer('fc', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getConnectableTxPower(target_port, null)
        },
        function(callback) {
            utils.log(5, "* Get Beacon Enable")
            expected_results.push(new Buffer('00', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getBeaconEnable(target_port, null)
        },
        function(callback) {
            utils.log(5, "* Get Beacon Calibration")
            expected_results.push(new Buffer('fc c1', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.getRssiCalibrationData(target_port, null)
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

            utils.log(5, "AT Lock/Unlock test done")
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
            testLockUnlock(callback)
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
        return 'AT Lock Unlock Test'
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
