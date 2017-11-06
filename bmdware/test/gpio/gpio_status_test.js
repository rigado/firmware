#!/usr/bin/env nodejs

var noble = require('noble');
var ble = require('../support/ble')
var utils = require('../support/utils')
var bmdware = require('../support/bmdware')
var async = require('async')
var commander = require('commander')

var testConfig
var testResult = 'FAIL'
var testNote = ''
var testShouldContinue = true
var test_status_count = 0

var expected_results = []
var results = []

var onDataCompleteCallback

function testSetup(setupCompleteCallback) {
    async.series([
        function(callback) {
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
            var result = utils.compareBuffers(expected, data)
            results.push(result)
        }
        onDataCompleteCallback()
    } else {
        utils.log(5, 'onDataCompleteCallback is null?!?!')
    }
}

function testGpioStatus(testCompleteCallback) {
    if(!testShouldContinue) {
        testCompleteCallback()
        return
    }
    async.series([
        function(callback) {
            utils.log(5, "GPIO status test connect")
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
            utils.log(5, "Notification enable")
            bmdware.configureBleReceiveNotifications(onData, callback)
        },
        function(callback) {
            utils.log(5, "Configure GPIO P0.17 (LED1) as status pin")
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_SUCCESS]))
            bmdware.setGpioStatConfig(bmdware.P0_17, bmdware.POLARITY_LOW, null)
        },
        function(callback) {
            utils.log(5, "Get status configuration GPIO P0.17")
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.CP_GPIO_STAT_CONFIG_GET, bmdware.P0_17, bmdware.POLARITY_LOW]))
            bmdware.getGpioStatConfig(null)
        },
        function(callback) {
            utils.log(5, "Read status pin")
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.CP_GPIO_STAT_READ, bmdware.STATE_HIGH]))
            bmdware.readGpioStat(null)
        },
        function(callback) {
            utils.log(5, "Deconfigure status pin")
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.RC_SUCCESS]))
            bmdware.setGpioStatDeconfig(null)
        },
        function(callback) {
            utils.log(5, "Get status configuration GPIO P0.17")
            onDataCompleteCallback = callback
            expected_results.push(new Buffer([bmdware.CP_GPIO_CONFIG_GET, bmdware.P0_17, bmdware.DIRECTION_IN, bmdware.PULL_NONE]))
            bmdware.getGpioConfig(bmdware.P0_17, null)
        },
        function(callback) {
            utils.log(5, "GPIO status test disconnect")
            ble.disconnectPeripheralUT(function(disconnectResult) {
                if(!disconnectResult) {
                    testNote = 'Failed to disconnect'
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
            utils.log(5, "GPIO status test done")
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
    async.series([
        function(callback) {
            testSetup(callback)
        },
        function(callback) {
            testGpioStatus(callback)
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
        return 'GPIO status Test'
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
