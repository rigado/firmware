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

    //check for mfg manufactuer data match
    var majorHex = utils.hexStringFromInt(parseInt(testConfig.test_beacon_major), true)
    var minorHex = utils.hexStringFromInt(parseInt(testConfig.test_beacon_minor), true)
    var matchData = "4c000215" + testConfig.test_beacon_uuid + majorHex + minorHex
    var beaconData = utils.bytesToHexString(mfg_data)
    if(beaconData.indexOf(matchData) >= 0) {
        testResult = 'PASS'
    } else {
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

function testBeaconPayload(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }

    async.series([
        function(callback) {
            utils.log(5, "Beacon payload connect")
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
            utils.log(5, "Beacon set uuid")
            bmdware.setBeaconUuid(testConfig.test_beacon_uuid, callback)
        },
        function(callback) {
            utils.log(5, "Beacon set major")
            var major = parseInt(testConfig.test_beacon_major)
            bmdware.setBeaconMajor(major, callback)
        },
        function(callback) {
            utils.log(5, "Beacon set minor")
            var minor = parseInt(testConfig.test_beacon_minor)
            bmdware.setBeaconMinor(minor, callback)
        },
        function(callback) {
            utils.log(5, "Beacon set enable")
            bmdware.setBeaconEnable(true, callback)
        },
        function(callback) {
            utils.log(5, "Beacon payload disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Failed to disconnect after setting Beacon parameters!'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Beacon payload start scanning")
            startScanBeacon(function(scanResult) {
                if(!scanResult) {
                    utils.log(5, 'Failed to discover beacon with appropriate data')
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Beacon payload test done")
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
            utils.log(5, "TearDown Beacon set uuid")
            bmdware.setBeaconUuid(null, callback)
        },
        function(callback) {
            utils.log(5, "TearDown Beacon set major")
            bmdware.setBeaconMajor(null, callback)
        },
        function(callback) {
            utils.log(5, "TearDown Beacon set minor")
            bmdware.setBeaconMinor(null, callback)
        },
        function(callback) {
            utils.log(5, "TearDown Beacon clear enable")
            bmdware.setBeaconEnable(false, callback)
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
            testBeaconPayload(callback)
        },
        function(callback, peripheral) {
            testTearDown(callback)
        },
        function(callback) {
            utils.log(5, "Beacon Enable Test Complete")
            if(testCompleteCallback) {
                testCompleteCallback(testResult, testNote)  
            } else {
                process.exit(0)
            }
        }
    ])
}

function getName() {
        return 'Beacon Payload'
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
