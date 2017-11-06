#!/usr/bin/env nodejs

/*
  bletest.js tool for automated Bluetooth Low Engergy testing
  @copyright (c) Rigado, LLC. All rights reserved.

  Source code licensed under BMD-200 Software License Agreement.
  You should have received a copy with purchase of BMD-200 product.
  If not, contact info@rigado.com for for a copy. 
*/
var noble = require('noble');
var async = require('async');
var util = require('util');
var commander = require('commander');
var sprintf = require('sprintf-js').sprintf;
var bitsyntax = require('bitsyntax');

function printf() {
    var args = Array.prototype.slice.call(arguments);
    console.log(sprintf.apply(null, args));
}
function errx() {
    var args = Array.prototype.slice.call(arguments);
    var retval = args.shift();
    console.error("ERROR: " + sprintf.apply(null, args));
    process.exit(retval);
}

// DFU constants

var DFU_NAME = 'RigCom';
var DFU_SERVICE_UUID       = '2413b33f707f90bd20452ab8807571b7';
var DFU_CONTROL_POINT_UUID = '2413b43f707f90bd20452ab8807571b7';
var DFU_BEACON_UUID        = '2413b53b707f90bd20452ab8807571b7';
var DFU_MAJOR_NUMBER_UUID  = '2413b63F707f90bd20452ab8807571b7';
var DFU_MINOR_NUMBER_UUID  = '2413b63f707f90bd20452ab8807571b7';

var DFU_BEACON_ENABLE_UUID = '2413ba3f707f90bd20452ab8807571b7';
var OP_ENABLE  = 1;
var OP_DISABLE = 0;

var DFU_BEACON_TX_POWER_UUID      = '2413b93f707f90bd20452ab8807571b7';
var original_rssi = 0;
var updated_rssi = 0;
var DFU_CONNECT_TX_POWER_UUID     = '2413bb3f707f90bd20452ab8807571b7';
var OP_LOW  = -30;
var OP_HIGH = 4;

var DIS_SERVICE_UUID = '180a';
var DIS_FWREV_UUID = '2a26';

var OP_RESPONSE = 16;
var OP_PKT_RCPT_NOTIF = 17;

var OPS = {
    1: 'Start DFU',
    2: 'Initialize DFU',
    3: 'Receive firmware image',
    4: 'Validate firmware image',
    5: 'Activate firmware and reset',
    6: 'System reset',
    7: 'Request written image size',
    8: 'Request packet receipt notification',
    9: 'Config',
    16: 'Response',
    17: 'Packet receipt notification',
};

var RESPONSES = {
    1: 'success',
    2: 'invalid state',
    3: 'not supported',
    4: 'data size exceeds limit',
    5: 'CRC error',
    6: 'operation failed',
};

// Configuration

// Require a specific BT address for the target
var mac_address = "";

// Implementation

var mode_verify = false;
var mode_config = false;

commander.
    version('1.0.0').
    usage('[options]').
    option('-f, --filter <TEST_NAME>', 'Test name to run.  Can be a wild card').
    option('-r, --repeat <Number of Time>', 'The number of time the test to be run.  Default: 1').
    option('-m, --mac <MAC>', 'Only test device with this MAC').
    option('-c, --check', 'Check or verify').
    parse(process.argv);

// Parse MAC address
if (commander.mac) {
    mac_address = commander.mac;
    printf("Target address: " + mac_address);
}

if (commander.check) {
    mode_verify = true;
}
else {
    mode_config = true;
}

// Start the scanning process
noble.on('stateChange', function(state) {
    if (state === "poweredOn") {
        console.log("Scanning for DFU device...");
        startScan();
    } else {
        console.error("unexpected state change: %s", state);
        process.exit(1);
    }
});

function startScan() {
    noble.on('discover', onDiscover);
    noble.startScanning([], true);
}

function checkError(error) {
    if (error !== null && error !== undefined) {
	console.error("Error: %s", error);
	process.exit(1);
    }
}

function unexpectedDisconnect() {
    console.error("Unexpected disconnect!");
    process.exit(1);
}

function onDiscover(peripheral) {
    var adv = peripheral.advertisement;
    var uuids = adv.serviceUuids;

    var bad = "";
    if (uuids && uuids.indexOf(DFU_SERVICE_UUID) < 0) {
        bad += " (wrong service)";
        console.log("uuids %s,   uuids.indexOf(DFU_SERVICE_UUID) %d,  peripheral.uuid %s",
		uuids, uuids.indexOf(DFU_SERVICE_UUID), peripheral.uuid);
    }
    if (adv.localName !== DFU_NAME)
        bad += " (wrong name)";
    if (mac_address && (mac_address !== peripheral.uuid))
        bad += " (wrong address)";
    console.log("Found: name %s, address %s, RSSI %d%s",
		adv.localName, peripheral.uuid, peripheral.rssi, bad);
    if (bad)
        return; // not our device

    // Once we discover one DFU device, stop looking.
    noble.removeListener('discover', onDiscover);
    noble.stopScanning();

    // Print a message and quit if the device ever disconnects.
    peripheral.on('disconnect', unexpectedDisconnect);

    // Connect
    console.log("Connecting...");
    peripheral.connect(function(error) {
        checkError(error);
	console.log("Discovering services...");
        // This just hangs up sometimes; add a timeout
        timer = setTimeout(function() {
            timer = null;
            console.log("Timed out");
            peripheral.disconnect();
            process.exit(1);
        }, 5000);
        peripheral.discoverSomeServicesAndCharacteristics(
            [DFU_SERVICE_UUID, DIS_SERVICE_UUID],
            [DFU_CONTROL_POINT_UUID,
             DIS_FWREV_UUID, DFU_BEACON_ENABLE_UUID, 
             DFU_BEACON_TX_POWER_UUID, DFU_CONNECT_TX_POWER_UUID,
            ],
	    function(error, services, characteristics) {
                if (!timer)
                    return;
                clearTimeout(timer);
		checkError(error);
                var chars = {};
                for (var i = 0; i < characteristics.length; i++) {
                    chars[characteristics[i].uuid] = characteristics[i];
                    console.log(characteristics[i].uuid)
                }
                if (!(DFU_CONTROL_POINT_UUID in chars) ||
                    !(DFU_BEACON_ENABLE_UUID)) {
                    console.log("Device is missing DFU characteristics!");
                    process.exit(1);
                }

                async.series([
                    function(callback) {
                        printRevision(peripheral, chars, callback);
                    },
                    function(callback) {
                        if (mode_config) {
                            if (commander.filter == "connectTXPower")
                               configConnectTxPower(peripheral, chars);
                            else if (commander.filter == "beaconTXPower")
                               configBeaconTxPower(peripheral, chars);
                            else
                               configBeaconEnable(peripheral, chars);
                        } else if (mode_verify) {
                            if (commander.filter == "connectTXPower")
                                readConnectTXPower(peripheral, chars, callback);
                            else if (commander.filter == "beaconTXPower")
                                readBeaconTXPower(peripheral, chars, callback);
                            else
                                readBeaconEnable(peripheral, chars, callback);
                        }
                    },
                    ]);
            });
    });
}

function formatResponse(data) {
    var dstr = data.toString('hex');
    if (data[0] === OP_PKT_RCPT_NOTIF) {
        return "packet receipt notification: " + dstr.slice(2);
    } else if (data[0] === OP_RESPONSE && data.length >= 3) {
        var op = util.format("(unknown op %d)", data[1]);
        if (data[1] in OPS) {
            op = OPS[data[1]];
        }
        var resp = util.format("(unknown response %d)", data[2]);
        if (data[2] in RESPONSES) {
            resp = RESPONSES[data[2]];
        }
        return "response to \"" + op + "\": " + resp + " " + dstr.slice(6);
    } else {
        return "unknown response: " + dstr;
    }
}

function readBeaconEnable(peripheral, chars, callback) {
                console.log(
                    "Read Beacon Enable");
    if (DFU_BEACON_ENABLE_UUID in chars) {
        chars[DFU_BEACON_ENABLE_UUID].read(function(error, data) {
            if (data) {
                console.log(
                    "Beacon Enable: " + data.toString('hex'));
                if (data.readUInt8(0) == OP_ENABLE)
                   console.log("Beacon Enable Test PASSED");
                else
                   console.log("Beacon Enable Test FAILED");
                peripheral.removeListener('disconnect', unexpectedDisconnect);
                peripheral.on('disconnect', function() {
                    process.exit(0);
                });
                peripheral.disconnect();
            }
            callback();
        });
    } else {
        callback();
    }
}

function readBeaconTXPower(peripheral, chars, callback) {
                console.log(
                    "Read Beacon TX Power");
    if (DFU_BEACON_TX_POWER_UUID in chars) {
        chars[DFU_BEACON_TX_POWER_UUID].read(function(error, data) {
            if (data) {
                console.log(
                    "Beacon TX Power: " + data.toString('hex'));
                updated_rssi = peripheral.rssi;
                console.log(
                    "updated_rssi: RSSI %d db", updated_rssi);
                if ((data.readInt8(0) == OP_LOW) && ((original_rssi - updated_rssi) > 15) )
                   console.log("Beacon TX Power Test PASSED");
                else
                   console.log("Beacon TX Power Test FAILED");
                peripheral.removeListener('disconnect', unexpectedDisconnect);
                peripheral.on('disconnect', function() {
                    process.exit(0);
                });
                peripheral.disconnect();
            }
            callback();
        });
    } else {
        callback();
    }
}

function readConnectTXPower(peripheral, chars, callback) {
                console.log(
                    "Read Connectable TX Power");
    if (DFU_CONNECT_TX_POWER_UUID in chars) {
        chars[DFU_CONNECT_TX_POWER_UUID].read(function(error, data) {
            if (data) {
                console.log(
                    "Connectable TX Power: " + data.toString('hex'));
                updated_rssi = peripheral.rssi;
                console.log(
                    "updated_rssi: RSSI %d db", updated_rssi);
                if ((data.readInt8(0) == OP_LOW) && ((original_rssi - updated_rssi) > 15) )
                   console.log("Connectable TX Power Test PASSED");
                else
                   console.log("Connectable  TX Power Test FAILED");
                peripheral.removeListener('disconnect', unexpectedDisconnect);
                peripheral.on('disconnect', function() {
                    process.exit(0);
                });
                peripheral.disconnect();
            }
            callback();
        });
    } else {
        callback();
    }
}

function printRevision(peripheral, chars, callback) {
    if (DIS_FWREV_UUID in chars) {
        chars[DIS_FWREV_UUID].read(function(error, data) {
            // Ignore errors; missing FW revision is OK
            if (data) {
                console.log(
                    "Bootloader firmware revision: " +
                        data.toString('ascii'));
            }
            callback();
        });
    } else {
        callback();
    }
}

function configBeaconEnable(peripheral, chars) {
    control = chars[DFU_BEACON_ENABLE_UUID];
    console.log("Test mode; Enabling Beacon.");

    // Set up a handler for received data
    rxCallbackQueue = [];
    control.on('read', function(data, isNotification) {
        if (rxCallbackQueue.length === 0) {
            // If no callbacks have been registered, just print it out.
            console.log("unhandled " + formatResponse(data));
            return;
        }
        // Otherwise, call the next registered callback.  This is a
        // pretty simple model that just assumes we're waiting for one
        // response at a time and that they'll come in order.
        var callback = rxCallbackQueue.shift();
        callback(data);
    });

    async.series([
        function(callback) {
            console.log("Enabling notifications...");
            control.notify(true, callback);
        },
        function(callback) {
            var buf = new Buffer(1);
            buf.writeUInt8(OP_ENABLE, 0);
            console.log("Write to Beacon Enable...");
            control.write(buf, false, function(err) {
                checkError(err);
                peripheral.disconnect();
            });
            peripheral.removeListener('disconnect', unexpectedDisconnect);
            peripheral.on('disconnect', function() {
                mode_verify = true;
                mode_config = false;
                startScan();
                // process.exit(0);
            });
        },
    ]);
}

function configBeaconTxPower(peripheral, chars) {
    control = chars[DFU_BEACON_TX_POWER_UUID];
    console.log("configBeaconTxPower()");
    original_rssi = peripheral.rssi;

    // Set up a handler for received data
    rxCallbackQueue = [];
    control.on('read', function(data, isNotification) {
        if (rxCallbackQueue.length === 0) {
            // If no callbacks have been registered, just print it out.
            console.log("unhandled " + formatResponse(data));
            return;
        }
        // Otherwise, call the next registered callback.  This is a
        // pretty simple model that just assumes we're waiting for one
        // response at a time and that they'll come in order.
        var callback = rxCallbackQueue.shift();
        callback(data);
    });

    async.series([
        function(callback) {
            console.log("Enabling notifications...");
            control.notify(true, callback);
        },
        function(callback) {
            var buf = new Buffer(1);
            buf.writeInt8(OP_LOW, 0);
            console.log("Write to Beacon Tx Power...");
            control.write(buf, false, function(err) {
                checkError(err);
                peripheral.disconnect();
            });
            peripheral.removeListener('disconnect', unexpectedDisconnect);
            peripheral.on('disconnect', function() {
                mode_verify = true;
                mode_config = false;
                startScan();
                // process.exit(0);
            });
        },
    ]);
}

function configConnectTxPower(peripheral, chars) {
    control = chars[DFU_CONNECT_TX_POWER_UUID];
    console.log("configConnectTxPower()");

    // Set up a handler for received data
    rxCallbackQueue = [];
    control.on('read', function(data, isNotification) {
        if (rxCallbackQueue.length === 0) {
            // If no callbacks have been registered, just print it out.
            console.log("unhandled " + formatResponse(data));
            return;
        }
        // Otherwise, call the next registered callback.  This is a
        // pretty simple model that just assumes we're waiting for one
        // response at a time and that they'll come in order.
        var callback = rxCallbackQueue.shift();
        callback(data);
    });

    async.series([
        function(callback) {
            console.log("Enabling notifications...");
            control.notify(true, callback);
        },
        function(callback) {
            var buf = new Buffer(1);
            buf.writeInt8(OP_LOW, 0);
            console.log("Write to Connect Tx Power...");
            control.write(buf, false, function(err) {
                checkError(err);
                peripheral.disconnect();
            });
            peripheral.removeListener('disconnect', unexpectedDisconnect);
            peripheral.on('disconnect', function() {
                mode_verify = true;
                mode_config = false;
                startScan();
                // process.exit(0);
            });
        },
    ]);
}
