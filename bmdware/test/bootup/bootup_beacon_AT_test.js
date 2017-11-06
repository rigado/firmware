#!/usr/bin/env nodejs

var noble = require('noble');
var ble = require('../support/ble')
var utils = require('../support/utils')
var bmdware = require('../support/bmdware')
var bmdware_at = require('../support/bmdware_at')
var common = require('../support/common')
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
var test_beacon_uuid = "6d224c46ef864b4692090f559ec3adb4"

var expected_results = []
var results = []

var onDataCompleteCallback

var target_uart = '/dev/ttyACM0'
var setup_uart = '/dev/ttyACM1'
const baudrate = 57600

var target_port
var setup_port

var onAtDataCompleteCallback

function onAtDataReceived(data, isNotification) {
    if(onAtDataCompleteCallback) {
        utils.log(5, "AT onDataCompleteCallback")
        clearTimeout(bmdware_at.uartTimer)
        bmdware_at.uartTimer = null;

        var expected = expected_results.pop()
        if(expected != null) {
            var receiveData = new Buffer(data, 'ascii')
            utils.log(5, 'received data: ' + receiveData)
            utils.log(5, 'expected: ' + expected + '\n')
            var result = false
            result = utils.compareBuffers(expected, receiveData)
            if (!result) {
                for (var i=0; i < expected.length && i < receiveData.length; i++) {
                    utils.log(5, 'expected[%d] = %d, receiveData[%d] = %d', i, expected[i], i, receiveData[i])
                }
            }
            results.push(result)
        }
        onAtDataCompleteCallback()
        if (bmdware_at.callbackAfterDataReceive) return bmdware_at.callbackAfterDataReceive(true)
    }
    else {
        utils.log(5, 'onAtDataCompleteCallback is null?!?!')
    }
}

function scanServiceUuids(callback) {
    callbackAfterBeaconDiscovery = callback
    // Add a time out to ensure test will eventually fail if beacon not found
    testTimer = setTimeout(function() {
        noble.removeListener('discover', onDiscoverBeacon)
        noble.stopScanning()
        testTimer = null;
        testResult = 'FAIL'
        testNode = 'Failed to discover beacon with correct data'
        utils.log(5,  "Time Out")
        callbackAfterBeaconDiscovery(false)
    }, 50000)
    noble.on('discover', onDiscoverBeacon)
    noble.startScanning(bmdware.getBmdwareServiceUuids(), true)
}

function onDiscoverBeacon(peripheral) {
    var adv = peripheral.advertisement
    var serviceUuids = adv.serviceUuids

    var beaconServiceFound = false
    var uartServiceFound   = false

    var bmdwareServiceUuids = bmdware.getBmdwareServiceUuids()

    utils.log(5,  "peripheral.uuid = " + JSON.stringify(peripheral.uuid))
    utils.log(5,  "adv = " + JSON.stringify(adv))
    if (serviceUuids && serviceUuids.length) {
        for (var i in serviceUuids) {
            utils.log(5,  "serviceUuid       = " + JSON.stringify(serviceUuids[i]))
            utils.log(5,  "beaconServiceUuid = " + JSON.stringify(bmdwareServiceUuids[0]))
            utils.log(5,  "uartServiceUuid   = " + JSON.stringify(bmdwareServiceUuids[1]))
            if(serviceUuids[i] == bmdwareServiceUuids[0]) {
                beaconServiceFound = true
            }
            else if (serviceUuids[i] == bmdwareServiceUuids[1]) {
                uartServiceFound = true
            }
        }
    }
    else {
        return
    }
    clearTimeout(testTimer)
    testTimer = null

    if (beaconServiceFound && !uartServiceFound) {
        results.push(true)
    }
    else
        results.push(false)

    noble.removeListener('discover', onDiscoverBeacon)
    noble.stopScanning()
    callbackAfterBeaconDiscovery(true)
}

function testSetup(setupCompleteCallback) {
    if(!testShouldContinue) {
        return setupCompleteCallback()
    }
    return common.setupMode(setup_port, bmdware_at.AT_STATE_LOW,
               bmdware_at.AT_STATE_LOW, setupCompleteCallback)
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

function testBeaconATMode(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }
    async.series([
        function(callback) {
            utils.log(5, "Scanning Service Uuids to verify that only beacon service is broadcast")
            scanServiceUuids(function(scanResult) {
                if(!scanResult) {
                    utils.log(5, 'Failed to discover beacon with appropriate data')
                    results.push(scanResult)
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, 'open Uart target port for read notification')
            bmdware_at.configureAtReceiveNotifications(target_port, onAtDataReceived, callback)
        },
        function(callback) {
            utils.log(5, "Send AT to verify AT mode is enable and response OK")
            expected_results.push(new Buffer('OK', 'ascii'))
            onAtDataCompleteCallback = callback
            bmdware_at.writeAtCommandTimeOut(target_port, "at\n", 3000, function(status) {
                if(!status) {
                    utils.log(5, 'Error timeout return')
                    results.push(false)
                }
                callback()
            })
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

            utils.log(5, "Bootup Config Beacon only test done")
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
            onDataCompleteCallback = null
            utils.log(5, "TearDown connect")
            ble.connectPeripheralUT(function(connectResult) {
                if(!connectResult) {
                    utils.log(2, 'Failed to connect to device for teardown')
                    tearDownCompleteCallback()
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Notification enable")
            bmdware.configureBleReceiveNotifications(onData, callback)
        },
        function(callback) {
            utils.log(5, "Reset Default Configuration")
            onDataCompleteCallback = callback
            bmdware.resetDefaultConfiguration(null)
        },
        function(callback) {
            utils.log(5, "TearDown disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    utils.log(2, 'Failed to disconnect after tear down!')
                }
                callback()
            })
        },
        function(callback) {
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

    target_port = new SerialPort(target_uart, {
        baudrate: baudrate,
        // look for return and newline at the end of each data packet:
        parser: serialPort.parsers.readline("\n")
    }, false)

    setup_port = new SerialPort(setup_uart, {
        baudrate: baudrate,
        // look for return and newline at the end of each data packet:
        parser: serialPort.parsers.readline("\n")
    }, false)

    async.series([
        function(callback) {
            testSetup(callback)
        },
        function(callback) {
            testBeaconATMode(callback)
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
        return 'Bootup Beacon Only with AT Mode Test'
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
