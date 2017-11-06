#!/usr/bin/env nodejs

var noble = require('noble');
var ble = require('../support/ble')
var utils = require('../support/utils')
var bmdware = require('../support/bmdware')
var bmdware_at = require('../support/bmdware_at')
var common = require('../support/common')
var serial = require('../support/serial')
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

var expected_results = []
var results = []

var onDataCompleteCallback

var target_port
var setup_port

function atDataReceivecallback(data) {   
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
    }
}

function testSetup(setupCompleteCallback) {
    if(!testShouldContinue) {
        return setupCompleteCallback()
    }

    common.init_at_mode(target_port, setupCompleteCallback)
    bmdware_at.configureAtReceiveNotifications(target_port, atDataReceivecallback)
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

function testUartSetting(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }
    async.series([
        function(callback) {
            utils.log(5, "Configure Uart Baud Rate")
            expected_results.push(new Buffer(bmdware_at.RC_OK, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setUartBaudRate(target_port, '115200', null)
        },
        function(callback) {
            utils.log(5, "Configure Uart Flow Control Enable")
            expected_results.push(new Buffer(bmdware_at.RC_OK, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setUartFlowControlEnable(target_port, true, null)
        },
        function(callback) {
            utils.log(5, "Configure Uart Parity Enable")
            expected_results.push(new Buffer(bmdware_at.RC_OK, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setUartParityEnable(target_port, true, null)
        },
       function(callback) {
            utils.log(5, "Configure pass-through UART Enable")
            expected_results.push(new Buffer(bmdware_at.RC_OK, 'ascii'))
            onDataCompleteCallback = callback
            bmdware_at.setUartEnable(target_port, true, null)
        },
        function(callback) {
            common.disable_at_mode(callback)
        },
        function(callback) {
            utils.log(5, "AT Command Device Reset: devrst")
            // wait for 3 seconds after starting the boot loader
            utils.log(5, "Timeout in 3 secs ....")
            onDataCompleteCallback = null
            bmdware_at.reset(target_port, null)
            setTimeout(function() {
                utils.log(5, "Timeout passed now execute")
                callback()
            }, 3000)
        },
        function(callback) {
            utils.log(5, "Find the target board")
            onDataCompleteCallback = null
            ble.findTestDevice(bmdware.getBmdwareServiceUuids(), function(deviceFound) {
                if(!deviceFound) {
                    testNote = 'Could not find a BMDware test device!'
                    testShouldContinue = false
                    testCompleteCallback()
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
                    testCompleteCallback()
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Read Uart BaudRate and verify")
            onDataCompleteCallback = null
            bmdware.getUartBaudRate(function(error, data) {
                var result = false
                if(error) {
                    testNote = 'Failed to read!'
                    testShouldContinue = false
                    testCompleteCallback()
                }
                else {
                    utils.log(5, "Uart BaudRate data.length = " + data.length)
                    utils.log(5, "UART BaudRate (int) = ", data.readUInt32LE(0))
                    if (data != null && data.length > 0 && data.readUInt32LE(0) == 115200) {
                        utils.log(5, "UART BaudRate matched !!!")
                        result = true
                    }
                }
                results.push(result)
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Read Uart Flow Control and verify")
            onDataCompleteCallback = null
            bmdware.getUartFlowControlEnable(function(error, data) {
                var result = false
                if(error) {
                    testNote = 'Failed to read!'
                    testShouldContinue = false
                    testCompleteCallback()
                }
                else {
                    utils.log(5, "UART flow control data.length = " + data.length)
                    utils.log(5, "UART flow control data[0] (int) = %d", data[0])
                    if (data != null && data.length > 0 && data[0] == 1) {
                        result = true
                    }
                }
                results.push(result)
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Read Uart Parity Enable and verify")
            onDataCompleteCallback = null
            bmdware.getUartParityEnable(function(error, data) {
                var result = false
                if(error) {
                    testNote = 'Failed to read!'
                    testShouldContinue = false
                    testCompleteCallback()
                }
                else {
                    utils.log(5, "UART Parity Enable data.length = " + data.length)
                    utils.log(5, "UART Parity Enable data[0] (int) = %d", data[0])
                    if (data != null && data.length > 0 && data[0] == 1) {
                        result = true
                    }
                }
                results.push(result)
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Read Uart Enable and verify")
            onDataCompleteCallback = null
            bmdware.getUartEnable(function(error, data) {
                var result = false
                if(error) {
                    testNote = 'Failed to read!'
                    testShouldContinue = false
                    testCompleteCallback()
                }
                else {
                    utils.log(5, "UART Enable data.length = " + data.length)
                    utils.log(5, "UART Enable data[0] (int) = %d", data[0])
                    if (data != null && data.length > 0 && data[0] == 1) {
                        result = true
                    }
                }
                results.push(result)
                callback()
            })
        },
        function(callback) {
            bmdware.resetDefaultConfiguration(null)
            callback()
        },
        function(callback) {
            ble.disconnectPeripheralUT(function(result) {
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

            utils.log(5, "AT UART enable test done")
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
            target_port.close()
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

    async.series([
        function(callback) {
            target_port = serial.open(target_uart, {
                baudrate: baudrate,
                parser: serialPort.parsers.readline("\n")
            }, callback)
        },
        function(callback) {
            testSetup(callback)
        },
        function(callback) {
            testUartSetting(callback)
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
        return 'AT UART Setting Test'
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
