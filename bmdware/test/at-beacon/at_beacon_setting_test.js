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
var test_beacon_uuid = "6d224c46ef864b4692090f559ec3adb4"
var test_major = '9876'
var test_minor = '5234'
var test_rssi = 'e3'

var expected_results = []
var results = []

var onDataCompleteCallback
var expectedData

var target_port
var setup_port

function atDataReceivecallback(data) {
    if(onDataCompleteCallback) {
        utils.log(5, "atDataReceivecallback: " + data)
        var expected = expected_results.pop()
        if(expected != null) {
            receiveData = new Buffer(data, 'ascii')
            utils.log(5, 'received data: ' + receiveData)
            utils.log(5, 'expected: ' + expected)
            var result = false
            result = utils.compareBuffers(expected, receiveData)
            if (!result) {
                for (var i=0; i < expected.length && i < receiveData.length; i++) {
                    utils.log(5, 'expected[%d] = %d, data[%d] = %d', i, expected[i], i, receiveData[i])
                }
            }
            results.push(result)
        }
        onDataCompleteCallback()
    } else {
        // utils.log(5, 'onDataCompleteCallback is null?!?!')
    }
}

function startScanBeacon(callback) {
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
    noble.startScanning([], true)
}

function onDiscoverBeacon(peripheral) {
    var adv = peripheral.advertisement
    var mfg_data = adv.manufacturerData

    if(mfg_data == null) {
        return
    }

    // check that the beacon shows iBeacon style broadcast
    if (mfg_data.length > 0 && mfg_data[0] == 0x4C && mfg_data[1] == 0x00) {
        var matchData = '4c000215' + test_beacon_uuid + test_major + test_minor + test_rssi
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
            noble.removeListener('discover', onDiscoverBeacon)
            noble.stopScanning()
            callbackAfterBeaconDiscovery(true)
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

function testBeaconSetting(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }
    async.series([
        function(callback) {
            utils.log(5, "Reset to defaults")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.resetDefaultConfiguration(target_port, null)
        },
        function(callback) {
            utils.log(5, "AT Command beacon uuid: " + test_beacon_uuid)
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setBeaconUuid(target_port, test_beacon_uuid, null)
        },
        function(callback) {
            utils.log(5, "AT Command beacon major: " + test_major)
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setBeaconMajor(target_port, test_major, null)
        },
        function(callback) {
            utils.log(5, "AT Command beacon minor: " + test_minor)
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setBeaconMinor(target_port, test_minor, null)
        },
        function(callback) {
            utils.log(5, "AT Command beacon adv Interval: 70")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setBeaconAdvertisingInterval(target_port, "70", null)
        },
        function(callback) {
            utils.log(5, "AT Command beacon Calibration: fc e3")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setRssiCalibrationData(target_port, 'fc', test_rssi, null)
        },
        function(callback) {
            utils.log(5, "AT Command beacon enable")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setBeaconEnable(target_port, bmdware_at.AT_STATE_HIGH, null)
        },
        function(callback) {
            utils.log(5, "Beacon scanning")
            onDataCompleteCallback = null
            startScanBeacon(function(scanResult) {
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

            utils.log(5, "AT Beacon Setting test done")
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
            target_port = new SerialPort(testConfig.target_uart, 
                                        {
                                            baudrate: baudrate,
                                            autoOpen: true,
                                            // look for return and newline at the end of each data packet:
                                            parser: SerialPort.parsers.readline("\n")
                                        },
                                        function(){} )
            target_port.open(function(err) {
                utils.checkError(err)

                callback()
            })
        },
        function(callback) {
            testSetup(callback)
        },
        function(callback) {
            testBeaconSetting(callback)
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
        return 'AT Beacon Setting Test'
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
