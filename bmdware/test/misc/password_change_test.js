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

var expected_results = []
var results = []

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

function testPasswordChange(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }
    async.series([
        function(callback) {
            utils.log(5, "PasswordChange test connect")
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
            utils.log(5, "Set Password  to 'test'")
            var data = new Buffer('74657374', 'hex')
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_SUCCESS]))
            bmdware.setPassword(data, null)
        },
        function(callback) {
            utils.log(5, "PasswordChange test disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Failed to disconnect'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "PasswordChange test start scanning")
            startScanBeacon(function(scanResult) {
                if(!scanResult) {
                    utils.log(5, 'Failed to discover beacon with appropriate data')
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "PasswordChange test reconnect")
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
        function(callback) {
            utils.log(5, "Unlock Device with 'test'")
            var data = new Buffer('74657374', 'hex')
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_SUCCESS]))
            bmdware.unlockDevice(data, null)
        },
        function(callback) {
            utils.log(5, "Set Password  to 'tester'")
            var data = new Buffer('746573746572', 'hex')
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_SUCCESS]))
            bmdware.setPassword(data, null)
        },
        function(callback) {
            utils.log(5, "Unlock Device with old password 'test'")
            var data = new Buffer('74657374', 'hex')
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_UNLOCKED_FAILED]))
            bmdware.unlockDevice(data, null)
        },
        function(callback) {
            utils.log(5, "Unlock Device with new password 'tester'")
            var data = new Buffer('746573746572', 'hex')
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_SUCCESS]))
            bmdware.unlockDevice(data, null)
        },
        function(callback) {
            utils.log(5, "PasswordChange test disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Failed to disconnect'
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

            utils.log(5, "PasswordChange test done")
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
            testPasswordChange(callback)
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
        return 'PasswordChange Test'
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
