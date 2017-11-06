#!/usr/bin/env nodejs

var noble = require('noble');
var ble = require('../support/ble')
var utils = require('../support/utils')
var bmdware = require('../support/bmdware')
var bmdware_at = require('../support/bmdware_at')
var common = require('../support/common')
var async = require('async')
var commander = require('commander')
var serialPort = require("serialport")
var SerialPort = serialPort.SerialPort

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

var target_uart = '/dev/ttyACM0'
var setup_uart = '/dev/ttyACM1'
const baudrate = 57600

var target_port
var setup_port

var onAtDataCompleteCallback

var passthrough_data
var onReceiveLoopbackNotification
var receivedData = []
var dataIndex = 0
var receiveCount = 0
const dataChunkSize = 20

// Function to create test data for uart passthrouh
function buildTestData() {
    passthrough_data = new Buffer(256)
    for(var i = 0; i < 256; i++) {
        passthrough_data.writeUInt8(i, i)
    }
    utils.log(5, utils.bytesToHexString(passthrough_data))
}

// handler the uart received and write back during the passthrough mode test
function onUartPassthroughLoopback(data, isNotification) {
    utils.log(5, 'uart_rx: ' + utils.bytesToHexString(data))
    utils.log(5, 'data.length: ' + data.length)
    target_port.write(data)
}

function onAtDataReceived(data, isNotification) {
    if(onAtDataCompleteCallback) {
        utils.log(5, "AT onDataCompleteCallback")
        if (bmdware_at.uartTimer) {
            clearTimeout(bmdware_at.uartTimer)
            bmdware_at.uartTimer = null;
        }
        var expected = expected_results.pop()
        if(expected != null) {
            var receiveData = new Buffer(data, 'ascii')
            utils.log(5, 'received data: ' + receiveData)
            utils.log(5, 'expected: ' + expected + '\n')
            var result = false
            result = utils.compareBuffers(expected, receiveData)
            if (!result) {
                for (var i=0; i < expected.length && i < receiveData.length; i++) {
                    utils.log(5, 'expected[%d] = %d, receiveData[%d] = %d', i, expected[i], i, receiveData[i])
                }
            }
            results.push(result)
        }
        onAtDataCompleteCallback()
        if (bmdware_at.callbackAfterDataReceive) return bmdware_at.callbackAfterDataReceive(true)
    }
    else {
        utils.log(5, 'onAtDataCompleteCallback is null?!?!')
    }
}

function scanServiceUuids(callback) {
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
    var serviceUuids = adv.serviceUuids

    var beaconServiceFound = false
    var uartServiceFound   = false

    var bmdwareServiceUuids = bmdware.getBmdwareServiceUuids()

    utils.log(5,  "peripheral.uuid = " + JSON.stringify(peripheral.uuid))
    utils.log(5,  "adv = " + JSON.stringify(adv))
    if (serviceUuids && serviceUuids.length) {
        for (var i in serviceUuids) {
            utils.log(5,  "serviceUuid       = " + JSON.stringify(serviceUuids[i]))
            utils.log(5,  "beaconServiceUuid = " + JSON.stringify(bmdwareServiceUuids[0]))
            utils.log(5,  "uartServiceUuid   = " + JSON.stringify(bmdwareServiceUuids[1]))
            if(serviceUuids[i] == bmdwareServiceUuids[0]) {
                beaconServiceFound = true
            }
            else if (serviceUuids[i] == bmdwareServiceUuids[1]) {
                uartServiceFound = true
            }
        }
    }
    else {
        return
    }
    clearTimeout(testTimer)
    testTimer = null

    if (beaconServiceFound && uartServiceFound) {
        results.push(true)
    }
    else
        results.push(false)

    noble.removeListener('discover', onDiscoverBeacon)
    noble.stopScanning()
    callbackAfterBeaconDiscovery(true)
}

function onBLEUartNotification(data, isNotification) {
    utils.log(5, 'ble_rx: ' + utils.bytesToHexString(data))
    for(var i = 0; i < data.length; i++) {
        if((data[i] == 255 || data[i] == 254) && receiveCount == 0) {
            continue
        }

        receivedData.push(data[i])
        receiveCount++
    }

    if(onReceiveLoopbackNotification && receiveCount == lastDataSent.length) {
        receiveCount = 0
        onReceiveLoopbackNotification()
    }
}

function testSetup(setupCompleteCallback) {
    if(!testShouldContinue) {
        return setupCompleteCallback()
    }
    buildTestData()
    return common.setupMode(setup_port, bmdware_at.AT_STATE_HIGH,
               bmdware_at.AT_STATE_HIGH, setupCompleteCallback)
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

function testDefaultMode(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }
    dataIndex = 0
    async.series([
        function(callback) {
            utils.log(5, "Scanning Service Uuids to verify that both beacon and uart service are broadcast")
            scanServiceUuids(function(scanResult) {
                if(!scanResult) {
                    utils.log(5, 'Failed to discover beacon with appropriate data')
                    results.push(scanResult)
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, 'open Uart target port for read notification')
            bmdware_at.configureAtReceiveNotifications(target_port, onAtDataReceived, callback)
        },

        function(callback) {
            utils.log(5, "Send AT to verify AT mode disable")
            expected_results.push(new Buffer('', 'ascii'))
            onAtDataCompleteCallback = callback
            bmdware_at.writeAtCommandTimeOut(target_port, "at\n", 3000, function(status) {
                if(!status) {
                    utils.log(5, 'timeout return Expected')
                    results.push(true)
                }
                else results.push(false)
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Connect via ble")
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
            onDataCompleteCallback = null 
            utils.log(5, "UartEnable: false ")
            bmdware.setUartEnable(false, callback)
        },
        function(callback) {
            utils.log(5, "Enable Uart Notification")
            bmdware.configureUartReceiveNotifications(onBLEUartNotification, callback)
        },
        function(callback) {
            utils.log(5, "Parity Enable: False")
            bmdware.setUartParityEnable(false, callback)
        },
        function(callback) {
            utils.log(5, "Flow Control: False")
            bmdware.setUartFlowControlEnable(false, callback)
        },
        function(callback) {
            utils.log(5, "baudrate: ")
            bmdware.setUartBaudRate(baudrate, callback)
        },
        function(callback) {
            utils.log(5, 'open Uart target port for read notification')
            onDataCompleteCallback = null
            onAtDataCompleteCallback = null
            target_port.removeListener('data', onAtDataReceived)
            target_port.close()
            // open the target_port in pass-through mode no parser (raw data)
            target_port = new SerialPort(target_uart, {
                baudrate: baudrate,
            }, false)
            bmdware_at.configureAtReceiveNotifications(target_port, onUartPassthroughLoopback, callback)
        },
        function(callback) {
            utils.log(5, "UartEnable: true ")
            bmdware.setUartEnable(true, callback)
        },
        function(callback) {
            utils.log(5, "Send first Passthrough data")
            var data = passthrough_data.slice(dataIndex, dataIndex + dataChunkSize)
            lastDataSent = new Buffer(data)
            dataIndex += dataChunkSize
            onReceiveLoopbackNotification = callback
            bmdware.writeBufferToUart(data, null)
        },
        function(callback) {
            utils.log(5, "Send Second Passthrough data")
            var data = passthrough_data.slice(dataIndex, dataIndex + dataChunkSize)
            lastDataSent = new Buffer(data)
            dataIndex += dataChunkSize
            onReceiveLoopbackNotification = callback
            bmdware.writeBufferToUart(data, null)
        },
        function(callback) {
            utils.log(5, "Send Third Passthrough data")
            var data = passthrough_data.slice(dataIndex, dataIndex + dataChunkSize)
            lastDataSent = new Buffer(data)
            dataIndex += dataChunkSize
            onReceiveLoopbackNotification = callback
            bmdware.writeBufferToUart(data, null)
        },
        function(callback) {
            utils.log(5, "Send fourth Passthrough data")
            var data = passthrough_data.slice(dataIndex, dataIndex + dataChunkSize)
            lastDataSent = new Buffer(data)
            dataIndex += dataChunkSize
            onReceiveLoopbackNotification = callback
            bmdware.writeBufferToUart(data, null)
        },
        function(callback) {
            utils.log(5, "Send fifth Passthrough data")
            var data = passthrough_data.slice(dataIndex, dataIndex + dataChunkSize)
            lastDataSent = new Buffer(data)
            dataIndex += dataChunkSize
            onReceiveLoopbackNotification = callback
            bmdware.writeBufferToUart(data, null)
        },
        function(callback) {
            utils.log(5, "Send sixth Passthrough data")
            var data = passthrough_data.slice(dataIndex, dataIndex + dataChunkSize)
            lastDataSent = new Buffer(data)
            dataIndex += dataChunkSize
            onReceiveLoopbackNotification = callback
            bmdware.writeBufferToUart(data, null)
        },
        function(callback) {
            utils.log(5, "Send seventh Passthrough data")
            var data = passthrough_data.slice(dataIndex, dataIndex + dataChunkSize)
            lastDataSent = new Buffer(data)
            dataIndex += dataChunkSize
            onReceiveLoopbackNotification = callback
            bmdware.writeBufferToUart(data, null)
        },
        function(callback) {
            utils.log(5, "Send eighth Passthrough data")
            var data = passthrough_data.slice(dataIndex, dataIndex + dataChunkSize)
            lastDataSent = new Buffer(data)
            dataIndex += dataChunkSize
            onReceiveLoopbackNotification = callback
            bmdware.writeBufferToUart(data, null)
        },
        function(callback) {
            utils.log(5, "Send ninth Passthrough data")
            var data = passthrough_data.slice(dataIndex, dataIndex + dataChunkSize)
            lastDataSent = new Buffer(data)
            dataIndex += dataChunkSize
            onReceiveLoopbackNotification = callback
            bmdware.writeBufferToUart(data, null)
        },
        function(callback) {
            utils.log(5, "Send tenth Passthrough data")
            var data = passthrough_data.slice(dataIndex, dataIndex + dataChunkSize)
            lastDataSent = new Buffer(data)
            dataIndex += dataChunkSize
            onReceiveLoopbackNotification = callback
            bmdware.writeBufferToUart(data, null)
        },
        function(callback) {
            utils.log(5, "Send eleventh Passthrough data")
            var data = passthrough_data.slice(dataIndex, dataIndex + dataChunkSize)
            lastDataSent = new Buffer(data)
            dataIndex += dataChunkSize
            onReceiveLoopbackNotification = callback
            bmdware.writeBufferToUart(data, null)
        },
        function(callback) {
            utils.log(5, "Send twelveth Passthrough data")
            var data = passthrough_data.slice(dataIndex, dataIndex + dataChunkSize)
            lastDataSent = new Buffer(data)
            dataIndex += dataChunkSize
            onReceiveLoopbackNotification = callback
            bmdware.writeBufferToUart(data, null)
        },
        function(callback) {
            utils.log(5, "Send last Passthrough data")
            var data = passthrough_data.slice(dataIndex, dataIndex + 16)
            lastDataSent = new Buffer(data)
            onReceiveLoopbackNotification = callback
            bmdware.writeBufferToUart(data, null)
        },
        function(callback) {
            var actual = new Buffer(receivedData)
            if(utils.compareBuffers(actual, passthrough_data)) {
                results.push(true)
            } else {
                testNote = 'Received Data did not match Expected Data'
                utils.log(1, 'Passthrough Fail')
                utils.log(1, utils.bytesToHexString(actual) + ' != ' + utils.bytesToHexString(passthrough_data))
                results.push(false)
            }
            testResult = 'PASS'
            for (i = 0; i < results.length; i++) {
                if(results.pop() != true) {
                    testResult = 'FAIL'
                    break
                }
            }
            utils.log(5, "Test : " + testResult)

            utils.log(5, "Bootup Config Default test done")
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


    target_port = new SerialPort(target_uart, {
        baudrate: baudrate,
        // look for return and newline at the end of each data packet:
        parser: serialPort.parsers.readline("\n")
    }, false)

    setup_port = new SerialPort(setup_uart, {
        baudrate: baudrate,
        // look for return and newline at the end of each data packet:
        parser: serialPort.parsers.readline("\n")
    }, false)

    async.series([
        function(callback) {
            testSetup(callback)
        },
        function(callback) {
            testDefaultMode(callback)
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
        return 'Bootup Default Mode Test'
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
