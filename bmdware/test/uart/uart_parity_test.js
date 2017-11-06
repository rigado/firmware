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
var onReceiveLoopbackNotification
var receivedData = []
var dataIndex = 0
var results = []
var receiveCount = 0
var dataRxDoneCallback;
var loopbackbytes = 0
var baudRate = 115200

var flowctrl = true

var canSendData = true;
const dataChunkSize = 96

function buildTestData(len,callback) {
    passthrough_data = new Buffer(len)

    for(var i = 0; i < passthrough_data.length; i++) {
        passthrough_data.writeUInt8(i%0xff, i)
    }

    //utils.log(5, utils.bytesToHexString(passthrough_data))
    utils.log(1, "generated " + passthrough_data.length + " bytes test data.")

    callback()
}

function openSerialPort(baudrate, callback) {
    console.log("Create Serial port")
    port = new SerialPort(ble.getConfiguration().target_uart, {
        baudrate: baudrate,
        rtscts: flowctrl,
        parity: 'even'
    }, function(callback){})

    port.on('data', function(data) {
            //loop the data back
            loopbackbytes += data.length
            console.log('uart_rx->tx (' + loopbackbytes+ '): ' + utils.bytesToHexString(data))
            port.write(data)
        }
    )

    port.on('error', function(error) {
            console.log('uart error: ' + error)
        }
    )

    port.open(function(err) {    
        if(err) {
            console.log(err)
        }
        callback()
    })
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

    onReceiveLoopbackNotification();
}

function onBLEFlowControlNotification(data, isNotification) {
    utils.log(5, '########### Received Control Point Notification')

    if(data[0] == 0x09)
    {
        canSendData = false
        utils.log(5, 'Buffer almost full!')
    }
    else if(data[0] == 0x0a)
    {
        canSendData = false
        utils.log(5, 'Buffer full!')
    }
    else if(data[0] == 0x0b)
    {
        canSendData = true;
        utils.log(0, "Buffer empty!")

        //resume
        sendData()
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
            bmdware.setUartEnable(false, callback)
        },
        function(callback) {
            bmdware.configureUartReceiveNotifications(onBLEUartNotification, callback)
        },
        function(callback) {
            bmdware.configureBleReceiveNotifications(onBLEFlowControlNotification, callback)
        },
        function(callback) {
            bmdware.setUartParityEnable(true, callback)
        },
        function(callback) {
            bmdware.setUartFlowControlEnable(flowctrl, callback)
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
            buildTestData(commander.datalen, callback);
        },
        function(callback) {
            console.log("writing " + commander.datalen +" bytes at " + baudRate + " baud " + dataChunkSize + " chunkSize");
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

function sendData()
{
    if(canSendData) {
        console.log("senddata: idx " + dataIndex + " rx "+ receiveCount + " plen " + passthrough_data.length )

        if(receiveCount < passthrough_data.length)
        {
            if(dataIndex < passthrough_data.length) {

                var chunkSize = passthrough_data.length - dataIndex

                if(chunkSize > dataChunkSize)
                {
                    chunkSize = dataChunkSize
                }

                var data = passthrough_data.slice(dataIndex, dataIndex + chunkSize)
                dataIndex += dataChunkSize
                onReceiveLoopbackNotification = sendData
                bmdware.writeBufferToUart(data, null)
                console.log("ble_tx: " + utils.bytesToHexString(data))
            }
            else
            {
                //we are done txing, just have to wait to rx all the data through the loopback
            }
        }
        else
        {
            //done!!
            dataRxDoneCallback();
        }
    }
    else
    {
        console.log("waiting for flow control signal to resume...")
    }
}

function testUartPassthrough(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }

    async.series([
        function(callback) {
            dataIndex = 0;
            dataRxDoneCallback = callback;
            sendData();
        },
        function(err) {
            var actual = new Buffer(receivedData)
            if(utils.compareBuffers(actual, passthrough_data)) {
                testResult = 'PASS'
            } else {
                testResult = 'FAIL'
                testNote = 'Received Data did not match Expected Data'
                utils.log(5, 'Parity Fail')
                utils.log(5, "exp: " + utils.bytesToHexString(passthrough_data))
                utils.log(5, "act: " + utils.bytesToHexString(actual))
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
        return 'Uart Parity'
    }

//run all tests
module.exports = {
    testRunner: testRunner,
    getName: getName
}

commander.
    version('1.0.1').
    usage('[options]').
    option('-r, --run', 'Run as stand alone test').
    option('-b, --baud <baud>', 'baudrate', /^(1000000|921600|460800|230400|115200|57600|38400|19200|9600|4800|2400|1200)$/i).
    option('-l, --datalen <datalen>', 'test data length', "1024").
    parse(process.argv);

if(commander.baud && isNaN(parseInt(commander.baud,10))) {
    utils.log(1, "illegal baudrate specified")    
    process.exit()
}
else if(commander.baud) {
    baudRate = parseInt(commander.baud,10)
}

//I have no idea why I cannot get the commander parseInt to work as expected, this is hacky at best.
var length = parseInt(commander.datalen)
if(commander.datalen == null || length == 0 || isNaN(length)) {
    utils.log(1, "invalid data length specified")    
    process.exit()
}
else
{
    commander.datalen = length
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
