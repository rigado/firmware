#!/usr/bin/env nodejs

var noble = require('noble');
var ble = require('../support/ble')
var utils = require('../support/utils')
var bmdware = require('../support/bmdware')
var async = require('async')
var commander = require('commander')

var callbackAfterBeaconDiscovery
var beaconUnderTestId

var testConfig
var testResult = 'FAIL'
var testNote = ''
var testShouldContinue = true
var testTimer
var test_status_count = 0
var test_beacon_uuid = "00112233445566778899aabbccddeeff"

const SET_PASSWD_INDEX = 0
const SET_UUID_ATLOCKED_INDEX = 1
const CP_BEACON_DATA1_INDEX = 2
const CP_BEACON_DATA2_INDEX = 3
const SAVE_BEACON_DATA_INDEX = 4
const CLEAR_BEACON_DATA_INDEX = 5
const SET_RSSI_CAL_INDEX = 6
const GET_RSSI_CAL_INDEX = 7
const SET_PASSWD_ATLOCKED_INDEX = 8
const START_BOOT_LOADER_INDEX = 9
const UNLOCK_DEVICE_INDEX = 10
const SET_UUID_INDEX = 11
const RESET_DEFAULT_INDEX = 12
const SET_MAJOR_INDEX = 13

var command_status = new Buffer(SET_MAJOR_INDEX+1).fill(0)
var command_sequence = 0
var status_count = 0

var onDataCompleteCallback

function startScanBeacon(callback) {
    callbackAfterBeaconDiscovery = callback
    var peripheral = ble.getPeripheralUT()
    if(peripheral) {
        beaconUnderTestId = peripheral.id
    } else {
        utils.log(1, 'Beacon peripheral not found!')
        process.exit(1)
    }
    
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
    //noble.startScanning([], true)
    noble.startScanning(bmdware.getBmdwareServiceUuids(), true)
}

function onDiscoverBeacon(peripheral) {
    var adv = peripheral.advertisement


    if(peripheral.id != beaconUnderTestId) {
        return
    }

    // Once we discover one BMDWARE device, stop looking.
    noble.removeListener('discover', onDiscoverBeacon)
    noble.stopScanning()

    callbackAfterBeaconDiscovery(true)
}

var testSetupFunction
var testFunction
var testTearDownFunction

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
    utils.log(5, "Command Sequence: " + command_sequence)
    utils.log(1, "data = " + data.toString('hex'))
/*
    if ((command_sequence == SET_PASSWD_INDEX && data[0] == 0) ||
        (command_sequence == UNLOCK_DEVICE_INDEX && data[0] == 0))
    {
        //command_status[command_sequence] = 1
        status_count++
        return
    }
*/
    if ((data[0] == 1) || (data[0] == 0)) { 
        //command_status[command_sequence] = 1
        status_count++
    }

    if(onDataCompleteCallback) {
        utils.log(5, "onDataCompleteCallback")
        onDataCompleteCallback()
        //onDataCompleteCallback = null
    } else {
        utils.log(5, 'onDataCompleteCallback is null?!?!')
    }
}

function testPasswordLockUnlock(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }
    async.series([
        function(callback) {
            utils.log(5, "PasswordLockUnlock test connect")
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
            bmdware.resetDefaultConfiguration(null)
        },
        function(callback) {
            utils.log(5, "Set Password 'test'")
            command_sequence = SET_PASSWD_INDEX
            var data = new Buffer('74657374', 'hex')
            onDataCompleteCallback = callback
            bmdware.setPassword(data, null)
        },
        function(callback) {
            utils.log(5, "PasswordLockUnlock test disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Failed to disconnect'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "PasswordLockUnlock test start scanning")
            startScanBeacon(function(scanResult) {
                if(!scanResult) {
                    utils.log(5, 'Failed to discover beacon with appropriate data')
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "PasswordLockUnlock test reconnect")
            ble.connectPeripheralUT(function(connectResult) {
                if(!connectResult) {
                    testNote = 'Failed to connect to device!'
                    testShouldContinue = false
                    testCompleteCallback()
                }
                callback()
            })
        },
/*
        function(callback) {
            utils.log(5, "Set beacon Enable")
            bmdware.setBeaconEnable(true, callback)
        },
*/
        function(callback) {
            utils.log(5, "Notification enable")
            bmdware.configureBleReceiveNotifications(onData, callback)
        },
        // changing the uuid will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "beacon uuid set")
            command_sequence = SET_UUID_ATLOCKED_INDEX
            onDataCompleteCallback = callback
            bmdware.setBeaconUuid(test_beacon_uuid, null)
        },
        // changing the CP beacon data1 will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "Set Custom Beacon Data1 to 1234")
            command_sequence = CP_BEACON_DATA1_INDEX
            var data = new Buffer('1234', 'hex')
            onDataCompleteCallback = callback
            bmdware.setCustomBeaconData1(data, null)
        },
        // changing the CP beacon data2 will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "Set Custom Beacon Data2")
            command_sequence = CP_BEACON_DATA2_INDEX
            var data = new Buffer('1234', 'hex')
            onDataCompleteCallback = callback
            bmdware.setCustomBeaconData2(data, null)
        },
        // Save the CP beacon data will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "Save Custom Beacon Data")
            command_sequence = SAVE_BEACON_DATA_INDEX
            onDataCompleteCallback = callback
            bmdware.saveCustomBeaconData(null)
        },
        // Clear the CP beacon data will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "Clear Custom Beacon Data")
            command_sequence = CLEAR_BEACON_DATA_INDEX
            onDataCompleteCallback = callback
            bmdware.clearCustomBeaconData(null)
        },
        // Set RSSI Calibration will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "Set RSSI Calibration")
            command_sequence = SET_RSSI_CAL_INDEX
            var txPowerValue = 0xfc
            var rssiValue = 0xc0
            onDataCompleteCallback = callback
            bmdware.setRssiCalibrationData(txPowerValue, rssiValue, null)
        },
        // Get RSSI Calibration will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "Get RSSI Calibration")
            command_sequence = GET_RSSI_CAL_INDEX
            onDataCompleteCallback = callback
            bmdware.getRssiCalibrationData(null)
        },
        // Set Password will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "Set Password 'test'")
            command_sequence = SET_PASSWD_ATLOCKED_INDEX
            var data = new Buffer('74657374', 'hex')
            onDataCompleteCallback = callback
            bmdware.setPassword(data, null)
        },
        // Start Boot Loader will generate the notification RC=01 for (device locked)
        function(callback) {
            utils.log(5, "Start Boot Loader")
            command_sequence = START_BOOT_LOADER_INDEX
            onDataCompleteCallback = callback
            bmdware.startBootloader(null)
        },
/* comment this command due to the new change that allow this command to succeed even it is locked
        function(callback) {
            utils.log(5, "Reset Default Configuration")
            bmdware.resetDefaultConfiguration(callback)
        },
*/
        function(callback) {
            utils.log(5, "Unlock Device with 'test'")
            command_sequence = UNLOCK_DEVICE_INDEX
            var data = new Buffer('74657374', 'hex')
            onDataCompleteCallback = callback
            bmdware.unlockDevice(data, null)
        },
        function(callback) {
            utils.log(5, "beacon uuid set")
            command_sequence = SET_UUID_INDEX
            // onDataCompleteCallback = callback
            bmdware.setBeaconUuid(test_beacon_uuid, callback)
        },
        function(callback) {
            utils.log(5, "Reset Default Configuration")
            command_sequence = RESET_DEFAULT_INDEX
            onDataCompleteCallback = function(err) {
                //command_status[command_sequence] = 1
                // status_count++
                callback()
            }
            bmdware.resetDefaultConfiguration(null)
        },
        function(callback) {
            utils.log(5, "test disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Failed to disconnect'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "PasswordLockUnlock test rescanning")
            test_status_count++
            startScanBeacon(function(scanResult) {
                if(!scanResult) {
                    utils.log(5, 'Failed to discover beacon with appropriate data')
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "PasswordLockUnlock test reconnect")
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
            utils.log(5, "Beacon set major")
            command_sequence = SET_MAJOR_INDEX
            var major = parseInt(testConfig.test_beacon_major)
            bmdware.setBeaconMajor(major, function(err) {
                //command_status[command_sequence] = 1
                status_count++
                callback()
            })
        },
        function(callback) {
            utils.log(5, "PasswordLockUnlock test disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Failed to disconnect'
                }
                callback()
            })
        },
        function(callback) {
/*
            var status_count = 0
            for (i = 0; i < command_status.length; i++) {
                if (command_status[i] == 0) {
                    utils.log(5, "command_status[%d] is zero", i)
                }
                status_count += command_status[i]
            }
*/
            utils.log(5, "status_count = %d", status_count)
            if (status_count == command_status.length) {
                testResult = 'PASS'
                clearTimeout(testTimer  );
                testTimer = null
                utils.log(5, "Test : PASS")
            }
            utils.log(5, "PasswordLockUnlock test done")
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
/*
        function(callback) {
            utils.log(5, "Clear beacon Enable")
            bmdware.setBeaconEnable(false, callback)
        },
*/
        function(callback) {
            utils.log(5, "Notification Disable")
            bmdware.disableBleReceiveNotifications(callback, callback)
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
            testPasswordLockUnlock(callback)
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
        return 'PasswordLockUnlock Test'
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
