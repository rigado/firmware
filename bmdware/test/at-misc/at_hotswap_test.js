#!/usr/bin/env nodejs

var async = require('async')
var ble = require('../support/ble')
var bmdware = require('../support/bmdware')
var bmdware_at = require('../support/bmdware_at')
var commander = require('commander')
var common = require('../support/common')
var fs = require('fs')
var noble = require('noble')
var serial = require('../support/serial')
var SerialPort = require('serialport')
var utils = require('../support/utils')

var testResult = 'FAIL'
var testNote = ''
var testShouldContinue = true

var onDataCompleteCallback

var results = []
var uartExpData = []

var waitRx = false;
var uartRxData = []
var bleRxData = []

var test_config
var target_port
var setup_port


const commandDelay = 2000
const baudRate = 57600

function atDataReceivecallback(data) {
    
    if(waitRx) {
        utils.log(5, 'uart rx (hex): ' + utils.bytesToHexString(data))
        utils.log(5, 'uart rx (ascii): ' + data.toString('ascii').trim())
        
        for(var i = 0; i < data.length; i++) {
            uartRxData.push(data[i])
        }
    }

    if(onDataCompleteCallback) {
        utils.log(5, "AT onDataCompleteCallback")
        var expected = uartExpData.pop()
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

function onBLEUartNotification(data, isNotification) {
    if(waitRx) {
        utils.log(5, 'ble rx (hex): ' + utils.bytesToHexString(data))
        utils.log(5, 'ble rx (ascii): ' + data.toString('ascii').trim())
        //utils.log(5, utils.bytesToHexString(lastDataSent))

        for(var i = 0; i < data.length; i++) {
            bleRxData.push(data[i])
        }
    }
}

function verifyBuffer(expected, actual, callback)
{
    var result = true

    if(expected.length || actual.length) {
        result = false;

        utils.log(5, "verifyBuffer exp: " + expected.toString('ascii').trim())
        utils.log(5, "verifyBuffer actual: " + actual.toString('ascii').trim())

        result = utils.compareBuffers(expected, actual)
        if (!result) {
            for (var i=0; i < expected.length && i < actual.length; i++) {
                utils.log(5, 'expected[%d] = %d, actual[%d] = %d', i, expected[i], i, actual[i])
            }
        }
    }

    utils.log(5, "verifyBuffer result: " + result)
    results.push(result)

    if(callback)
        callback()
}

function flushBLEData()
{
    bleRxData = []
}

function flushUARTData()
{
    target_port.flush()
    uartRxData = []
}

var verifyCount = 0

function verifyCommandResponse(uart_write_data, exp_uart_resp, exp_ble_resp, delay_ms, compareFinishCallback) {
    verifyCount++
    utils.log(5,"\n\n========= #" + verifyCount +" verifyCommandResponse =========")
    utils.log(5,"uart_write: " + uart_write_data.toString('ascii').trim())
    utils.log(5,"exp_uart_resp: " + exp_uart_resp.toString('ascii').trim())
    utils.log(5,"exp_ble_resp: " + exp_ble_resp.toString('ascii').trim())

    var w_uart  = new Buffer(uart_write_data, 'ascii')
    var e_uart  = new Buffer(exp_uart_resp, 'ascii')
    var e_ble   = new Buffer(exp_ble_resp, 'ascii')

    async.series([
        function(callback) {

            //flush
            flushBLEData();
            flushUARTData();

            //unregister cb
            onDataCompleteCallback = null
            waitRx = true

            //write command to the uart
            target_port.write(w_uart, callback)
        },
        //delay?
        function(callback) {
            if(delay_ms < 5)
                delay_ms = 5
            
            utils.delay(delay_ms,callback)
        },
        //check rx'd data
        function(callback) {
            waitRx = false

            //check uart data
            if(e_uart.length || uartRxData.length) {
                utils.log(5, "verifying uart data:")
                verifyBuffer(e_uart, uartRxData, null)
            }

            //check ble data
            if(e_ble.length || bleRxData.length) {
                utils.log(5, "verifying ble data:")
                verifyBuffer(e_ble, bleRxData, null)
            }

            callback()
        },
        function(callback) {
            utils.log(5,"=============================================\n\n")

            if(compareFinishCallback)
                compareFinishCallback()
        }
    ])
}

function openSetupUART(callback)
{
    setup_port = new SerialPort(test_config.setup_uart, 
                            {
                                baudrate: baudRate,
                                autoOpen: true,
                                // look for return and newline at the end of each data packet:
                                parser: SerialPort.parsers.readline("\n")
                            }, 
                            callback)
}

function openTargetUART(callback)
{
    target_port = new SerialPort(test_config.target_uart, 
                            {
                                baudrate: baudRate,
                                autoOpen: true,
                                // look for return and newline at the end of each data packet:
                                //parser: SerialPort.parsers.readline("\n")
                            }, 
                            callback)
}


function setATGpioPin(at_mode_enable, callback) {
    var hwConfig = JSON.parse(fs.readFileSync('hw_config.json', 'utf8'))
    var pinVal = bmdware_at.AT_STATE_LOW

    if(!at_mode_enable) {
        pinVal = bmdware_at.AT_STATE_HIGH
    }

    utils.log(5, "setATGpioPin: " + at_mode_enable)
    bmdware_at.writeGpio(setup_port, hwConfig.nrf52.at_ctrl_pin, pinVal, null)

    // delay to take effect
    utils.delay(1000, callback)
}

function setupBoards(setupCompleteCallback) {
    utils.log(5,'Setting up boards...')
    async.series([
        function(callback) {
            openTargetUART(callback)
        },
        function(callback) {
            common.init_at_mode(target_port, callback)
            bmdware_at.configureAtReceiveNotifications(target_port, atDataReceivecallback)
        },
        function(callback) {
            openSetupUART(callback)
        },
        function(callback) {
            setATGpioPin(1,callback)
        },
        function(callback) {    
            setupCompleteCallback()
        }
    ])

}


function configureBmdware(setupCompleteCallback) {
    utils.log(5,'configureBmdware')
    async.series([
        function(callback) {
            ble.loadConfiguration('test_config.json')
            ble.findTestDevice(bmdware.getBmdwareServiceUuids(), function(deviceFound) {
                if(!deviceFound) {
                    utils.log(5,'Did not find BMDware device!')
                    testShouldContinue = false
                }
                callback()
            })
        },
        function(callback) {
            utils.log(5, "Enable connect")
            ble.connectPeripheralUT(function(connectResult) {
                if(!connectResult) {
                    testNote = 'Could not connect to device!'
                    testShouldContinue = false
                    setupCompleteCallback()
                }
                callback()
            })
        },
        function(callback) {
            bmdware.resetDefaultConfiguration(callback)
        },
        function(callback) {
            bmdware.setUartEnable(false, callback)
        },
        function(callback) {
            bmdware.configureUartReceiveNotifications(onBLEUartNotification, callback)
        },
        function(callback) {
            bmdware.setUartParityEnable(false, callback)
        },
        function(callback) {
            bmdware.setUartFlowControlEnable(false, callback)
        },
        function(callback) {
            bmdware.setUartBaudRate(baudRate, callback)
        },
        function(callback) {
            bmdware.setUartEnable(true, callback)
        },
        function(callback) {    
            setupCompleteCallback()
        }
    ]);
}

function testSetup(setupCompleteCallback) {
    async.series([
        function(callback) {
            setupBoards(callback)
        },
        function(callback) {
            setupCompleteCallback()
        }
    ]);
}

function testHotswap(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }

    async.series([
        ////////////////////////////////////////
        // Reset the board with AT pin enabled
        
        function(callback) {
            utils.log(5, "Reset the board with AT pin enabled")
            configureBmdware(callback)
        },
        //hotswap off?
        function(callback) {
            verifyCommandResponse('at$hotswap?\n', '0\n', '', commandDelay, callback);
        },
        //unset at pin
        function(callback) {
            setATGpioPin(0,callback)
        },
        //verify at mode works
        function(callback) {
            verifyCommandResponse('at\n','OK\n','', commandDelay, callback);
        },
        //set at pin
        function(callback) {
            setATGpioPin(1,callback)
        },
        //verify at mode works
        function(callback) {
            verifyCommandResponse('at\n','OK\n','', commandDelay, callback);
        },

        ////////////////////////////////////////
        // Reset the board with AT pin disabled
        function(callback) {
            utils.log(5, "Reset the board with AT pin disabled")
            setATGpioPin(0,callback)
        },
        //wait for target to come back
        function(callback) {

            //utils.log(5,ble.getPeripheralUTCharacteristics())
            bmdware.reset(null)
            utils.delay(3000,callback)
        },
        //connect BLE
        function(callback) {
            configureBmdware(callback)
        },
        //set at pin
        function(callback) {
            setATGpioPin(1,callback)
        },
        //verify passthrough mode works
        function(callback) {
            verifyCommandResponse('at\n','','at\n', commandDelay, callback);
        },
        //unset at pin
        function(callback) {
            setATGpioPin(0,callback)
        },
        //verify passthrough mode works
        function(callback) {
            verifyCommandResponse('at\n','','at\n', commandDelay, callback);
        },

        ////////////////////////////////////////
        // Reset the board with AT pin active
        function(callback) {
            utils.log(5, "Reset the board with AT pin enabled")
            setATGpioPin(1,callback)
        },
        //wait for target to come back
        function(callback) {
            bmdware.reset(null)
            utils.delay(3000,callback)
        },
        //connect BLE
        function(callback) {
            configureBmdware(callback)
        },
        //check hotswap is off
        function(callback) {
            verifyCommandResponse('at$hotswap?\n','0\n', '', commandDelay, callback);
        },
        //enable hotswap
        function(callback) {
            verifyCommandResponse('at$hotswap 1\n','OK\n', '', commandDelay, callback);
        },
        //check hotswap is on
        function(callback) {
            verifyCommandResponse('at$hotswap?\n', '1\n', '', commandDelay, callback);
        },
        //unset at pin
        function(callback) {
            setATGpioPin(0,callback)
        },
        //verify passthrough mode works
        function(callback) {
            verifyCommandResponse('at\n','','at\n', commandDelay, callback);
        },
        //set at pin
        function(callback) {
            setATGpioPin(1,callback)
        },
        //verify at mode works
        function(callback) {
            verifyCommandResponse('at\n','OK\n','', commandDelay, callback);
        },
        //unset at pin
        function(callback) {
            setATGpioPin(0,callback)
        },
        //wait for target to come back
        function(callback) {
            bmdware.reset(null)
            utils.delay(3000,callback)
        },
        function(callback) {
            configureBmdware(callback)
        },
        //verify passthrough mode works
        function(callback) {
            verifyCommandResponse('at\n','','at\n', commandDelay, callback);
        },
        //set at pin
        function(callback) {
            setATGpioPin(1,callback)
        },
        //verify at mode works
        function(callback) {
            verifyCommandResponse('at\n','OK\n','', commandDelay, callback);
        },

        //////
        // Done!
        function(callback) {
            if(testCompleteCallback)
                testCompleteCallback()
        }
    ])
}


function testTearDown(tearDownCompleteCallback) {
    if(!testShouldContinue) {
        tearDownCompleteCallback()
        return
    }

    async.series([
        function(callback) {
            utils.log(5, "Reset to default settings")
            bmdware.resetDefaultConfiguration(callback)
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
    test_config = ble.getConfiguration()

    async.series([
        function(callback) {
            testSetup(callback)
        },
        function(callback) {
            testHotswap(callback)
        },
        function(callback) {
            calculateResults(callback)
        },
        function(callback, peripheral) {
            testTearDown(callback)
        },
        function(callback) {
            utils.log(5, getName() + " Test Complete")
            if(runnerCompleteCallback) {
                runnerCompleteCallback(testResult, testNote)  
            } else {
                process.exit(0)
            }
        }
    ])
}

function getName() {
        return 'AT Hotswap'
}

function calculateResults(callback) {
    var failCnt = 0;
    var passCnt = 0;

    for(var i=0; results.length > 0; i++) {
        var result = results.pop()

        if(result)
            passCnt++
        else
            failCnt++
    }

    utils.log(5, "TestsPass: " + passCnt + " TestsFail: " + failCnt)

    if(failCnt==0 && passCnt)
        testResult = 'PASS'
    else
        testResult = 'FAIL'

    if(callback)
        callback()
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
