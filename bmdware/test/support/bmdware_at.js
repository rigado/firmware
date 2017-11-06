#!/usr/bin/env nodejs

var noble = require('noble')
var ble = require('./ble')
var utils = require('./utils')

/* Beacon */
const AT_BEACON_ENABLE_CMD              = 'at$ben'
const AT_BEACON_UUID_CMD                = 'at$buuid'
const AT_BEACON_MAJOR_CMD               = 'at$bmjid'
const AT_BEACON_MINOR_CMD               = 'at$bmnid'
const AT_BEACON_ADV_INT_CMD             = 'at$badint'
const AT_BEACON_TX_POWER_CMD            = 'at$btxpwr'
const AT_CUSTOM_BEACON_DATA_CMD         = 'at$cusbcn'
const AT_CUSTOM_BEACON_DATA_CLEAR_CMD   = 'at$cbclr'
const AT_RSSI_CALIBRATION_CMD           = 'at$bcal'

/* Misc */
const AT_VERSION_CMD                    = 'at$ver'
const AT_BOOTLOADER_VER_CMD             = 'at$blver'
const AT_CONNECT_TX_POWER_CMD           = 'at$ctxpwr'
const AT_CONNECT_ADV_ENABLE_CMD         = 'at$conadv'
const AT_PASSWORD_CMD                   = 'at$password'
const AT_UNLOCK_CMD                     = 'at$unlock'
const AT_DEV_RESET_CMD                  = 'at$devrst'
const AT_RESET_DEFAULTS_CMD             = 'at$defaults'
const AT_CON_ADV_CMD                    = 'at$conadv'
const AT_CON_ADV_INT_CMD                = 'at$cadint'
const AT_HW_INFO_CMD                    = 'at$hwinfo'
const AT_HOTSWAP_CMD                    = 'at$hotswap'
const AT_MAC_CMD                        = 'at$mac'


// GPIO commands
const AT_GPIO_CONF_CMD                  = 'at$gcfg'
const AT_GPIO_WRITE_CMD                 = 'at$gset'
const AT_GPIO_READ_CMD                  = 'at$gread'
const AT_GPIO_CONF_GET_CMD              = 'at$gcget'

// Uart commands
const AT_UART_BAUD_CMD    = 'at$ubr'
const AT_UART_PARITY_CMD  = 'at$upar'
const AT_UART_FLOW_CMD    = 'at$ufc'
const AT_UART_ENABLE_CMD  = 'at$uen'

// GPIO Pins
const AT_P0_01 = '00'
const AT_P0_24 = '01'
const AT_P0_25 = '02'
const AT_P0_00 = '03'
const AT_P0_04 = '04'
const AT_P0_06 = '05'
const AT_P0_08 = '06'

const AT_N52_P0_00 = '00'
const AT_N52_P0_01 = '01'
const AT_N52_P0_02 = '02'
const AT_N52_P0_03 = '03'
const AT_N52_P0_04 = '04'
const AT_N52_P0_09 = '05'
const AT_N52_P0_10 = '06'
const AT_N52_P0_15 = '07'
const AT_N52_P0_16 = '08'
const AT_N52_P0_17 = '09'
const AT_N52_P0_18 = '0A'
const AT_N52_P0_19 = '0B'
const AT_N52_P0_20 = '0C'
const AT_N52_P0_21 = '0D'
const AT_N52_P0_22 = '0E'
const AT_N52_P0_23 = '0F'
const AT_N52_P0_24 = '10'
const AT_N52_P0_25 = '11'
const AT_N52_P0_26 = '12'
const AT_N52_P0_27 = '13'
const AT_N52_P0_28 = '14'
const AT_N52_P0_29 = '15'
const AT_N52_P0_30 = '16'
const AT_N52_P0_31 = '17'

const AT_GPIO_MIN = '00'
const AT_GPIO_N51_MAX = '06'
const AT_GPIO_N51_INVALID = '07'
const AT_GPIO_N52_MAX = '17'
const AT_GPIO_N52_INVALID = '18'

// Pin Direction
const AT_DIRECTION_IN  = '00'
const AT_DIRECTION_OUT = '01'

// Pin Pull-up/down
const AT_PULL_NONE = '00'
const AT_PULL_DOWN = '01'
const AT_PULL_UP   = '03'

// Pin state
const AT_STATE_HIGH = '01'
const AT_STATE_LOW  = '00'

// Return Code
const RC_OK     = 'OK'
const RC_LOCKED = 'LOCKED'
const RC_ERROR  = 'ERR'

const AT_NRF51_HW_ID = 'NRF51'
const AT_NRF52_HW_ID = 'NRF52'

var uartTimer
var callbackAfterDataReceive
var rxData

var atDataCallback
var atDataThrottle = 150

/* General methods begin */
function configureAtReceiveNotifications(port, onData) {
    atDataCallback = onData
    port.on('data', function(data) {
        utils.log(5, 'onData:' + data)    
        var delay = setTimeout(function() {
            delay = null;
            rxData = new Buffer(data, 'ascii')
            onData(rxData)
        }, atDataThrottle)
    })
}

function disableBleReceiveNotifications(port, onData, callback) {
        port.removeListener('read', onData)
        callback()
}

function writeAtCommand(port, data, callback) {
    port.write(data, function(err, results) {
        utils.checkError(err)
        utils.log(5, 'Send results: ' + results)
        utils.log(5, 'Send Data: ' + data)
        if(callback) {
            callback()
        }
   })
}

function writeAtCommandTimeOut(port, data, timeout, callback) {
    callbackAfterDataReceive = callback
    uartTimer = setTimeout(function() {
        uartTimer = null;
        testNode = 'uart time out'
        utils.log(5,  "Time Out")
        callbackAfterDataReceive(false)
    }, timeout)
    writeAtCommand(port, data, null)
}
/* General methods end */

/* Beacon methods begin */
function getBeaconEnable(port, callback) {
	var buf = new Buffer(AT_BEACON_ENABLE_CMD + '?\n', 'ascii')
	writeAtCommand(port, buf, callback)
}

function setBeaconEnable(port, enable, callback) {
	var buf
    
	if(enable) {
		buf = new Buffer(AT_BEACON_ENABLE_CMD + ' 01\n', 'ascii')
	} else {
		buf = new Buffer(AT_BEACON_ENABLE_CMD + ' 00\n', 'ascii')
	}
	writeAtCommand(port, buf, callback)
}

function getBeaconUuid(port, callback) {
	var buf = new Buffer(AT_BEACON_UUID_CMD + '?\n', 'ascii')
	writeAtCommand(port, buf, callback)
}

function setBeaconUuid(port, uuid, callback) {
    var buf

    buf = new Buffer(AT_BEACON_UUID_CMD + ' ' + uuid + '\n', 'ascii')

    writeAtCommand(port, buf, callback)
}

function getBeaconMajor(port, callback) {
    var buf = new Buffer(AT_BEACON_MAJOR_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function setBeaconMajor(port, major, callback) {
    var buf

    buf = new Buffer(AT_BEACON_MAJOR_CMD + ' ' + major + '\n', 'ascii')

    writeAtCommand(port, buf, callback)
}

function getBeaconMinor(port, callback) {
    var buf = new Buffer(AT_BEACON_MINOR_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function setBeaconMinor(port, minor, callback) {
    var buf

    buf = new Buffer(AT_BEACON_MINOR_CMD + ' ' + minor + '\n', 'ascii')

    writeAtCommand(port, buf, callback)
}

function getBeaconAdvertisingInterval(port, callback) {
    var buf = new Buffer(AT_BEACON_ADV_INT_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function setBeaconAdvertisingInterval(port, interval, callback) {
    var buf

    buf = new Buffer(AT_BEACON_ADV_INT_CMD + ' ' + interval + '\n', 'ascii')

    writeAtCommand(port, buf, callback)
}

function getBeaconTxPower(port, callback) {
	var buf = new Buffer(AT_BEACON_TX_POWER_CMD + '?\n', 'ascii')
	writeAtCommand(port, buf, callback)
}

function setBeaconTxPower(port, power, callback) {
    var buf

    buf = new Buffer(AT_BEACON_TX_POWER_CMD + ' ' + power + '\n', 'ascii')

    writeAtCommand(port, buf, callback)
}

function getCustomBeaconData(port, callback) {
    var buf = new Buffer(AT_CUSTOM_BEACON_DATA_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}
function setCustomBeaconData(port, data, callback) {
    var buf

    buf = new Buffer(AT_CUSTOM_BEACON_DATA_CMD + ' ' + data + '\n', 'ascii')

    writeAtCommand(port, buf, callback)
}

function clearCustomBeaconData(port, callback) {
    var buf = new Buffer(AT_CUSTOM_BEACON_DATA_CLEAR_CMD + '\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function getRssiCalibrationData(port, callback) {
    var buf = new Buffer(AT_RSSI_CALIBRATION_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function setRssiCalibrationData(port, power, rssi, callback) {
    var buf = new Buffer(AT_RSSI_CALIBRATION_CMD + ' ' + power + rssi + '\n', 'ascii')
    writeAtCommand(port, buf, callback)
}
/* Beacon methods end */

/* Miscellaneous methods begin */
function getVersion(port, callback) {
    var buf = new Buffer(AT_VERSION_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function getBootloaderVersion(port, callback) {
    var buf = new Buffer(AT_BOOTLOADER_VER_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function getConnectableTxPower(port, callback) {
    var buf = new Buffer(AT_CONNECT_TX_POWER_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function setConnectableTxPower(port, power, callback) {
    var buf = new Buffer(AT_CONNECT_TX_POWER_CMD + ' ' + power + '\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function getConnectableAdvEnable(port, callback) {
    var buf = new Buffer(AT_CONNECT_ADV_ENABLE_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function setConnectableAdvEnable(port, enable, callback) {
    var buf

    if(enable) {
            buf = new Buffer(AT_CONNECT_ADV_ENABLE_CMD + ' 01\n', 'ascii')
    } else {
            buf = new Buffer(AT_CONNECT_ADV_ENABLE_CMD + ' 00\n', 'ascii')
    }
    writeAtCommand(port, buf, callback)
}

function getConnectableAdvertisingInterval(port, callback) {
    var buf = new Buffer(AT_CON_ADV_INT_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function setConnectableAdvertisingInterval(port, interval, callback) {
    var buf = new Buffer(AT_CON_ADV_INT_CMD + ' ' + interval + '\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function setPassword(port, passwd, callback) {
	if(!passwd || passwd.length < 1) {
        if(callback) { 
		  callback()
        }
	}
    var buf = new Buffer(AT_PASSWORD_CMD + ' ' + passwd + '\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function unlockDevice(port, passwd, callback) {
	if(!passwd || passwd.length < 1) {
        if(callback) {
            callback()    
        }
	}
    var buf = new Buffer(AT_UNLOCK_CMD + ' ' + passwd + '\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function getHardwareInfo(port, callback) {
    var buf = new Buffer(AT_HW_INFO_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function reset(port, callback) {
    var buf = new Buffer(AT_DEV_RESET_CMD + '\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function resetDefaultConfiguration(port, callback) {
    var buf = new Buffer(AT_RESET_DEFAULTS_CMD + '\n', 'ascii')
    writeAtCommand(port, buf, callback)
    port.flush()
}

function getHotswapEnable(port, callback) {
    var buf = new Buffer(AT_HOTSWAP_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function setHotswapEnable(port, enable, callback) {
    var buf

    if(enable) {
            buf = new Buffer(AT_HOTSWAP_CMD + ' 01\n', 'ascii')
    } else {
            buf = new Buffer(AT_HOTSWAP_CMD + ' 00\n', 'ascii')
    }
    writeAtCommand(port, buf, callback)
}

function getMac(port, callback) {
    var buf = new Buffer(AT_MAC_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}


/* Gpio methods begin */
function setGpioConfig(port, pin, direction, pull, callback) {
    var buf

    buf = new Buffer(AT_GPIO_CONF_CMD + ' ' + pin + ' ' +
                     direction + ' ' + pull + '\n', 'ascii')

    writeAtCommand(port, buf, callback)
}

function writeGpio(port, pin, state, callback) {
    var buf

    buf = new Buffer(AT_GPIO_WRITE_CMD + ' ' + pin + ' ' +
                     state + '\n', 'ascii')

    writeAtCommand(port, buf, callback)
}

function readGpio(port, pin, callback) {
    var buf

    buf = new Buffer(AT_GPIO_READ_CMD + ' ' + pin + '\n', 'ascii')

    writeAtCommand(port, buf, callback)
}

function getGpioConfig(port, pin, callback) {
    var buf

    buf = new Buffer(AT_GPIO_CONF_GET_CMD + ' ' + pin + '\n', 'ascii')

    writeAtCommand(port, buf, callback)
}
/* Gpio methods end */

/* Uart methods begin */
function getUartBaudRate(port, callback) {
    var buf = new Buffer(AT_UART_BAUD_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}
function setUartBaudRate(port, baudRate, callback) {
    var buf = new Buffer(AT_UART_BAUD_CMD + ' ' + baudRate + '\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function getUartParityEnable(port, callback) {
    var buf = new Buffer(AT_UART_PARITY_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function setUartParityEnable(port, enable, callback) {
    var buf

    if(enable) {
            buf = new Buffer(AT_UART_PARITY_CMD + ' 01\n', 'ascii')
    } else {
            buf = new Buffer(AT_UART_PARITY_CMD + ' 00\n', 'ascii')
    }
    writeAtCommand(port, buf, callback)
}

function getUartFlowControlEnable(port, callback) {
    var buf = new Buffer(AT_UART_FLOW_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function setUartFlowControlEnable(port, enable, callback) {
    var buf

    if(enable) {
            buf = new Buffer(AT_UART_FLOW_CMD + ' 01\n', 'ascii')
    } else {
            buf = new Buffer(AT_UART_FLOW_CMD + ' 00\n', 'ascii')
    }
    writeAtCommand(port, buf, callback)
}

function getUartEnable(port, callback) {
    var buf = new Buffer(AT_UART_ENABLE_CMD + '?\n', 'ascii')
    writeAtCommand(port, buf, callback)
}

function setUartEnable(port, enable, callback) {
    var buf

    if(enable) {
            buf = new Buffer(AT_UART_ENABLE_CMD + ' 01\n', 'ascii')
    } else {
            buf = new Buffer(AT_UART_ENABLE_CMD + ' 00\n', 'ascii')
    }
    writeAtCommand(port, buf, callback)
}

function sendAt(port, callback) {
    var buf = new Buffer('AT\n', 'ascii')
    writeAtCommand(port, buf, callback)
}
/* Uart methods end */

module.exports = {
    // General methods
	configureAtReceiveNotifications: configureAtReceiveNotifications,
	disableBleReceiveNotifications: disableBleReceiveNotifications,
	writeAtCommand: writeAtCommand,
	writeAtCommandTimeOut: writeAtCommandTimeOut,

    // Beacon methods
    getBeaconEnable: getBeaconEnable,
	setBeaconEnable: setBeaconEnable,
	getBeaconUuid: getBeaconUuid,
	setBeaconUuid: setBeaconUuid,
	getBeaconMajor: getBeaconMajor,
	setBeaconMajor: setBeaconMajor,
	getBeaconMinor: getBeaconMinor,
	setBeaconMinor: setBeaconMinor,
	getBeaconAdvertisingInterval: getBeaconAdvertisingInterval,
	setBeaconAdvertisingInterval: setBeaconAdvertisingInterval,
	getBeaconTxPower: getBeaconTxPower,
	setBeaconTxPower: setBeaconTxPower,
	
    getCustomBeaconData: getCustomBeaconData,
    setCustomBeaconData: setCustomBeaconData,
    clearCustomBeaconData: clearCustomBeaconData,
    getRssiCalibrationData: getRssiCalibrationData,
    setRssiCalibrationData: setRssiCalibrationData,
	
    // Miscellaneous methods
	getVersion: getVersion,
    getBootloaderVersion: getBootloaderVersion,
    getConnectableTxPower: getConnectableTxPower,
    setConnectableTxPower: setConnectableTxPower,
    getConnectableAdvertisingInterval: setConnectableAdvertisingInterval,
    setConnectableAdvertisingInterval: setConnectableAdvertisingInterval,
    getConnectableAdvEnable: getConnectableAdvEnable,
    setConnectableAdvEnable: setConnectableAdvEnable,
	getHardwareInfo: getHardwareInfo,
    setPassword: setPassword,
	unlockDevice: unlockDevice,
	reset: reset,
	resetDefaultConfiguration: resetDefaultConfiguration,
    getHotswapEnable: getHotswapEnable,
    setHotswapEnable: setHotswapEnable,
    getMac: getMac,
	
	// GPIO methods
	setGpioConfig: setGpioConfig,
	writeGpio: writeGpio,
	readGpio: readGpio,
	getGpioConfig: getGpioConfig,

	// UART methods
	getUartBaudRate: getUartBaudRate,
	setUartBaudRate: setUartBaudRate,
	getUartParityEnable: getUartParityEnable,
	setUartParityEnable: setUartParityEnable,
	getUartFlowControlEnable: getUartFlowControlEnable,
	setUartFlowControlEnable: setUartFlowControlEnable,
	getUartEnable: getUartEnable,
	setUartEnable: setUartEnable,

    // Export AT check method
    sendAt: sendAt,

	// Export Return Code
	RC_OK: RC_OK,
	RC_LOCKED: RC_LOCKED,
	RC_ERROR: RC_ERROR,

    // GPIO const
	AT_GPIO_CONF_CMD: AT_GPIO_CONF_CMD,
	AT_GPIO_WRITE_CMD: AT_GPIO_WRITE_CMD,
	AT_GPIO_READ_CMD: AT_GPIO_READ_CMD,
	AT_GPIO_CONF_GET_CMD: AT_GPIO_CONF_GET_CMD,

	AT_P0_01: AT_P0_01,
	AT_P0_24: AT_P0_24,
	AT_P0_25: AT_P0_25,
	AT_P0_00: AT_P0_00,
	AT_P0_04: AT_P0_04,
	AT_P0_06: AT_P0_06,
	AT_P0_08: AT_P0_08,

	AT_DIRECTION_IN: AT_DIRECTION_IN,
	AT_DIRECTION_OUT: AT_DIRECTION_OUT,
	AT_PULL_NONE: AT_PULL_NONE,
	AT_PULL_DOWN: AT_PULL_DOWN,
	AT_PULL_UP: AT_PULL_UP,
	AT_STATE_HIGH: AT_STATE_HIGH,
	AT_STATE_LOW: AT_STATE_LOW,

    AT_N52_P0_00: AT_N52_P0_00,
    AT_N52_P0_01: AT_N52_P0_01,
    AT_N52_P0_02: AT_N52_P0_02,
    AT_N52_P0_03: AT_N52_P0_03,
    AT_N52_P0_04: AT_N52_P0_04,
    AT_N52_P0_09: AT_N52_P0_09,
    AT_N52_P0_10: AT_N52_P0_10,
    AT_N52_P0_15: AT_N52_P0_15,
    AT_N52_P0_16: AT_N52_P0_16,
    AT_N52_P0_17: AT_N52_P0_17,
    AT_N52_P0_18: AT_N52_P0_18,
    AT_N52_P0_19: AT_N52_P0_19,
    AT_N52_P0_20: AT_N52_P0_20,
    AT_N52_P0_21: AT_N52_P0_21,
    AT_N52_P0_22: AT_N52_P0_22,
    AT_N52_P0_23: AT_N52_P0_23,
    AT_N52_P0_24: AT_N52_P0_24,
    AT_N52_P0_25: AT_N52_P0_25,
    AT_N52_P0_26: AT_N52_P0_26,
    AT_N52_P0_27: AT_N52_P0_27,
    AT_N52_P0_28: AT_N52_P0_28,
    AT_N52_P0_29: AT_N52_P0_29,
    AT_N52_P0_30: AT_N52_P0_30,
    AT_N52_P0_31: AT_N52_P0_31,
    AT_GPIO_MIN: AT_GPIO_MIN,
    AT_GPIO_N51_MAX: AT_GPIO_N51_MAX, 
    AT_GPIO_N51_INVALID: AT_GPIO_N51_INVALID,
    AT_GPIO_N52_MAX: AT_GPIO_N52_MAX,
    AT_GPIO_N52_INVALID: AT_GPIO_N52_INVALID,

    AT_NRF51_HW_ID: AT_NRF51_HW_ID,
    AT_NRF52_HW_ID: AT_NRF52_HW_ID,

	uartTimer: uartTimer,
	callbackAfterDataReceive: callbackAfterDataReceive,
}
