#!/usr/bin/env nodejs

var noble = require('noble');
var ble = require('../support/ble')
var utils = require('../support/utils')
var bmdware = require('../support/bmdware')
var async = require('async')
var commander = require('commander')
var SerialPort = require("serialport")

var callbackAfterBeaconDiscovery
var beaconUnderTestId

var testConfig
var testResult = 'FAIL'
var testNote = ''
var testShouldContinue = true
var testTimer
var test_status_count = 0
var test_beacon_uuid = "6d224c46ef864b4692090f559ec3adb4"

var expected_results = []
var results = []

var onDataCompleteCallback

const baudrate = 57600

var target_port
var setup_port

var rssi_total = 0
var rssi_count = 0
var MAX_COUNT = 20
var average_rssi
var high_rssi = 0
var low_rssi = 0
var rssi_difference
const EXPECTED_DIFFERENCE = 30 

function atDataReceivecallback(uartError, data) {
    if ( uartError ) {
        utils.log(1, 'uartError: ' + uartError)
    }
    else {
        if(onDataCompleteCallback) {
            utils.log(5, "AT onDataCompleteCallback")
            var expected = expected_results.pop()
            if(expected != null) {
                utils.log(5, 'received data: ' + data)
                utils.log(5, 'expected: ' + expected)
                var result = false
                result = utils.compareBuffers(expected, data)
                if (!result) {
                    for (var i=0; i < expected.length && i < data.length; i++) {
                        utils.log(5, 'expected[%d] = %d, data[%d] = %d', i, expected[i], i, data[i])
                    }
                }
                results.push(result)
            }
            onDataCompleteCallback()
        } else {
            // utils.log(5, 'onDataCompleteCallback is null?!?!')
        }
    }
}

// todo: move to uart.js
function uartTransmitReceive(port, sendData, callback) {
    var uartError
    var receiveData = null

    //assign data callback
    port.on('data', 
            processData = function(data) {
                    if (data != null) {
                        receiveData = new Buffer(data, 'ascii')
                        utils.log(5, 'data received: ' + receiveData)
                        atDataReceivecallback(uartError, receiveData)
                        port.removeListener('data', processData)
                    }
                });

    port.flush()

    //write the data
    port.write(sendData, function(err, results) {
        uartError = err
        //utils.log(5, 'Send err ' + err)
        //utils.log(5, 'Send results ' + results)
    })

    //exec callback
    if (callback)
    {
        callback(error)
    }

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
        utils.log(5,  "Time Out")
        callbackAfterBeaconDiscovery(false)
    }, 50000)
    noble.on('discover', onDiscoverBeacon)
    noble.startScanning(bmdware.getBmdwareServiceUuids(), true)
}

function onDiscoverBeacon(peripheral) {
    var adv = peripheral.advertisement

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
    } 
    else {
        low_rssi = average_rssi
        utils.log(5, "low_rssi = %d", low_rssi)
        rssi_difference = high_rssi - low_rssi
        utils.log(5, "rssi_difference = %d", rssi_difference)
        if (rssi_difference >= EXPECTED_DIFFERENCE) {
            var result = true
            results.push(result)
            clearTimeout(testTimer  );
            testTimer = null
                utils.log(5, "Connectable TX Power : PASS")
        }
    }


    // Once we discover one BMDWARE device, stop looking.
    noble.removeListener('discover', onDiscoverBeacon)
    noble.stopScanning()

    callbackAfterBeaconDiscovery(true)
}

function testSetup(setupCompleteCallback) {
    if(!testShouldContinue) {
        return setupCompleteCallback()
    }
    async.series([
        //open port
        function(callback) {
            utils.log(5,"opening setup port...")
            setup_port.open(callback)
        },
        // send AT command on the setup board to disable beacon advertisement
        // at$conadv 0\n
        function(callback) {
            utils.log(5, 'send AT command on the setup board to disable beacon advertisement')
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            uartTransmitReceive(setup_port, "at$conadv 0\n", null)
        },
        // send AT command to configure P0.24 as output, pull none
        function(callback) {
            utils.log(5, 'send AT command to configure P0.24 as output, pull none')
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            uartTransmitReceive(setup_port, "at$gcfg 01 01 00\n", null)
        },
        //
        // set P0.24 to LOW
        function(callback) {
            utils.log(5, 'send AT command to set P0.24 to LOW')
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            uartTransmitReceive(setup_port, "at$gset 01 00\n", null)
        },
        //
        // send AT command to configure P0.25 as output, pull none
        function(callback) {
            utils.log(5, 'send AT command to configure P0.25 as output, pull none')
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            uartTransmitReceive(setup_port, "at$gcfg 02 01 00\n", null)
        },
        //
        // set P0.25 to HIGH
        function(callback) {
            utils.log(5, 'send AT command to set P0.25 to HIGH')
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            uartTransmitReceive(setup_port, "at$gset 02 01\n", null)
        },
        // Find the target board
        function(callback) {
            utils.log(5, "Find the target board")
            onDataCompleteCallback = null
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
            utils.log(5, "connect to target board")
            onDataCompleteCallback = null
            ble.connectPeripheralUT(function(connectResult) {
                if(!connectResult) {
                    testNote = 'Failed to connect to device!'
                    testShouldContinue = false
                    setupCompleteCallback()
                }
                callback()
            })
        },
        //
        function(callback) {
            utils.log(5, "Notification enable")
            onDataCompleteCallback = null
            bmdware.configureBleReceiveNotifications(onData, callback)
        },
        //
        // Start Boot Loader
        function(callback) {
            utils.log(5, "Start Boot Loader")
            onDataCompleteCallback = null
            ble.getPeripheralUT().once('disconnect', function() {
                // wait for 3 seconds after starting the boot loader
                utils.log(5, "Done Start Boot Loader, timeout in 3 secs")
                var delay = setTimeout(function() {
                    delay = null;
                    callback()
                }, 3000)
            })
            bmdware.startBootloader(null)
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

function testConnectTxpower(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }
    async.series([
        function(callback) {
            target_port.flush(callback)
        },
        function(callback) {
            utils.log(5, "Set Connectable TX Power HIGH")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            uartTransmitReceive(target_port, "at$ctxpwr 04\n", null)
        },
        function(callback) {
            utils.log(5, "AT Command Device Reset: devrst")
            // wait for 3 seconds after starting the boot loader
            utils.log(5, "Timeout in 500 ms ....")
            onDataCompleteCallback = null
            uartTransmitReceive(target_port, "at$devrst\n", null)
            setTimeout(function() {
                utils.log(5, "Timeout passed now execute")
                callback()
            }, 500)
        },
        function(callback) {
            utils.log(5, "Beacon rescanning")
            startScanBeacon(function(scanResult) {
                if(!scanResult) {
                    utils.log(5, 'Failed to discover beacon with appropriate data')
                    results.push(scanResult)
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Set Connectable TX Power to LOW")
            expected_results.push(new Buffer('OK', 'ascii'))
            onDataCompleteCallback = callback
            uartTransmitReceive(target_port, "at$ctxpwr E2\n", null)
        },
        function(callback) {
            utils.log(5, "Beacon rescanning")
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

            utils.log(5, "AT Connectable TX Power test done")
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
            onDataCompleteCallback = null
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
        //close ports
        function(callback) {
            utils.log(5,"closing setup port...")
            setup_port.close(callback)
        },
        //close ports
        function(callback) {
            utils.log(5,"closing target port...")
            target_port.close(callback)
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

    if (testConfig.target_uart != "") {
        target_uart = testConfig.target_uart
    }
    if (testConfig.setup_uart != "") {
        setup_uart = testConfig.setup_uart
    }
    if (testConfig.baudrate != "") {
        baudrate =  parseInt(testConfig.baudrate)
    }

    target_port = new SerialPort(testConfig.target_uart, 
                                {
                                    baudrate: baudrate,
                                    autoOpen: true,
                                    // look for return and newline at the end of each data packet:
                                    parser: SerialPort.parsers.readline("\n")
                                },
                                function(){} )

    setup_port = new  SerialPort(testConfig.setup_uart, 
                                {
                                    baudrate: baudrate,
                                    autoOpen: true,
                                    // look for return and newline at the end of each data packet:
                                    parser: SerialPort.parsers.readline("\n")
                                },
                                function(){} )

    async.series([
        function(callback) {
            testSetup(callback)
        },
        function(callback) {
            testConnectTxpower(callback)
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
        return 'AT Connectable TX Power Test'
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
