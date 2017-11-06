#!/usr/bin/env nodejs

var noble = require('noble')
var ble = require('../support/ble')
var utils = require('../support/utils')
var bmdware = require('../support/bmdware')
var async = require('async')
var commander = require('commander')

var callbackAfterBeaconDiscovery
var beaconUnderTestId
var testResult = 'FAIL'
var testNote = ''
var testShouldContinue = true
var testBeaconUuid = '56fc48c55a8443e2ab524cca99d5268c'
var testTimer

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
    }, 5000)

    noble.on('discover', onDiscoverBeacon)
    noble.startScanning([], true)
}

function onDiscoverBeacon(peripheral) {
    var adv = peripheral.advertisement
    var mfg_data = adv.manufacturerData

    if(mfg_data == null) {
        return
    }

    if(mfg_data.length > 0 && mfg_data[0] == 0x4C && mfg_data[1] == 0x00 && peripheral.rssi > -65) {
        var matchData = '4c000215' + testBeaconUuid
        var beaconData = utils.bytesToHexString(mfg_data)
        if(beaconData.indexOf(matchData) >= 0) {
            testResult = 'PASS'
            clearTimeout(testTimer  );
            testTimer = null
        } else {
            return
        }
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
                    testShouldContinue = false
                    testNote = 'Could not find a BMDware test device!'
                }
                callback()
            })
        },
        function(callback) {
            setupCompleteCallback()
        }
    ]);
}

function testBeaconEnable(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }

    async.series([
        function(callback) {
            utils.log(5, "Enable connect")
            ble.connectPeripheralUT(function(connectResult) {
                if(!connectResult) {
                    testNote = 'Could not connect to device!'
                    testShouldContinue = false
                    testCompleteCallback()
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, 'Set connectable advertisement interval')
            var buf = new Buffer(3)
            buf.writeUInt8(0x42, 0)
            buf.writeUInt8(0x1D, 1)
            buf.writeUInt8(0x00, 2)
            bmdware.writeControlPoint(buf, callback)
        },
        function(callback) {
            utils.log(5, "Enable set uuid")
            bmdware.setBeaconUuid(testBeaconUuid, callback)
        },
        function(callback) {
            utils.log(5, "Enable set")
            bmdware.setBeaconEnable(true, callback)
        },
        function(callback) {
            utils.log(5, "Enable disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Could not disconnect from device!'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Enable start scanning")
            startScanBeacon(function(discoverResult) {
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Enable test done")
            testCompleteCallback()
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
            ble.connectPeripheralUT(function(connectResut) {
                if(!connectResut){
                    testResult = 'FAIL'
                    testNote = 'Could not connect to device for Tear Down'
                    tearDownCompleteCallback()
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "TearDown disable beacon")
            bmdware.setBeaconEnable(false, callback)
        },
        function(callback) {
            utils.log(5, "TearDown disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Could not disconnect during Tear Down'
                }
            })
            callback()
        },
        function(callback) {
            utils.log(5, "TearDown done")
            tearDownCompleteCallback()
        }
    ]);
}

function testRunner(runnerCompleteCallback) {
    ble.loadConfiguration('test_config.json')
    async.series([
        function(callback) {
            testSetup(callback)
        },
        function(callback) {
            testBeaconEnable(callback)
        },
        function(callback, peripheral) {
            testTearDown(callback)
        },
        function(callback) {
            utils.log(5, "Beacon Enable Test Complete")
            if(runnerCompleteCallback) {
                runnerCompleteCallback(testResult, testNote)  
            } else {
                process.exit(0)
            }
        }
    ])
}

function getName() {
        return 'Beacon Enable'
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
