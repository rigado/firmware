#!/usr/bin/env nodejs

var noble = require('noble')
var ble = require('./ble')
var utils = require('./utils')
var bmdware = require('./bmdware')
var bmdware_at = require('./bmdware_at')
var serial = require('./serial')
var async = require('async')
var fs = require('fs')
var SerialPort = require('serialport')

var onAtDataCompleteCallback
var onDataCompleteCallback
var expected_results = []
var results = []

var atCheckResult = false
var hwType = 'Unknown'

function onAtData(data) {
    if(onAtDataCompleteCallback) {
        utils.log(5, "AT onDataCompleteCallback")
        clearTimeout(bmdware_at.uartTimer)
        bmdware_at.uartTimer = null;

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
        var delay = setTimeout(function() {
             utils.log('timeout')
             delay = null
            onAtDataCompleteCallback()
        }, 300)
    }
    else {
        utils.log(5, 'onAtDataCompleteCallback is null?!?!')
    }
}

function onBleData(data, isNotification) {
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

function checkAtMode(port, callback) {
    var timer
    onData = function(data) {
        if(data == 'OK' || data == 'ERR') {
            if(timer != null) {
                clearTimeout(timer)
            }
            atCheckResult = true
            if(callback) {
                port.removeListener('data', onData)
                callback(true)
            }
        }
    }
    port.on('data', onData)
    timer = setTimeout(function() {
        port.removeListener('data', onData)
        atCheckResult = false
        timer = null
        if(callback) {
            callback(false)
        }
    }, 1000)
    bmdware_at.sendAt(port, null)
}

function determine_hw_type(port, callback) {
    if(!atCheckResult) {
        if(callback) {
            callback(hwType)
        }
    }

    var timer
    onData = function(data) {
        utils.log('hwinfo: ' + data)
        if(timer != null) {
            clearTimeout(timer)
        }

        if(data.indexOf(bmdware_at.AT_NRF51_HW_ID) >= 0) {
            hwType = bmdware_at.AT_NRF51_HW_ID
        } else if(data.indexOf(bmdware_at.AT_NRF52_HW_ID) >= 0) {
            hwType = bmdware_at.AT_NRF52_HW_ID
        }
        if(callback) {
            port.removeListener('data', onData)
            callback(hwType)
        }
    }
    port.on('data', onData)
    var timer = setTimeout(function() {
        timer = null
        if(callback) {
            port.removeListener('data', onData)
            hwType = 'Unknown'
            callback(hwType)
        }
    }, 500)
    bmdware_at.getHardwareInfo(port, null)
}

function init_at_mode(target_port, completeCallback)
{
    var testConfig = JSON.parse(fs.readFileSync('test_config.json', 'utf8'))
    var setup_port
    async.series([
        function(callback) {
            utils.log(5, 'Configure setup UART')
            setup_port = serial.open(testConfig.setup_uart, {
                baudrate: parseInt(testConfig.baudrate),
                parser: SerialPort.parsers.readline("\n")
            }, callback)
        },
        function(callback) {
            checkAtMode(setup_port, function(result) {
                if(result) {
                    callback()
                } else {
                    utils.log(1, "AT mode not enabled on setup board!!")
                    completeCallback(false)
                }
            })
        },
        function(callback) {
            determine_hw_type(setup_port, function(result) {
                callback()
            })
        },
        function(callback) {
            checkAtMode(target_port, function(result) {
                callback()
            })
        },
        function(err) {
            setup_port.close()
            configure('', bmdware_at.AT_STATE_LOW, bmdware_at.AT_STATE_HIGH, completeCallback)
        }
    ])
}

function disable_at_mode(completeCallback) {
    var testConfig = JSON.parse(fs.readFileSync('test_config.json', 'utf8'))
    var hwConfig = JSON.parse(fs.readFileSync('hw_config.json', 'utf8'))
    var hwSetup

    async.series([
        function(callback) {
            utils.log(5, 'Configure setup UART')
            setup_port = serial.open(testConfig.setup_uart, {
                baudrate: parseInt(testConfig.baudrate),
                parser: SerialPort.parsers.readline("\n")
            }, callback)
        },
        function(callback) {
            checkAtMode(setup_port, function(result) {
                if(result) {
                    callback()
                } else {
                    utils.log(1, "AT mode not enabled on setup board!!")
                    completeCallback(false)
                }
            })
        },
        function(callback) {
            determine_hw_type(setup_port, function(result) {
                if(hwType == bmdware_at.AT_NRF52_HW_ID) {
                    hwSetup = hwConfig.nrf52
                } else if(hwType == bmdware_at.AT_NRF51_HW_ID) {
                    hwSetup = hwConfig.nrf51
                } else {
                    utils.log(1, 'Unknown device!')
                    return
                }
                callback()
            })
        },
        function(callback) {
            bmdware_at.configureAtReceiveNotifications(setup_port, onAtData)
            callback()
        },
        function(callback) {
            utils.log(5, 'Configure AT mode pin')
            expected_results.push(new Buffer('OK', 'ascii'))
            onAtDataCompleteCallback = callback
            bmdware_at.setGpioConfig(setup_port, hwSetup.at_ctrl_pin,
            bmdware_at.AT_DIRECTION_OUT, bmdware_at.AT_PULL_NONE, null)
        },
        function(callback) {
            utils.log(5, 'Configure AT mode')
            expected_results.push(new Buffer('OK', 'ascii'))
            onAtDataCompleteCallback = callback
            bmdware_at.writeGpio(setup_port, hwSetup.at_ctrl_pin, bmdware_at.AT_STATE_LOW, null)
        },
        function(err) {
            setup_port.close()
            completeCallback()
        }
    ])
}

function configure(device, at_mode, beacon_only_mode, setupCompleteCallback) {
    var testConfig = JSON.parse(fs.readFileSync('test_config.json', 'utf8'))
    var hwConfig = JSON.parse(fs.readFileSync('hw_config.json', 'utf8'))
    var hwSetup

    if(hwType == bmdware_at.AT_NRF52_HW_ID) {
        hwSetup = hwConfig.nrf52
    } else if(hwType == bmdware_at.AT_NRF51_HW_ID) {
        hwSetup = hwConfig.nrf51
    } else {
        utils.log(1, 'Unknown device!')
        return
    }

    var setup_port
    async.series([
        function(callback) {
            utils.log(5, 'Configure setup UART')
            setup_port = serial.open(testConfig.setup_uart, {
                baudrate: parseInt(testConfig.baudrate),
                parser: SerialPort.parsers.readline("\n")
            }, callback)
        },
        function(callback) {
            bmdware_at.configureAtReceiveNotifications(setup_port, onAtData)
            callback()
        },
        function(callback) {
            utils.log(5, 'Disable connectable advertisements for the setup board')
            expected_results.push(new Buffer('OK', 'ascii'))
            onAtDataCompleteCallback = callback
            bmdware_at.setConnectableAdvEnable(setup_port, false, null)
        },
        function(callback) {
            utils.log(5, 'Configure AT mode pin')
            expected_results.push(new Buffer('OK', 'ascii'))
            onAtDataCompleteCallback = callback
            bmdware_at.setGpioConfig(setup_port, hwSetup.at_ctrl_pin,
                bmdware_at.AT_DIRECTION_OUT, bmdware_at.AT_PULL_NONE, null)
        },
        function(callback) {
            utils.log(5, 'Configure AT mode')
            expected_results.push(new Buffer('OK', 'ascii'))
            onAtDataCompleteCallback = callback
            bmdware_at.writeGpio(setup_port, hwSetup.at_ctrl_pin, at_mode, null)
        },
        function(callback) {
            utils.log(5, 'Configure beacon only mode pin')
            expected_results.push(new Buffer('OK', 'ascii'))
            onAtDataCompleteCallback = callback
            bmdware_at.setGpioConfig(setup_port, hwSetup.beacon_only_pin,
                bmdware_at.AT_DIRECTION_OUT, bmdware_at.AT_PULL_NONE, null)
        },
        function(callback) {
            utils.log(5, 'Configure beacon only mode')
            expected_results.push(new Buffer('OK', 'ascii'))
            onAtDataCompleteCallback = callback
            bmdware_at.writeGpio(setup_port, hwSetup.beacon_only_pin, beacon_only_mode, null)
        },
        function(callback) {
            if(atCheckResult && at_mode == bmdware_at.AT_STATE_LOW) {
                setup_port.close()
                setupCompleteCallback()
                return
            } else {
                callback()
            }
        },
        function(callback) {
            utils.log(5, "Find the target board")
            onAtDataCompleteCallback = null
            onDataCompleteCallback = null
            ble.findTestDevice(bmdware.getBmdwareServiceUuids(), function(deviceFound) {
                if(!deviceFound) {
                    testNote = 'Could not find a BMDware test device!'
                    testShouldContinue = false
                    setup_port.close()
                    setupCompleteCallback()
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Connect to target board")
            onDataCompleteCallback = null
            ble.connectPeripheralUT(function(connectResult) {
                if(!connectResult) {
                    testNote = 'Failed to connect to device!'
                    testShouldContinue = false
                    setup_port.close()
                    setupCompleteCallback()
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Reset target board")
            onDataCompleteCallback = null
            ble.getPeripheralUT().once('disconnect', function() {
                // wait for 3 seconds after starting the boot loader
                utils.log(5, "Device reset complete")
                var delay = setTimeout(function() {
                    callback()    
                }, 500)
            })
            bmdware.reset(null)
        },
        function(callback) {
            utils.log(5, 'Setup complete')
            if(setupCompleteCallback) {
                setup_port.close()
                setupCompleteCallback()
            }
        }
    ]);
}

module.exports = {
    configure: configure,
    checkAtMode: checkAtMode,
    init_at_mode: init_at_mode,
    disable_at_mode: disable_at_mode,
}

