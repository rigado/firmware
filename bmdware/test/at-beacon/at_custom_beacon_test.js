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


var callbackAfterCustomBeaconDiscovery
var beaconUnderTestId

var testConfig
var testResult = 'FAIL'
var testNote = ''
var testShouldContinue = true
var testTimer
var initialscan = true

var customBeaconData = '0303fed81a16fed800112233445566778899aabbccddeeff0123456789abcd'
var test_data = '00112233445566778899aabbccddeeff0123456789abcd'
var test_service = 'd8fe'

var test_uuid = '0303fed81a16fed80011223344556677'

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

function startScanCustomBeacon(callback) {
    callbackAfterCustomBeaconDiscovery = callback
    // Add a time out to ensure test will eventually fail if beacon not found
    testTimer = setTimeout(function() {
        noble.removeListener('discover', onDiscoverCPBeacon)
        noble.stopScanning()
        testTimer = null;
        testResult = 'FAIL'
        testNode = 'Failed to discover beacon with correct data'
        utils.log(5,  "Time Out")
        callbackAfterCustomBeaconDiscovery(false)
    }, 50000)
    noble.on('discover', onDiscoverCPBeacon)
    noble.startScanning([], true)
}

function onDiscoverCPBeacon(peripheral) {
    if(initialscan) {
        var adv = peripheral.advertisement
        var serviceData = adv.serviceData
        var test_status_count = 0
        if (serviceData && serviceData.length) {
            for (var i in serviceData) {
                utils.log(5,  "serviceData.uuid = " + JSON.stringify(serviceData[i].uuid))
                utils.log(5,  "serviceData.data = " + JSON.stringify(serviceData[i].data.toString('hex')))
                var cpUuid = serviceData[i].uuid
                var cpData = serviceData[i].data
                var matchData = test_data
                if(cpUuid == test_service && cpData.toString('hex') == matchData) {
                    utils.log(5, "Found custom beacon uuid = " + JSON.stringify(cpUuid))
                    clearTimeout(testTimer)
                    testTimer = null
                    results.push(true)
                    noble.removeListener('discover', onDiscoverCPBeacon)
                    noble.stopScanning()
                    initialscan = false
                    callbackAfterCustomBeaconDiscovery(true)
                }
            }
        }
    } else {
        var adv = peripheral.advertisement
        var mfg_data = adv.manufacturerData

        if(mfg_data == null) {
            return
        }

        // check that the beacon shows iBeacon style broadcast
        if (mfg_data.length > 0 && mfg_data[0] == 0x4C && mfg_data[1] == 0x00) {
            var matchData = '4c000215' + test_uuid
            var beaconData = utils.bytesToHexString(mfg_data)
            utils.log(5, "matchData  = " + matchData)
            utils.log(5, "beaconData = " + beaconData)

            if(beaconData.indexOf(matchData) >= 0) {
                utils.log(5, "data match")
                var result = true
                results.push(result)
                clearTimeout(testTimer)
                testTimer = null
                // Once we discover one BMDWARE device, stop looking.
                noble.removeListener('discover', onDiscoverCPBeacon)
                noble.stopScanning()
                callbackAfterCustomBeaconDiscovery(true)
            }
        }
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

function testCustomBeacon(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }
    async.series([
        function(callback) {
            utils.log(5, "AT Command Set default: at$defaults")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.resetDefaultConfiguration(target_port, null)
        },
        function(callback) {
            utils.log(5, "AT Command custom beacon uuid")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setCustomBeaconData(target_port, customBeaconData, null)
        },
        function(callback) {
            utils.log(5, "AT Command beacon enable: at$ben 01")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setBeaconEnable(target_port, true, null)
        },
        function(callback) {
            utils.log(5, "Scan for custom beacon")
            startScanCustomBeacon(function(scanResult) {
                if(!scanResult) {
                    utils.log(5, 'Failed to discover beacon with appropriate data')
                    results.push(scanResult)
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Clear custom beacon data")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.clearCustomBeaconData(target_port, null)
        },
        function(callback) {
            utils.log(5, "Set beacon uuid")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setBeaconUuid(target_port, test_uuid, null)
        },
        function(callback) {
            utils.log(5, "Scan for iBeacon")
            startScanCustomBeacon(function(scanResult) {
                if(!scanResult) {
                    utils.log(5, 'Failed to discover beacon with appropriate data')
                    results.push(scanResult)
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

            utils.log(5, "AT Custom Beacon test done")
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
                parser: serialPort.parsers.readline("\n")
            }, callback)
        },
        function(callback) {
            testSetup(callback)
        },
        function(callback) {
            testCustomBeacon(callback)
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
        return 'AT Custom Beacon Test'
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
