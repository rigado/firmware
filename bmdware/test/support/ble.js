#!/usr/bin/env nodejs

var noble = require('noble');
var async = require('async');
var commander = require('commander');
var sprintf = require('sprintf-js').sprintf;
var bitsyntax = require('bitsyntax');
var fs = require('fs')
var os = require('os')

var utils = require('./utils')

var DEVICE_FIRMWARE_REVISION_UUID = '2a26'
var DEVICE_SERIAL_NUMBER_UUID = '2a25'

var testConfig
var peripheralUnderTest
var charList
var logLevel
var timer

var discoverFinishedCallback

function scan(uuids, callback) {
	discoverFinishedCallback = callback
	noble.on('discover', onDiscover);

    timer = setTimeout(function() {
        noble.removeListener('discover', onDiscover)
        noble.stopScanning()
        testTimer = null;
        utils.log(1, 'Could not find test device')
        discoverFinishedCallback(false)        
    }, 20000)

    if(noble.state == "poweredOff" || noble.state == "unknown") {    
        // Start the scanning process
        noble.on('stateChange', function(state) {
            if (state === "poweredOn") {
                utils.log(3, "Scanning for BMDware device...");
                noble.startScanning(uuids, true)
            } else {
                utils.log(1, "unexpected state change: %s", state);
                process.exit(1);
            }
        });
    } else {
        noble.startScanning(uuids, true);
    }
}

function onDiscover(peripheral) {
    var adv = peripheral.advertisement;
    var uuids = adv.serviceUuids;
    //var serviceUuid = fullUuidFromBase(BMDWARE_BEACON_BASE_UUID, BMDWARE_SERVICE_UUID)
    var bad = "";

	//   if (uuids && (uuids.indexOf(serviceUuid) < 0) && (uuids.indexOf(BMDWARE_UART_SERVICE_UUID) < 0)) {
	//       bad += " (wrong service)";
	//       console.log("uuids %s,   uuids.indexOf(BMDWARE_SERVICE_UUID) %d,  peripheral.uuid %s",
		// uuids, uuids.indexOf(serviceUuid), peripheral.uuid);
	//   }

    if (peripheral.rssi < -68) {
        bad += " (too far away)"
    }

    if (adv.localName != testConfig.adv_name) {
        bad += " (wrong name)";
    }

    utils.log(3, "Found: name %s, address %s, RSSI %d%s",
		adv.localName, peripheral.uuid, peripheral.rssi, bad);

    if (bad) {
        return; // not our device
    }

    clearTimeout(timer)
    timer = null

    // Once we discover one BMDWARE device, stop looking.
    noble.removeListener('discover', onDiscover);
    noble.stopScanning();

    peripheralUnderTest = peripheral
    discoverFinishedCallback(true)
}

function connect(peripheral, connectedCallback) {
    timer = setTimeout(function() {
        timer = null
        utils.log(1, "Could not connect to device!")
        connectedCallback(false)
    }, 10000);

    peripheral.connect(function connectCallback(error) {

        if(timer == null) {
            //Connect timer already fired and called the callback
            return
        }

        clearTimeout(timer)
        timer = null

        utils.log(5, "Connected!")
        if(!utils.checkError(error)) {
            peripheral.disconnect()
            connectCallback(false)
        }
        utils.log(5, "Discovering services...");

        // This just hangs up sometimes; add a timeout
        timer = setTimeout(function() {
            timer = null;
            utils.log(1, "Serivce discovery timed out");
            peripheral.disconnect();
            connectedCallback(false)
        }, 10000);

        //Yes, this is nasty to look at. I'm sorry because I don't understand javascript very well
		//- Eric
	peripheral.discoverAllServicesAndCharacteristics(function discoverCallback(error, services, characteristics) {
            utils.log(5, "Discovered services and characteristics")
            if (!timer) {
                return
            }

            clearTimeout(timer);
            if(!utils.checkError(error)) {
                peripheral.disconnect()
                connectedCallback(false)
                return
            }
            
            charList = {};
            if(!characteristics) {
                peripheral.disconnect()
                connectedCallback(false)
                return
            }

            for (var i = 0; i < characteristics.length; i++) {
                charList[characteristics[i].uuid] = characteristics[i];
            }

            //Verify correct version is being tested
            charList[DEVICE_FIRMWARE_REVISION_UUID].read(function(error, data) {
                async.series([
                    function(callback) {
                        utils.log(3, "BMDware firmware version: " +
                            data.toString('utf8'));
                        if((testConfig.version_under_test != "") &&
                                (data.toString('utf8') != testConfig.version_under_test)) {
                            utils.log(1, "Wrong BMDware version!")
                            connectedCallback(false)
                        }
                        callback()
                    }, 
                    function(cb) {
                    	//Verify correct mac address
                        charList[DEVICE_SERIAL_NUMBER_UUID].read(function(error, data) {
			                async.series([
			                    function(callback) {
			                        utils.log(3, "BMDware MAC address: " +
			                            data.toString('utf8'));
			                        if((testConfig.device_mac != "") &&
                                        (data.toString('utf8') != testConfig.device_mac)) {
			                            utils.log(1, "Wrong MAC version; test expects " + testConfig.device_mac)
			                            connectedCallback(false)
			                        } 
			                        callback()
			                    }, function(err) {
			                        cb()
			                    }
			                ])
			            }) 
                    },
                    function(err) {
                    	connectedCallback(true)
                    }
                ])
            })
		})
	})
}

module.exports = {
	fullUuidFromBase : function(baseUuid, shortUuid) {
		var uuid = baseUuid
        //this is a hack to get around the fact that the uart service starts with 6e40
        if(shortUuid.length == 5) {
            uuid = uuid.replace(/00000/g, shortUuid)
        } else {
            uuid = uuid.replace(/0000/g, shortUuid)
        }
		return uuid
	},
	loadConfiguration : function(config_file) {
		testConfig = JSON.parse(fs.readFileSync(config_file, 'utf8'))
		logLevel = parseInt(testConfig.log_level)
		utils.setLogLevel(logLevel)
	},
	getConfiguration : function() {
		return testConfig
	},
	getPeripheralUT : function() {
		return peripheralUnderTest
	},
	findTestDevice : function(uuids, callback) {
		if(!peripheralUnderTest) {
			scan(uuids, callback)
		} else {
			callback(true)
		}
	},
	connectPeripheralUT : function(callback) {
		if(!peripheralUnderTest) {
			utils.log(1, 'Peripheral not found!')
			callback(false)
		}
		
		if('connected' == peripheralUnderTest.state) {
			callback(true)
		} else {
			connect(peripheralUnderTest, callback)
		}
	},
	disconnectPeripheralUT : function(callback) {
		if(!peripheralUnderTest) {
			utils.log(1, 'Peripheral not found!')
			callback(false)
		}

		if(!peripheralUnderTest.state == 'connected') {
			callback(true)
		}

		peripheralUnderTest.disconnect(function() {
            callback(true)
        })
	},
	getPeripheralUTCharacteristics : function() {
		return charList
	},
}
