#!/usr/bin/env nodejs

var noble = require('noble');
var ble = require('../support/ble')
var utils = require('../support/utils')
var bmdware = require('../support/bmdware')
var async = require('async')
var commander = require('commander')

var callbackAfterBeaconDiscovery
var callbackAfterCustomBeaconDiscovery
var beaconUnderTestId

var testConfig
var testResult = 'FAIL'
var testNote = ''
var testShouldContinue = true
var testTimer
var test_beacon_uuid = "00112233445566778899aabbccddeeff"

var expected_results = []
var results = []

var onDataCompleteCallback

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
    var adv = peripheral.advertisement
    var serviceData = adv.serviceData
    var test_status_count = 0
    if (serviceData && serviceData.length) {
        for (var i in serviceData) {
            utils.log(5,  "serviceData.uuid = " + JSON.stringify(serviceData[i].uuid))
            utils.log(5,  "serviceData.data = " + JSON.stringify(serviceData[i].data.toString('hex')))
            var cpUuid = serviceData[i].uuid
            var cpData = serviceData[i].data
            var matchData = "00112233445566778899aabbccddeeff0123456789abcd"
            if(cpUuid == 'd8fe' && cpData.toString('hex') == matchData) {
                test_status_count = 1
                utils.log(5, "Found custom beacon uuid = " + JSON.stringify(cpUuid))
                clearTimeout(testTimer)
                testTimer = null
                break;
            }
        }
    }
    else {
        return
    }

    if (test_status_count == 0) {
        return
    }

    results.push(true)
    noble.removeListener('discover', onDiscoverCPBeacon)
    noble.stopScanning()
    callbackAfterCustomBeaconDiscovery(true)
}

function onDiscoverBeacon(peripheral) {
    var adv = peripheral.advertisement
    var mfg_data = adv.manufacturerData

    if(mfg_data == null) {
        return
    }

    // check that the beacon shows iBeacon style broadcast
    if (mfg_data.length > 0 && mfg_data[0] == 0x4C && mfg_data[1] == 0x00) {
        var matchData = "4c000215" + test_beacon_uuid
        var beaconData = utils.bytesToHexString(mfg_data)
        utils.log(5, "matchData = " + matchData)
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
    else {
        return
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

function testCustomBeaconData(testCompleteCallback) {
    // var control = bmdware.getBmdwareServiceUuids()
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }

    async.series([
        function(callback) {
            utils.log(5, "CustomBeaconData test connect")
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
            utils.log(5, "set Beacon Enable")
            bmdware.setBeaconEnable(true, callback)
        },
        function(callback) {
            utils.log(5, "Notification enable")
            bmdware.configureBleReceiveNotifications(onData, callback)
        },
        function(callback) {
            utils.log(5, "Set Custom Beacon Data1")
            var data = new Buffer('0303FED81A16fed800112233445566778899aa', 'hex')
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_SUCCESS]))
            bmdware.setCustomBeaconData1(data, null)
        },
        function(callback) {
            utils.log(5, "Set Custom Beacon Data2")
            var data = new Buffer('bbccddeeff0123456789abcd', 'hex')
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_SUCCESS]))
            bmdware.setCustomBeaconData2(data, null)
        },
        function(callback) {
            utils.log(5, "Save Custom Beacon Data")
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_SUCCESS]))
            bmdware.saveCustomBeaconData(null)
        },
        function(callback) {
            utils.log(5, "Custom Beacon Data test disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Failed to disconnect'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Custom Beacon Data test start scanning")
            startScanCustomBeacon(function(scanResult) {
                if(!scanResult) {
                    utils.log(5, 'Failed to discover beacon with appropriate data')
                    results.push(scanResult)
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "CustomBeaconData test reconnect")
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
            utils.log(5, "beacon uuid set")
            bmdware.setBeaconUuid(test_beacon_uuid, callback)
        },
        function(callback) {
            utils.log(5, "Notification enable")
            bmdware.configureBleReceiveNotifications(onData, callback)
        },
        function(callback) {
            utils.log(5, "Clear Custom Beacon Data")
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_SUCCESS]))
            bmdware.clearCustomBeaconData(null)
        },
        function(callback) {
            utils.log(5, "Custom Beacon Data test disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Failed to disconnect'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Custom Beacon Data test rescanning")
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

            utils.log(5, "Custom Beacon Data test test done")
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
            utils.log(5, "clear Beacon Enable")
            bmdware.setBeaconEnable(false, callback)
        },
/*
        function(callback) {
            utils.log(5, "Notification enable")
            bmdware.configureBleReceiveNotifications(onData, callback)
        },
        function(callback) {
            utils.log(5, "Reset Default Configuration")
            onDataCompleteCallback = callback
            bmdware.resetDefaultConfiguration(null)
        },
*/
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
            testCustomBeaconData(callback)
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
        return 'Custom Beacon Data Test'
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
