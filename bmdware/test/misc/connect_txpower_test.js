#!/usr/bin/env nodejs

var noble = require('noble');
var ble = require('../support/ble')
var utils = require('../support/utils')
var bmdware = require('../support/bmdware')
var async = require('async');
var commander = require('commander')

var callbackAfterBeaconDiscovery
var beaconUnderTestId
var testResult = 'FAIL'
var testNote = ''
var testShouldContinue = true
var testTimer

var rssi_total = 0
var rssi_count = 0
var MAX_COUNT = 50
var average_rssi
var high_rssi = 0
var low_rssi = 0
var rssi_difference
var DBN30 = -30
var DBN20 = -20
var DBN16 = -16
var DBN12 = -12
var DBN8  = -8
var DBN4  = -4
var DB0   = 0
var DB4   = 4

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
    noble.startScanning(bmdware.getBmdwareServiceUuids(), true)
}

function onDiscoverBeacon(peripheral) {
    var adv = peripheral.advertisement
    
    if(peripheral.id == beaconUnderTestId) {
        //utils.log(5, "peripheral.rssi = %d", peripheral.rssi)
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
            if (rssi_difference > 3) {
                utils.log(5, "Connectable TX Power : PASS")
                testResult = 'PASS'
                clearTimeout(testTimer  );
                testTimer = null
            } else {
                utils.log(5, "Connectable TX Power : FAIL")
            }
            high_rssi = low_rssi
            low_rssi = 0
        }
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

function testConnectTxPower(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }

    async.series([
        function(callback) {
            utils.log(5, "Connectable Tx Power connect")
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
            utils.log(5, "Connectable Tx Power set HIGH")
            bmdware.setConnectableTxPower(bmdware.TXPOWER_HIGH, callback)
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Could not disconnect from device!'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power start scanning with High TX Power Setting")
            startScanBeacon(function(discoverResult) {
                callback()
            })
        },

        // Set 0 
        function(callback) {
            utils.log(5, "Connectable Tx Power reconnect")
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
            utils.log(5, "Connectable Tx Power set 0")
            bmdware.setConnectableTxPower(DB0, callback)
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Could not disconnect from device!'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power start scanning")
            startScanBeacon(function(discoverResult) {
                callback()
            })
        },

        // Set -4
        function(callback) {
            utils.log(5, "Connectable Tx Power reconnect")
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
            utils.log(5, "Connectable Tx Power set DBN4")
            bmdware.setConnectableTxPower(DBN4, callback)
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Could not disconnect from device!'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power start scanning")
            startScanBeacon(function(discoverResult) {
                callback()
            })
        },

        // Set DBN8
        function(callback) {
            utils.log(5, "Connectable Tx Power reconnect")
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
            utils.log(5, "Connectable Tx Power set DBN8")
            bmdware.setConnectableTxPower(DBN8, callback)
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Could not disconnect from device!'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power start scanning")
            startScanBeacon(function(discoverResult) {
                callback()
            })
        },

        // Set DBN12
        function(callback) {
            utils.log(5, "Connectable Tx Power reconnect")
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
            utils.log(5, "Connectable Tx Power set DBN12")
            bmdware.setConnectableTxPower(DBN12, callback)
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Could not disconnect from device!'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power start scanning")
            startScanBeacon(function(discoverResult) {
                callback()
            })
        },

        // Set DBN16
        function(callback) {
            utils.log(5, "Connectable Tx Power reconnect")
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
            utils.log(5, "Connectable Tx Power set DBN16")
            bmdware.setConnectableTxPower(DBN12, callback)
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Could not disconnect from device!'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power start scanning")
            startScanBeacon(callback)
            startScanBeacon(function(discoverResult) {
                callback()
            })
        },

        // Set DBN20
        function(callback) {
            utils.log(5, "Connectable Tx Power reconnect")
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
            utils.log(5, "Connectable Tx Power set DBN20")
            bmdware.setConnectableTxPower(DBN12, callback)
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Could not disconnect from device!'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power start scanning")
            startScanBeacon(function(discoverResult) {
                callback()
            })
        },

        // Set DBN30
        function(callback) {
            utils.log(5, "Connectable Tx Power reconnect")
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
            utils.log(5, "Connectable Tx Power set low")
            bmdware.setConnectableTxPower(bmdware.TXPOWER_LOW, callback)
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Could not disconnect from device!'
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power start scanning with Low TX Power Setting")
            startScanBeacon(function(discoverResult) {
                callback()
            })
        },

        // done
        function(callback) {
            utils.log(5, "Connectable Tx Power test done")
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
            utils.log(5, "TearDown set Connectable TX Power to Default")
            bmdware.setConnectableTxPower(bmdware.TXPOWER_DEFAULT, callback)
        },
        function(callback) {
            utils.log(5, "TearDown set Beacon TX Power to Default")
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
            testConnectTxPower(callback)
        },
        function(callback, peripheral) {
            testTearDown(callback)
        },
        function(callback) {
            utils.log(5, "Connectable Tx Power Test Complete")
            if(runnerCompleteCallback) {
                runnerCompleteCallback(testResult, testNote)
            } else {
                process.exit(0)
            }
        }
    ])
}

function getName() {
        return 'Connectable TX Power'
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
