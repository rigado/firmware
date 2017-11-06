#!/usr/bin/env nodejs

var SerialPort = require('serialport')
var noble = require('noble')
var ble = require('../support/ble')
var utils = require('../support/utils')
var bmdware = require('../support/bmdware')
var async = require('async')
var commander = require('commander')

var beaconUnderTestId
var testResult = 'FAIL'
var testNote = ''
var testShouldContinue = true
var testTimer

var passthrough_data

//TODO: move this to test_config item
var testPort = "/dev/cu.usbserial-DN002U6U"
var onReceiveLoopbackNotification
var receivedData = []
var dataIndex = 0
var results = []
var receiveCount = 0
var port

const dataChunkSize = 20
var baudRate = 57600
const total_data = 15000

function buildTestData() {
    passthrough_data = new Buffer(total_data)
    for(var i = 0; i < total_data; i++) {
        i_mod = i % 18
        if(i_mod < 10)
        {
            passthrough_data.writeUInt8(0x30 + i_mod, i)
        }
        else
        {
            passthrough_data.writeUInt8(0x61 + (i_mod - 10), i)
        }

        if(i_mod == 17 && i != 0)
        {
            passthrough_data.writeUInt8('0x0d', i)
        }

        if(i_mod == 0 && i != 0)
        {
            passthrough_data.writeUInt8('0x0a', i)
        }
        //passthrough_data.writeUInt8((i & 0xFF), i)
    }
    utils.log(5, utils.bytesToHexString(passthrough_data))
}

function openSerialPort(baudrate, callback) {
    // console.log("Create Serial port")
    // port = new SerialPort(testPort, {
    //     baudrate: baudrate,
    //     rtscts: true,
    //     autoOpen: false
    // })

    // port.on('data', function(data) {
    //         //loop the data back
    //         console.log('uart rx: ' + utils.bytesToHexString(data))
    //         //port.write(data)
    //     }
    // )

    // port.open(function(err) {    
    //     if(err) {
    //         console.log(err)
    //     }
    //     callback()
    // })
    callback()
}

function onBLEUartNotification(data, isNotification) {
    utils.log(5, 'rx: ' + utils.bytesToHexString(data) + ' rx len: ' + data.length)
    utils.log(5, utils.bytesToHexString(data))
    utils.log(5, 'rx count: ' + receiveCount)
    for(var i = 0; i < data.length; i++) {
        if((data[i] == 255 || data[i] == 254) && receiveCount == 0) {
            continue
        }

        if(receiveCount == lastDataSent.length)
        {
            break
        }

        receivedData.push(data[i])
        receiveCount++
    }

    if(onReceiveLoopbackNotification && receiveCount == total_data) {
        utils.log(5, 'total received: ' + receiveCount)
        receiveCount = 0
        utils.log(5, "run on notification callback")
        onReceiveLoopbackNotification()
    }
}

function onBLEControlNotification(data, isNotification) {
    utils.log(5, 'Received Control Point Notification')

    if(data[0] == 9)
    {
        utils.log(5, 'Buffer almost full!')
    }
    else if(data[0] == 10)
    {
        utils.log(5, 'Buffer full!')
    }
}

function configureBmdware(setupCompleteCallback) {
    console.log('configureBmdware')
    async.series([
        function(callback) {
            ble.loadConfiguration('test_config.json')
            ble.findTestDevice(bmdware.getBmdwareServiceUuids(), function(deviceFound) {
                if(!deviceFound) {
                    console.log('Did not find BMDware device!')
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
            bmdware.setConnectableTxPower(4, callback)
        },
        function(callback) {
            bmdware.setUartEnable(false, callback)
        },
        function(callback) {
            bmdware.configureUartReceiveNotifications(onBLEUartNotification, callback)
        },
        function(callback) {
            bmdware.configureBleReceiveNotifications(onBLEControlNotification, callback)
        },
        function(callback) {
            bmdware.setUartParityEnable(false, callback)
        },
        function(callback) {
            bmdware.setUartFlowControlEnable(true, callback)
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
            buildTestData()
            openSerialPort(baudRate, callback)
        },
        function(callback) {
            configureBmdware(callback)
        },
        function(callback) {
            setupCompleteCallback()
        }
    ]);
}

function testUartPassthrough(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }

    dataIndex = 0

    var start_time = new Date().getTime();
    async.series([
        function(callback) {
            lastDataSent = new Buffer(passthrough_data)
            onReceiveLoopbackNotification = callback
        },
        function(err) {
            var actual = new Buffer(receivedData)
            if(utils.compareBuffers(actual, passthrough_data)) {
                testResult = 'PASS'
                var stop_time = new Date().getTime();
                utils.log(1, 'Start Time: ', start_time)
                utils.log(1, 'Stop Time: ', stop_time)
                utils.log(1, 'Transfer Time (ms): '+ (stop_time - start_time))
                utils.log(1, 'Data Rate (kbps): ' + (total_data * 8) / (stop_time - start_time) )
            } else {
                testResult = 'FAIL'
                testNote = 'Received Data did not match Expected Data'
                utils.log(1, 'Passthrough Fail')
                utils.log(1, utils.bytesToHexString(actual) + ' != ' + utils.bytesToHexString(passthrough_data))

                var stop_time = new Date().getTime();
                utils.log(1, 'Transfer Time (ms): ', (stop_time - start_time))
            }
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
            utils.log(5, "TearDown disable uart")
            bmdware.setUartEnable(false, callback)
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
            testUartPassthrough(callback)
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
        return 'Uart Passthrough Big'
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
    option('-b, --baud <BAUD>', 'Baud rate to use').
    parse(process.argv);

if(commander.baud) {
    baudRate = parseInt(commander.baud, 10)
    utils.log(1, 'Using baudrate ' + baudRate)
}

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
