#!/usr/bin/env nodejs

var noble = require('noble')
var ble = require('./ble')
var utils = require('./utils')

var BMDWARE_UART_SERVICE_UUID       = '6e400001b5a3f393e0a9e50e24dcca9e'
var BMDWARE_BEACON_BASE_UUID		= '24130000707f90bd20452ab8807571b7'
var BMDWARE_BEACON_SERVICE_UUID     = 'b33f'
var BMDWARE_CONTROL_POINT_UUID 		= 'b43f'
NOTIFICATION_ENABLE  = 0x0001
NOTIFICATION_DISABLE = 0x0000

var BMDWARE_BEACON_UUID        		= 'b53f'
var BMDWARE_MAJOR_NUMBER_UUID  		= 'b63f'
var BMDWARE_MINOR_NUMBER_UUID  		= 'b73f'
var BMDWARE_ADV_INT_UUID			= 'b83f'
var BMDWARE_BEACON_TX_POWER_UUID    = 'b93f'
var BMDWARE_BEACON_ENABLE_UUID 		= 'ba3f'
var BMDWARE_CONNECT_TX_POWER_UUID   = 'bb3f'
const TXPOWER_HIGH    = 4
const TXPOWER_DEFAULT = -4
const TXPOWER_LOW     = -30

var BMDWARE_UART_BASE_UUID			= '6e400000b5a3f393e0a9e50e24dcca9e'
var BMDWARE_UART_SERVICE_UUID		= '00001'
var BMDWARE_UART_TX_UUID			= '00003'
var BMDWARE_UART_RX_UUID			= '00002'
var BMDWARE_UART_BAUD_UUID			= '00004'
var BMDWARE_UART_PARITY_UUID		= '00005'
var BMDWARE_UART_FLOW_UUID			= '00006'
var BMDWARE_UART_ENABLE_UUID		= '00008'

var OP_ENABLE = 1
var OP_DISABLE = 0

var DEFAULT_BEACON_UUID = '00112233445566778899aabbccddeeff'
var DEFAULT_BEACON_MAJOR = '0000'
var DEFAULT_BEACON_MINOR = '0000'

// Define constants for the returned error code
const RC_SUCCESS           = 0
const RC_LOCKED            = 1
const RC_INVALID_LENGTH    = 2
const RC_UNLOCKED_FAILED   = 3
const RC_UPDATE_PIN_FAIED  = 4
const RC_INVALID_DATA      = 5
const RC_INVALID_STATE     = 6
const RC_INVALID_PARAMETER = 7
const RC_INVALID_COMMAND   = 8
var returnCodeStr = [
    "Command Success",
    "Device Locked",
    "Command Invalid Length",
    "Unlock Failed",
    "Update Pin Failed",
    "Invalid Data",
    "Invalid State",
    "Invalid Parameter",
    "Invalid Command"
]
// Control Point command for accessing RSSI Calibration data
const CP_RSSI_CALIBRATION_WRITE = 0x40
const CP_RSSI_CALIBRATION_READ  = 0x41

// Set Passwd and unlock device CP commands
const CP_PASSWD_WRITE           = 0x31
const CP_UNLOCK                 = 0xf8

// Set or clear Custom Beacon Data CP commands
const CP_BEACON_DATA1_WRITE     = 0x20
const CP_BEACON_DATA2_WRITE     = 0x21
const CP_BEACON_DATA_SAVE       = 0x22
const CP_BEACON_DATA_CLEAR      = 0x23

// GPIO commands
// Characteristic methods
const CP_GPIO_CONFIG_SET      = 0x50
const CP_GPIO_WRITE           = 0x51
const CP_GPIO_READ            = 0x52
const CP_GPIO_CONFIG_GET      = 0x53
const CP_GPIO_STAT_CONFIG_SET = 0x54
const CP_GPIO_STAT_DECONFIG   = 0x55
const CP_GPIO_STAT_CONFIG_GET = 0x56
const CP_GPIO_STAT_READ       = 0x57

//------------------------------------------------------------------------------
//	BMD-300 Pin	Name	Pin Map Index	Status
//	13		P0.00		0x00			XTAL1
//	14		P0.01		0x01			XTAL2
//	15		P0.02		0x02
//	19		P0.03		0x03
//	20		P0.04		0x04
//	25		P0.09		0x05
//	26		P0.10		0x06
//	33		P0.15		0x07			BTN3
//	34		P0.16		0x08			BTN4
//	35		P0.17		0x09			LED1
//	36		P0.18		0x0A			LED2
//	37		P0.19		0x0B			LED3
//	38		P0.20		0x0C			LED4
//	20		P0.22		0x0D
//	41		P0.23		0x0E
//	42		P0.24		0x0F
//	6		P0.25		0x10
//	7		P0.26		0x11
//	8		P0.27		0x12
//	9		P0.28		0x13
//	10		P0.29		0x14
//	11		P0.30		0x15
//	12		P0.31		0x16
//------------------------------------------------------------------------------
const P0_00 = 0x00		//	XTAL1
const P0_01 = 0x01		//	XTAL2
const P0_02 = 0x02
const P0_03 = 0x03
const P0_04 = 0x04
const P0_09 = 0x05
const P0_10 = 0x06
const P0_15 = 0x07		//	BTN3
const P0_16 = 0x08		//	BTN4
const P0_17 = 0x09		//	LED1
const P0_18 = 0x0A		//	LED2
const P0_19 = 0x0B		//	LED3
const P0_20 = 0x0C		//	LED4
const P0_22 = 0x0D
const P0_23 = 0x0E
const P0_24 = 0x0F
const P0_25 = 0x10
const P0_26 = 0x11
const P0_27 = 0x12
const P0_28 = 0x13
const P0_29 = 0x14
const P0_30 = 0x15
const P0_31 = 0x16

// Pin Direction
const DIRECTION_IN  = 0x00
const DIRECTION_OUT = 0x01

// Pin Polarity
const POLARITY_LOW  = 0x00
const POLARITY_HIGH = 0x01

// Pin Pull-up/down
const PULL_NONE = 0x00
const PULL_DOWN = 0x01
const PULL_UP   = 0x03

// Pin state
const STATE_HIGH      = 0x01
const STATE_LOW       = 0x00
const STATE_ACTIVE    = 0x01
const STATE_INACTIVE  = 0x00

function getCharacteristicForUuid(base, uuid) {
	var peripheral = ble.getPeripheralUT()
	var characteristic = {}
	if(peripheral) {
		var charList = ble.getPeripheralUTCharacteristics()
		characteristic = charList[ble.fullUuidFromBase(base, uuid)]
		if(!characteristic) {
			utils.log(1, 'Characteristic not found %s!', ble.fullUuidFromBase(base, uuid))
		}
		else {
			utils.log(5, 'Found Characteristic %s',  ble.fullUuidFromBase(base, uuid))
        }
	} else {
		utils.log(1, 'No available peripheral!')
		process.exit(1)
	}
	return characteristic
}

function writeCharacteristic(baseUuid, shortUuid, data, callback) {
	var characteristic = getCharacteristicForUuid(baseUuid, shortUuid)

	if(!characteristic) {
		utils.log(1, "Characteristic not found!")
		process.exit(1)
	}

	characteristic.write(data, false, function(err) {
		utils.checkError(err)
		if(callback) {
			callback()
		}
	})
}

function readCharacteristic(baseUuid, shortUuid, callback) {
	var characteristic = getCharacteristicForUuid(baseUuid, shortUuid)
	if(!characteristic) {
		utils.log(1, "Characteristic not found!")
                return callback(new Error('Characteristic not found!'))
	}
        characteristic.read(callback)
}

/* Beacon control methods */

function getBeaconEnable(callback) {
	readCharacteristic(BMDWARE_BEACON_BASE_UUID, BMDWARE_BEACON_ENABLE_UUID, callback)
}

function setBeaconEnable(enable, callback) {
	var buf = new Buffer(1);

    if(enable) {
    	buf.writeUInt8(OP_ENABLE, 0);
    } else {
    	buf.writeUInt8(OP_DISABLE, 0)
    }

	writeCharacteristic(BMDWARE_BEACON_BASE_UUID, BMDWARE_BEACON_ENABLE_UUID, buf, callback)
}

function getBeaconUuid(callback) {
	readCharacteristic(BMDWARE_BEACON_BASE_UUID, BMDWARE_BEACON_UUID, callback)
}
function setBeaconUuid(uuid, callback) {
	var uuidToWrite = uuid
	if(uuid == null) {
		uuidToWrite = DEFAULT_BEACON_UUID
	}

	var buf = utils.hexStringToBytes(uuidToWrite)
	if(!buf) {
		utils.log(1, 'Error creating UUID array!')
		process.exit(1)
	}
	utils.log(5, "buf = " + buf.toString('hex'))
	writeCharacteristic(BMDWARE_BEACON_BASE_UUID, BMDWARE_BEACON_UUID, buf, callback)
}

function getBeaconMajor(callback) {
	readCharacteristic(BMDWARE_BEACON_BASE_UUID, BMDWARE_MAJOR_NUMBER_UUID, callback)
}
function setBeaconMajor(major, callback) {
	var buf = new Buffer(2)

	if(major == null)
	{
		major = DEFAULT_BEACON_MAJOR
	}

	buf.writeUInt16LE(major, 0)
	writeCharacteristic(BMDWARE_BEACON_BASE_UUID, BMDWARE_MAJOR_NUMBER_UUID, buf, callback)
}

function getBeaconMinor(callback) {
	readCharacteristic(BMDWARE_BEACON_BASE_UUID, BMDWARE_MINOR_NUMBER_UUID, callback)
}
function setBeaconMinor(minor, callback) {
	var testConfig = ble.getConfiguration()
	var buf = new Buffer(2)

	if(minor == null) {
		minor = DEFAULT_BEACON_MINOR
	}

	buf.writeUInt16LE(minor, 0)
	writeCharacteristic(BMDWARE_BEACON_BASE_UUID, BMDWARE_MINOR_NUMBER_UUID, buf, callback)
}

function getBeaconAdvertisingInterval(callback) {
	readCharacteristic(BMDWARE_BEACON_BASE_UUID, BMDWARE_ADV_INT_UUID, callback)
}
function setBeaconAdvertisingInterval(interval, callback) {
	var buf = new Buffer(2)

	buf.writeUInt16LE(interval)
	writeCharacteristic(BMDWARE_BEACON_BASE_UUID, BMDWARE_ADV_INT_UUID, buf, callback)
}

function getBeaconTxPower(callback) {
	readCharacteristic(BMDWARE_BEACON_BASE_UUID, BMDWARE_BEACON_TX_POWER_UUID, callback)
}
function setBeaconTxPower(power, callback) {
	var buf = new Buffer(1)

	buf.writeInt8(power, 0)
        writeCharacteristic(BMDWARE_BEACON_BASE_UUID, BMDWARE_BEACON_TX_POWER_UUID, buf, callback)
}

function getConnectableTxPower(callback) {
	readCharacteristic(BMDWARE_BEACON_BASE_UUID, BMDWARE_CONNECT_TX_POWER_UUID, callback)
}
function setConnectableTxPower(power, callback) {
	var buf = new Buffer(1)

	buf.writeInt8(power, 0)
	writeCharacteristic(BMDWARE_BEACON_BASE_UUID, BMDWARE_CONNECT_TX_POWER_UUID, buf, callback)
}

function writeControlPoint(data, callback) {
	if(!data) {
		callback()
	}

	writeCharacteristic(BMDWARE_BEACON_BASE_UUID, BMDWARE_CONTROL_POINT_UUID, data, callback)
}

function setControlPoint(data, callback) {
	var buf = new Buffer(2)

	buf.writeUInt16LE(data, 0)
	writeCharacteristic(BMDWARE_BEACON_BASE_UUID, BMDWARE_CONTROL_POINT_UUID, buf, callback)
}

function configureBleReceiveNotifications(onData, callback) {
	var rxCharacteristic = getCharacteristicForUuid(BMDWARE_BEACON_BASE_UUID, BMDWARE_CONTROL_POINT_UUID)
	rxCharacteristic.notify(true, function(err) {
		if(!utils.checkError(err)) {
			utils.log(1, 'Error enabling BLE receive notifications')
			return
		}
		rxCharacteristic.on('read', onData)
		callback()
	})
}

function disableBleReceiveNotifications(onData, callback) {
	var rxCharacteristic = getCharacteristicForUuid(BMDWARE_BEACON_BASE_UUID, BMDWARE_CONTROL_POINT_UUID)
	rxCharacteristic.removeListener('read', onData)
	callback()
}

function setCustomBeaconData1(data, callback) {
	if(!data || data.length < 1 || data.length > 19) {
		callback()
	}
        var buf = new Buffer(data.length + 1)

    buf.writeUInt8(CP_BEACON_DATA1_WRITE, 0)
	data.copy(buf, 1)
	utils.log(5, "buf = " + buf.toString('hex'))
    writeControlPoint(buf, callback)
}

function setCustomBeaconData2(data, callback) {
	if(!data || data.length < 1 || data.length > 12) {
		callback()
	}
        var buf = new Buffer(data.length + 1)

        buf.writeUInt8(CP_BEACON_DATA2_WRITE, 0)
	data.copy(buf, 1)
	utils.log(5, "buf = " + buf.toString('hex'))
        writeControlPoint(buf, callback)
}

function saveCustomBeaconData(callback) {
        var buf = new Buffer(1)

        buf.writeUInt8(CP_BEACON_DATA_SAVE, 0)
        writeControlPoint(buf, callback)
}

function clearCustomBeaconData(callback) {
        var buf = new Buffer(1)

        buf.writeUInt8(CP_BEACON_DATA_CLEAR, 0)
        writeControlPoint(buf, callback)
}

function setPassword(passwd, callback) {
	if(!passwd || passwd.length < 1) {
		callback()
	}
        var buf = new Buffer(passwd.length + 1)

        buf.writeUInt8(CP_PASSWD_WRITE, 0)
	passwd.copy(buf, 1)
	utils.log(5, "buf = " + buf.toString('hex'))
        writeControlPoint(buf, callback)
}

function unlockDevice(passwd, callback) {
	if(!passwd || passwd.length < 1) {
		callback()
	}
        var buf = new Buffer(passwd.length + 1)

        buf.writeUInt8(CP_UNLOCK, 0)
	passwd.copy(buf, 1)
	utils.log(5, "buf = " + buf.toString('hex'))
        writeControlPoint(buf, callback)
}

function setRssiCalibrationData(power, rssi, callback) {
        var buf = new Buffer(3)

        buf.writeUInt8(CP_RSSI_CALIBRATION_WRITE, 0)
	buf.writeUInt8(power, 1)
	buf.writeUInt8(rssi, 2)
	utils.log(5, "buf = " + buf.toString('hex'))
        writeControlPoint(buf, callback)
}

function getRssiCalibrationData(callback) {
        var buf = new Buffer(1)

        buf.writeUInt8(CP_RSSI_CALIBRATION_READ, 0)
	utils.log(5, "buf = " + buf.toString('hex'))
        writeControlPoint(buf, callback)
}

function startBootloader(callback) {
    var buf = new Buffer('03563057', 'hex')
	utils.log(5, "buf = " + buf.toString('hex'))
    writeControlPoint(buf, callback)
}

function reset(callback) {
	var buf = new Buffer('4b102f37', 'hex')
	writeControlPoint(buf, callback)
}

function resetDefaultConfiguration(callback) {
    var buf = new Buffer('48b0fcd616ab43ac83453daeb999c29c', 'hex')
	utils.log(5, "buf = " + buf.toString('hex'))
	writeControlPoint(buf, callback)
}

// GPIO access methods

function setGpioConfig(pin, direction, pull, callback) {
	var buf = new Buffer(4)

	buf.writeUInt8(CP_GPIO_CONFIG_SET, 0)
	buf.writeUInt8(pin, 1)
	buf.writeUInt8(direction, 2)
	buf.writeUInt8(pull, 3)
	writeControlPoint(buf, callback)
}

function writeGpio(pin, state, callback) {
	var buf = new Buffer(3)

	buf.writeUInt8(CP_GPIO_WRITE, 0)
	buf.writeUInt8(pin, 1)
	buf.writeUInt8(state, 2)
	writeControlPoint(buf, callback)
}

function readGpio(pin, callback) {
	var buf = new Buffer(2)

	buf.writeUInt8(CP_GPIO_READ, 0)
	buf.writeUInt8(pin, 1)
	writeControlPoint(buf, callback)
}

function getGpioConfig(pin, callback) {
	var buf = new Buffer(2)

	buf.writeUInt8(CP_GPIO_CONFIG_GET, 0)
	buf.writeUInt8(pin, 1)
	writeControlPoint(buf, callback)
}

function setGpioStatConfig(pin, pol, callback) {
	var buf = new Buffer(3)

	buf.writeUInt8(CP_GPIO_STAT_CONFIG_SET, 0)
	buf.writeUInt8(pin, 1)
	buf.writeUInt8(pol, 2)
	writeControlPoint(buf, callback)
}

function setGpioStatDeconfig(callback) {
	var buf = new Buffer(1)

	buf.writeUInt8(CP_GPIO_STAT_DECONFIG, 0)
	writeControlPoint(buf, callback)
}

function getGpioStatConfig(callback) {
	var buf = new Buffer(1)

	buf.writeUInt8(CP_GPIO_STAT_CONFIG_GET, 0)
	writeControlPoint(buf, callback)
}

function readGpioStat(callback) {
	var buf = new Buffer(1)

	buf.writeUInt8(CP_GPIO_STAT_READ, 0)
	writeControlPoint(buf, callback)
}

/* End GPIO control methods */

/* Uart Control methods */
function getUartBaudRate(callback) {
	readCharacteristic(BMDWARE_UART_BASE_UUID, BMDWARE_UART_BAUD_UUID, callback)
}
function setUartBaudRate(baudRate, callback) {
	var buf = new Buffer(4)

	buf.writeUInt32LE(baudRate, 0)
	writeCharacteristic(BMDWARE_UART_BASE_UUID, BMDWARE_UART_BAUD_UUID, buf, callback)
}

function getUartParityEnable(callback) {
	readCharacteristic(BMDWARE_UART_BASE_UUID, BMDWARE_UART_PARITY_UUID, callback)
}
function setUartParityEnable(enable, callback) {
	var buf = new Buffer(1)

	var op = OP_ENABLE
	if(!enable) {
		op = OP_DISABLE
	}

	buf.writeUInt8(op, 0)
	writeCharacteristic(BMDWARE_UART_BASE_UUID, BMDWARE_UART_PARITY_UUID, buf, callback)
}

function getUartFlowControlEnable(callback) {
	readCharacteristic(BMDWARE_UART_BASE_UUID, BMDWARE_UART_FLOW_UUID, callback)
}
function setUartFlowControlEnable(enable, callback) {
	var buf = new Buffer(1)

	var op = OP_ENABLE
	if(!enable) {
		op = OP_DISABLE
	}

	buf.writeUInt8(op, 0)
	writeCharacteristic(BMDWARE_UART_BASE_UUID, BMDWARE_UART_FLOW_UUID, buf, callback)
}

function getUartEnable(callback) {
	readCharacteristic(BMDWARE_UART_BASE_UUID, BMDWARE_UART_ENABLE_UUID, callback)
}
function setUartEnable(enable, callback) {
	var buf = new Buffer(1)

	var op = OP_ENABLE
	if(!enable) {
		op = OP_DISABLE
	}

	buf.writeUInt8(op, 0)
	writeCharacteristic(BMDWARE_UART_BASE_UUID, BMDWARE_UART_ENABLE_UUID, buf, callback)
}

function configureUartReceiveNotifications(onData, callback) {
	var rxCharacteristic = getCharacteristicForUuid(BMDWARE_UART_BASE_UUID, BMDWARE_UART_TX_UUID)
	rxCharacteristic.notify(true, function(err) {
		if(!utils.checkError(err)) {
			utils.log(1, 'Error enabling UART receive notifications')
			return
		}
		rxCharacteristic.on('read', onData)
		callback()
	})
}

function disableUartReceiveNotifications(onData, callback) {
	var rxCharacteristic = getCharacteristicForUuid(BMDWARE_UART_BASE_UUID, BMDWARE_UART_TX_UUID)
	rxCharacteristic.removeListener('read', onData)
	callback()
}

function writeUartData(data, callback) {
	var buf = new Buffer(data.length)
	buf.write(data)

	writeCharacteristic(BMDWARE_UART_BASE_UUID, BMDWARE_UART_RX_UUID, buf, callback)
}

function writeBufferToUart(buffer, callback) {
	writeCharacteristic(BMDWARE_UART_BASE_UUID, BMDWARE_UART_RX_UUID, buffer, callback)
}
/* End Uart Control methods */

module.exports = {
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
    getBeaconEnable: getBeaconEnable,
	setBeaconEnable: setBeaconEnable,
	getConnectableTxPower: getConnectableTxPower,
	setConnectableTxPower: setConnectableTxPower,
	configureBleReceiveNotifications: configureBleReceiveNotifications,
	disableBleReceiveNotifications: disableBleReceiveNotifications,
	setControlPoint: setControlPoint,
	writeControlPoint: writeControlPoint,

	// Export UART methods
	getUartBaudRate: getUartBaudRate,
	setUartBaudRate: setUartBaudRate,
	getUartParityEnable: getUartParityEnable,
	setUartParityEnable: setUartParityEnable,
	getUartFlowControlEnable: getUartFlowControlEnable,
	setUartFlowControlEnable: setUartFlowControlEnable,
	getUartEnable: getUartEnable,
	setUartEnable: setUartEnable,
	configureUartReceiveNotifications: configureUartReceiveNotifications,
	disableUartReceiveNotifications: disableUartReceiveNotifications,
	writeUartData: writeUartData,
	writeBufferToUart: writeBufferToUart,
	getBmdwareServiceUuids : function() {
		var beaconService = ble.fullUuidFromBase(BMDWARE_BEACON_BASE_UUID, BMDWARE_BEACON_SERVICE_UUID)
		var uartService = ble.fullUuidFromBase(BMDWARE_UART_BASE_UUID, BMDWARE_UART_SERVICE_UUID)
		var uuidList = [ beaconService, uartService ]
		return uuidList
	},

	// Export Control Point commands methods
	setCustomBeaconData1:  setCustomBeaconData1,
	setCustomBeaconData2:  setCustomBeaconData2,
	saveCustomBeaconData:  saveCustomBeaconData,
	clearCustomBeaconData: clearCustomBeaconData,
	setPassword: setPassword,
	unlockDevice: unlockDevice,
	setRssiCalibrationData: setRssiCalibrationData,
	getRssiCalibrationData: getRssiCalibrationData,
	startBootloader: startBootloader,
	reset: reset,
	resetDefaultConfiguration: resetDefaultConfiguration,

	// Export GPIO commands methods
	setGpioConfig: setGpioConfig,
	writeGpio: writeGpio,
	readGpio: readGpio,
	getGpioConfig: getGpioConfig,

	setGpioStatConfig: setGpioStatConfig,
	setGpioStatDeconfig: setGpioStatDeconfig,
	getGpioStatConfig: getGpioStatConfig,
	readGpioStat: readGpioStat,

	// Export TX power constant
	TXPOWER_HIGH: TXPOWER_HIGH,
	TXPOWER_DEFAULT: TXPOWER_DEFAULT,
	TXPOWER_LOW: TXPOWER_LOW,

	// Export Return Code
	RC_SUCCESS: RC_SUCCESS,
	RC_LOCKED: RC_LOCKED,
	RC_INVALID_LENGTH: RC_INVALID_LENGTH,
	RC_UNLOCKED_FAILED: RC_UNLOCKED_FAILED,
	RC_UPDATE_PIN_FAIED: RC_UPDATE_PIN_FAIED,
	RC_INVALID_DATA: RC_INVALID_DATA,
	RC_INVALID_STATE: RC_INVALID_STATE,
	RC_INVALID_PARAMETER: RC_INVALID_PARAMETER,
	RC_INVALID_COMMAND: RC_INVALID_COMMAND,
	returnCodeStr: returnCodeStr,

        // GPIO const
	CP_GPIO_CONFIG_SET: CP_GPIO_CONFIG_SET,
	CP_GPIO_WRITE: CP_GPIO_WRITE,
	CP_GPIO_READ: CP_GPIO_READ,
	CP_GPIO_CONFIG_GET: CP_GPIO_CONFIG_GET,
	CP_GPIO_STAT_CONFIG_SET: CP_GPIO_STAT_CONFIG_SET,
	CP_GPIO_STAT_DECONFIG: CP_GPIO_STAT_DECONFIG,
	CP_GPIO_STAT_CONFIG_GET: CP_GPIO_STAT_CONFIG_GET,
	CP_GPIO_STAT_READ: CP_GPIO_STAT_READ,

	P0_00: P0_00,
	P0_01: P0_01,
	P0_02: P0_02,
	P0_03: P0_03,
	P0_04: P0_04,
	P0_09: P0_09,
	P0_10: P0_10,
	P0_15: P0_15,
	P0_16: P0_16,
	P0_17: P0_17,
	P0_18: P0_18,
	P0_19: P0_19,
	P0_20: P0_20,
	P0_22: P0_22,
	P0_23: P0_23,
	P0_24: P0_24,
	P0_25: P0_25,
	P0_26: P0_26,
	P0_27: P0_27,
	P0_28: P0_28,
	P0_29: P0_29,
	P0_30: P0_30,
	P0_31: P0_31,
	DIRECTION_IN: DIRECTION_IN,
	DIRECTION_OUT: DIRECTION_OUT,
	PULL_NONE: PULL_NONE,
	PULL_DOWN: PULL_DOWN,
	PULL_UP: PULL_UP,
	POLARITY_LOW: POLARITY_LOW,
	POLARITY_HIGH: POLARITY_HIGH,
	STATE_HIGH: STATE_HIGH,
	STATE_LOW: STATE_LOW,
	STATE_ACTIVE: STATE_ACTIVE,
	STATE_INACTIVE: STATE_INACTIVE,
}
