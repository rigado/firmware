#!/usr/bin/env nodejs

var noble = require('noble');
var ble = require('../support/ble')
var utils = require('../support/utils')
var bmdware = require('../support/bmdware')
var async = require('async')
var commander = require('commander')

var callbackAfterBeaconDiscovery
var testResult = 'FAIL'
var testNote = ''
var testShouldContinue = true
var testTimer

var rssi_total = 0
var rssi_count = 0
var MAX_COUNT = 20 
var average_rssi
var high_rssi = 0
var low_rssi = 0
var rssi_difference
var txpower_test_beacon_uuid = "1a3de0cd892b4e34a80acd9aa339bbee"

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
    }, 80000)

    noble.on('discover', onDiscoverBeacon)
    noble.startScanning([], true)
}

function onDiscoverBeacon(peripheral) {
    var adv = peripheral.advertisement
    var mfg_data = adv.manufacturerData

    if(mfg_data == null) {
        return
    }

    if(mfg_data.length > 0 && mfg_data[0] == 0x4C && mfg_data[1] == 0x00) {
        var matchData = "4c000215" + txpower_test_beacon_uuid
        var beaconData = utils.bytesToHexString(mfg_data)
        if(beaconData.indexOf(matchData) >= 0) {
            utils.log(5, "peripheral.rssi = %d", peripheral.rssi)
            rssi_total += peripheral.rssi;
            rssi_count++
            if (rssi_count < MAX_COUNT) {
               return
            }
            average_rssi = rssi_total/rssi_count

            // reset total and count
            rssi_total = 0
            rssi_count = 0

            if (high_rssi == 0) {
                high_rssi = average_rssi
                utils.log(5, "high_rssi = %d", high_rssi)
            } else {
                low_rssi = average_rssi
                utils.log(5, "low_rssi = %d", low_rssi)
                rssi_difference = high_rssi - low_rssi
                utils.log(5, "rssi_difference = %d", rssi_difference)
                if (rssi_difference > 15) {
                    testResult = 'PASS'
                    clearTimeout(testTimer  );
                    testTimer = null
                    utils.log(5, "Beacon TX Power : PASS")
                } else {
                    utils.log(5, "Beacon TX Power : FAIL")
                }
            }
        } else {
            return
        }
    } else {
        return
    }

    // Once we discover one BMDWARE device, stop looking.
    noble.removeListener('discover', onDiscoverBeacon)
    noble.stopScanning()

    callbackAfterBeaconDiscovery()
}

var testSetupFunction
var testFunction
var testTearDownFunction

function testSetup(setupCompleteCallback) {
	async.series([
        function(callback) {
            ble.findTestDevice(bmdware.getBmdwareServiceUuids(), 
                               function(deviceFound) {
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

function testBeaconTxPower(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }

    async.series([
        function(callback) {
            utils.log(5, "Beacon Tx Power connect")
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
            utils.log(5, "Beacon uuid set for TX Power test")
            bmdware.setBeaconUuid(txpower_test_beacon_uuid, callback)
        },
        function(callback) {
            utils.log(5, "Beacon Tx Power set HIGH")
            bmdware.setBeaconTxPower(bmdware.TXPOWER_HIGH, callback)
        },
        function(callback) {
            utils.log(5, "Enable set")
            bmdware.setBeaconEnable(true, callback)
        },
        function(callback) {
            utils.log(5, "Beacon Tx Power disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Could not disconnect from device!'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Beacon Tx Power start scanning with High TX Power Setting")
            startScanBeacon(function(discoverResult) {
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Beacon Tx Power reconnect")
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
            utils.log(5, "Beacon Tx Power set low")
            bmdware.setBeaconTxPower(bmdware.TXPOWER_LOW, callback)
        },
        function(callback) {
            utils.log(5, "Beacon Tx Power disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Could not disconnect from device!'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Beacon Tx Power start scanning with Low TX Power Setting")
            startScanBeacon(function(discoverResult) {
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Beacon Tx Power test done")
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
            utils.log(5, "TearDown set TX Power to Default")
            bmdware.setBeaconTxPower(bmdware.TXPOWER_DEFAULT, callback)
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
            testBeaconTxPower(callback)
        },
        function(callback, peripheral) {
            testTearDown(callback)
        },
        function(callback) {
            utils.log(5, "Beacon Tx Power Test Complete")
            if(runnerCompleteCallback) {
                runnerCompleteCallback(testResult, testNote)
            } else {
                process.exit(0)
            }
        }
    ])
}

function getName() {
        return 'Beacon TX Power'
    }

//run all tests
module.exports = {
    testRunner : testRunner,
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

