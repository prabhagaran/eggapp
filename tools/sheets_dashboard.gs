/**
 * Incubator Telemetry Dashboard builder
 * ------------------------------------------------------------------
 * Target spreadsheet:
 * https://docs.google.com/spreadsheets/d/1p3ud1bIm_ypz806wJOYs797Bpn0PZ_ON2fb6KQXr9OE/edit
 *
 * The Apps Script project attached to that sheet ALREADY RUNS the
 * telemetry receiver (doGet/doPost from egg_incubator_v2/
 * google_apps_script.txt). DO NOT replace that code. Instead, in the
 * same Apps Script project: click "+" next to Files > Script, name it
 * "dashboard", paste this file in, save, then reload the spreadsheet
 * and use the "Incubator" menu > "Build / refresh dashboard".
 * (The receiver defines no onOpen, so there is no menu conflict.)
 *
 * What it does:
 *  - Reads the raw telemetry tab ('Telemetry', written by the receiver;
 *    errors go to 'ErrorLogs' and are not charted here)
 *  - Aggregates per hour: avg/min/max temp, avg valid humidity
 *    (0% = invalid DHT read, excluded), heater & humidifier duty %
 *  - Writes the aggregate to a "Dash_Data" tab
 *  - Builds a "Dashboard" tab with temperature, humidity and duty
 *    charts plus a current-status summary block
 *
 * Re-running refreshes everything in place.
 */

var RAW_SHEET_NAME = 'Telemetry';   // tab written by the receiver script
var DATA_SHEET     = 'Dash_Data';
var DASH_SHEET     = 'Dashboard';

function onOpen() {
  SpreadsheetApp.getUi()
    .createMenu('Incubator')
    .addItem('Build / refresh dashboard', 'buildDashboard')
    .addToUi();
}

function buildDashboard() {
  var ss  = SpreadsheetApp.getActiveSpreadsheet();
  var raw = ss.getSheetByName(RAW_SHEET_NAME);
  if (!raw) throw new Error('Raw sheet "' + RAW_SHEET_NAME + '" not found — edit RAW_SHEET_NAME.');

  var values = raw.getDataRange().getValues();
  var head = values[0].map(String);
  var col = {};
  head.forEach(function (h, i) { col[h.trim()] = i; });
  ['Timestamp', 'Temp', 'Hum', 'SetTemp', 'SetHum', 'Heater', 'Humidifier', 'Day']
    .forEach(function (c) { if (!(c in col)) throw new Error('Column "' + c + '" missing in raw sheet.'); });

  // ── Aggregate per hour ────────────────────────────────────────────
  var buckets = {};  // key: 'yyyy-MM-dd HH:00'
  var keys = [];
  for (var r = 1; r < values.length; r++) {
    var ts = values[r][col.Timestamp];
    if (!(ts instanceof Date)) continue;
    var t = Number(values[r][col.Temp]);
    var h = Number(values[r][col.Hum]);
    if (isNaN(t)) continue;
    var key = Utilities.formatDate(ts, ss.getSpreadsheetTimeZone(), 'yyyy-MM-dd HH:00');
    var b = buckets[key];
    if (!b) {
      b = buckets[key] = { n: 0, tSum: 0, tMin: 99, tMax: -99,
                           hSum: 0, hN: 0, heat: 0, humid: 0,
                           setT: Number(values[r][col.SetTemp]),
                           setH: Number(values[r][col.SetHum]),
                           day: values[r][col.Day], ts: ts };
      keys.push(key);
    }
    b.n++;
    b.tSum += t; if (t < b.tMin) b.tMin = t; if (t > b.tMax) b.tMax = t;
    if (h > 0) { b.hSum += h; b.hN++; }            // 0% = invalid read, skip
    b.heat  += Number(values[r][col.Heater])     || 0;
    b.humid += Number(values[r][col.Humidifier]) || 0;
  }
  keys.sort();

  var out = [['Hour', 'Day', 'Temp avg', 'Temp min', 'Temp max', 'SetTemp',
              'Hum avg (valid)', 'SetHum', 'Heater duty %', 'Humidifier duty %', 'Samples']];
  keys.forEach(function (k) {
    var b = buckets[k];
    out.push([b.ts, b.day,
              round1(b.tSum / b.n), b.tMin, b.tMax, b.setT,
              b.hN ? round1(b.hSum / b.hN) : '', b.setH,
              Math.round(100 * b.heat / b.n), Math.round(100 * b.humid / b.n), b.n]);
  });

  var data = ss.getSheetByName(DATA_SHEET) || ss.insertSheet(DATA_SHEET);
  data.clearContents();
  data.getRange(1, 1, out.length, out[0].length).setValues(out);
  data.getRange(2, 1, out.length - 1, 1).setNumberFormat('MM-dd HH:mm');
  data.hideSheet();

  // ── Dashboard sheet ───────────────────────────────────────────────
  var dash = ss.getSheetByName(DASH_SHEET) || ss.insertSheet(DASH_SHEET);
  dash.getCharts().forEach(function (c) { dash.removeChart(c); });
  dash.clearContents();

  var last = out[out.length - 1];
  dash.getRange('A1').setValue('Incubator Dashboard — refreshed ' + new Date());
  dash.getRange('A3:B9').setValues([
    ['Last hour',            last[0]],
    ['Incubation day',       last[1]],
    ['Temp avg (last hour)', last[2] + ' °C  (set ' + last[5] + ')'],
    ['Hum avg (last hour)',  (last[6] === '' ? 'no valid reads' : last[6] + ' %') + '  (set ' + last[7] + ')'],
    ['Heater duty',          last[8] + ' %'],
    ['Humidifier duty',      last[9] + ' %'],
    ['Hours of data',        out.length - 1]
  ]);

  var n = out.length;
  var rng = function (c1, c2) {
    return [data.getRange(1, 1, n, 1), data.getRange(1, c1, n, 1)]
      .concat(c2 ? [data.getRange(1, c2, n, 1)] : []);
  };

  addLine(dash, 4,  1, 'Temperature (°C, hourly)', [
    data.getRange(1, 1, n, 1), data.getRange(1, 3, n, 1),
    data.getRange(1, 4, n, 1), data.getRange(1, 5, n, 1), data.getRange(1, 6, n, 1)]);
  addLine(dash, 4,  8, 'Humidity (%RH, hourly, invalid reads excluded)',
    rng(7, 8));
  addLine(dash, 26, 1, 'Heater duty (% of hour)',     rng(9));
  addLine(dash, 26, 8, 'Humidifier duty (% of hour)', rng(10));
}

function addLine(sheet, row, colPos, title, ranges) {
  var b = sheet.newChart().asLineChart()
    .setOption('title', title)
    .setOption('width', 640).setOption('height', 360)
    .setOption('useFirstColumnAsDomain', true)
    .setNumHeaders(1)
    .setPosition(row, colPos, 0, 0);
  ranges.forEach(function (r) { b.addRange(r); });
  sheet.insertChart(b.build());
}

function round1(x) { return Math.round(x * 10) / 10; }
