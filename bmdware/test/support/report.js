#!/usr/bin/env nodejs

var fs = require('fs')
var moment = require('moment')
var utils = require('./utils')

var testIdCount = 0

var testList = new Array()
var testReportFile
var testStartTime = 0
var testStartTimeHuman = ''
var testEndTime = 0

var testSuiteStartTimeHuman = ''
var testSuiteTotalRunTime = 0
var testOverallResult = 'PASS'

function testReportCreate(fileName) {
	var timeString = moment().format('HHmmss')
	testSuiteStartTimeHuman = timeString
	var fileNameString = timeString + '_' + fileName
	testReportFile = fs.openSync(fileNameString, 'a')
	if(testReportFile) {
		fs.writeSync(testReportFile, 'TestName,StartTime,RunTime,Result,Note\n')
	}
}

function testRegister(testName) {
	var testId = testIdCount
	utils.log(5, 'Starting test ' + testName + '...')
	testList.push(testName)
	testIdCount++
	return testId
}

function testStart(testId) {
	testStartTime = Date.now()
	testStartTimeHuman = moment().format('HHmmss')
}

function testEnd(testId) {
	testEndTime = Date.now()
}

function testResult(testId, result, note) {
	if(result == 'FAIL') {
		testOverallResult = 'FAIL'
	}

	var testRunTime = testEndTime - testStartTime
	testSuiteTotalRunTime += testRunTime
	var reportString = testList[testId] + ',' + testStartTimeHuman + ',' + testRunTime + ',' +
		result + ',' + note + '\n'
	fs.writeSync(testReportFile, reportString)
}

function testAllComplete() {
	var finalReport = 'Test Suite,' + testSuiteStartTimeHuman + ',' + testSuiteTotalRunTime + ',' + testOverallResult + ','
	fs.writeSync(testReportFile, finalReport)
	fs.closeSync(testReportFile)
}

module.exports = {
	testReportCreate: testReportCreate,
	testRegister: testRegister,
	testStart: testStart,
	testEnd: testEnd,
	testResult: testResult,
	testAllComplete: testAllComplete
}