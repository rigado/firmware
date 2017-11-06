#!/usr/bin/env nodejs

var fs = require('fs')
var utils = require('./support/utils')
var async = require('async')
var report = require('./support/report')

var testList = JSON.parse(fs.readFileSync('test_list.json', 'utf8'))
var testConfig = JSON.parse(fs.readFileSync('test_config.json', 'utf8'))

report.testReportCreate(testConfig.report_file)

var count = Object.keys(testList).length
var index = 0

var keyList = Object.keys(testList)

async.eachSeries(keyList,
	function(key, callback) {
		tests = testList[key]
		async.eachSeries(tests, 
			function(testName, callback2) {
				var test = require('./' + testName)
				var testId = report.testRegister(test.getName())
				report.testStart(testId)
				test.testRunner(function(result, note) {
					report.testEnd(testId)
					report.testResult(testId, result, note)
					callback2()
				})
			}, function(err) {
				utils.checkError(err)
				callback()
			}
		)
	}, function(err) {
		report.testAllComplete()
		process.exit(0)
	}
)

