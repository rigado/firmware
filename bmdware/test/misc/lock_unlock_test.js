#!/usr/bin/env nodejs

var noble = require('noble');
var ble = require('../support/ble')
var utils = require('../support/utils')
var bmdware = require('../support/bmdware')
var async = require('async')
var commander = require('commander')

var callbackAfterServiceDiscovery
var callbackAfterBeaconDiscovery
var beaconUnderTestId

var testConfig
var testResult = 'FAIL'
var testNote = ''
var testShouldContinue = true
var testTimer
var test_status_count = 0
var test_beacon_uuid = "00112233445566778899aabbccddeeff"
var test_major = "5489"

var expected_results = []
var results = []

var onDataCompleteCallback

function startScanService(callback) {
    callbackAfterServiceDiscovery = callback
    var peripheral = ble.getPeripheralUT()
    if(peripheral) {
        beaconUnderTestId = peripheral.uuid
    } else {
        utils.log(1, 'Beacon peripheral not found!')
        process.exit(1)
    }
    
    // Add a time out to ensure test will eventually fail if beacon not found
    testTimer = setTimeout(function() {
        noble.removeListener('discover', onDiscoverService)
        noble.stopScanning()
        testTimer = null;
        testResult = 'FAIL'
        testNode = 'Failed to discover beacon with correct data'
        callbackAfterServiceDiscovery(false)        
    }, 50000)

    noble.on('discover', onDiscoverService)
    noble.startScanning(bmdware.getBmdwareServiceUuids(), true)
}

function onDiscoverService(peripheral) {
    var adv = peripheral.advertisement

    if(peripheral.uuid != beaconUnderTestId) {
        return
    }

    utils.log(5, 'peripheral.uuid = ' + peripheral.uuid)
    //utils.log(5, 'serviceUuids = ' + JSON.stringify(adv.serviceUuids))

    // Once we discover one BMDWARE device, stop looking.
    noble.removeListener('discover', onDiscoverService)
    noble.stopScanning()

    callbackAfterServiceDiscovery(true)
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
        callbackAfterBeaconDiscovery(false)        
    }, 50000)

    noble.on('discover', onDiscoverBeacon)
    noble.startScanning([], true)
}

function onDiscoverBeacon(peripheral) {
    var adv = peripheral.advertisement
    var mfg_data = adv.manufacturerData

    if(mfg_data == null || peripheral.rssi < -65) {
        return
    }
    var majorHex = utils.hexStringFromInt(parseInt(test_major), true)
    var matchData = "4c000215" + test_beacon_uuid + majorHex
    var beaconData = utils.bytesToHexString(mfg_data)
    utils.log(5, "matchData = " + matchData) 
    utils.log(5, "beaconData = " + beaconData) 

    if(beaconData.indexOf(matchData) >= 0) {
        var result = true
        results.push(result)
        // Once we discover one BMDWARE device, stop looking.
        noble.removeListener('discover', onDiscoverBeacon)
        noble.stopScanning()
        callbackAfterBeaconDiscovery(true)
    }
}

function testSetup(setupCompleteCallback) {	
    async.series([
        function(callback) {
            ble.findTestDevice(bmdware.getBmdwareServiceUuids(), function(deviceFound) {
                if(!deviceFound) {
                    testNote = 'Could not find a BMDware test device!'
                    testShouldContinue = false
                    setupCompleteCallback()
                }
                callback()
            })
        },
        function(callback) {
            if(setupCompleteCallback) {
                setupCompleteCallback()
            }
        }
    ]);
}

function onData(data, isNotification) {
    utils.log(5, "Returned Code error: %d", data[0])
    utils.log(5, "Returned Code String: " + bmdware.returnCodeStr[data[0]])
    utils.log(5, "data = " + data.toString('hex'))

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

function testLockUnlock(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }
    async.series([
        function(callback) {
            utils.log(5, "LockUnlock test connect")
            ble.connectPeripheralUT(function(connectResult) {
                if(!connectResult) {
                    testNote = 'Failed to connect to device!'
                    testShouldContinue = false
                    testCompleteCallback()
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
            expected_results.push(new Buffer([bmdware.RC_SUCCESS]))
            bmdware.resetDefaultConfiguration(null)
        },
        function(callback) {
            utils.log(5, "Set Password to 'test' to lock the device")
            var data = new Buffer('74657374', 'hex')
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_SUCCESS]))
            bmdware.setPassword(data, null)
        },
        function(callback) {
            utils.log(5, "LockUnlock test disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Failed to disconnect'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "LockUnlock test start scanning")
            startScanService(function(scanResult) {
                if(!scanResult) {
                    utils.log(5, 'Failed to discover Service with appropriate data')
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "LockUnlock test reconnect")
            ble.connectPeripheralUT(function(connectResult) {
                if(!connectResult) {
                    testNote = 'Failed to connect to device!'
                    testShouldContinue = false
                    testCompleteCallback()
                }
                callback()
            })
        },

        function(callback) {
            utils.log(5, "Notification enable")
            bmdware.configureBleReceiveNotifications(onData, callback)
        },
        // changing the uuid will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "beacon uuid set")
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_LOCKED]))
            bmdware.setBeaconUuid(test_beacon_uuid, null)
        },
        // changing the CP beacon data1 will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "Set Custom Beacon Data1 to 1234")
            var data = new Buffer('1234', 'hex')
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_LOCKED]))
            bmdware.setCustomBeaconData1(data, null)
        },
        // changing the CP beacon data2 will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "Set Custom Beacon Data2")
            var data = new Buffer('1234', 'hex')
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_LOCKED]))
            bmdware.setCustomBeaconData2(data, null)
        },
        // Save the CP beacon data will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "Save Custom Beacon Data")
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_LOCKED]))
            bmdware.saveCustomBeaconData(null)
        },
        // Clear the CP beacon data will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "Clear Custom Beacon Data")
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_LOCKED]))
            bmdware.clearCustomBeaconData(null)
        },
        // Set RSSI Calibration will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "Set RSSI Calibration")
            var txPowerValue = 0xfc
            var rssiValue = 0xc0
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_LOCKED]))
            bmdware.setRssiCalibrationData(txPowerValue, rssiValue, null)
        },
        // Get RSSI Calibration will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "Get RSSI Calibration")
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_LOCKED]))
            bmdware.getRssiCalibrationData(null)
        },
        // Set Password will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "Set Password 'test'")
            var data = new Buffer('74657374', 'hex')
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_LOCKED]))
            bmdware.setPassword(data, null)
        },
        // Start Boot Loader will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "Start Boot Loader")
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_LOCKED]))
            bmdware.startBootloader(null)
        },
        function(callback) {
            utils.log(5, "Unlock Device with 'test'")
            var data = new Buffer('74657374', 'hex')
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_SUCCESS]))
            bmdware.unlockDevice(data, null)
        },

        //  set beacon uuid, major, disconnect, rescan beacon and verify beacon with uuid and major match
        function(callback) {
            utils.log(5, "beacon uuid set")
            bmdware.setBeaconUuid(test_beacon_uuid, callback)
        },
        function(callback) {
            utils.log(5, "Beacon set major")
            var major = parseInt(test_major)
            bmdware.setBeaconMajor(major, callback)
        },
        function(callback) {
            utils.log(5, "Beacon set enable")
            bmdware.setBeaconEnable(true, callback)
        },
        function(callback) {
            utils.log(5, "LockUnlock test disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Failed to disconnect'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "LockUnlock test rescanning to verify beacon uuid and major")
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

            utils.log(5, "LockUnlock test done")
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
    async.series([
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
        return 'LockUnlock Test'
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
