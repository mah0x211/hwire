#!/usr/bin/env node
/**
 * Convert Catch2 --reporter xml output (stdin) to a normalized JSON line (stdout).
 * Usage: ./bench_foo --reporter xml 2>/dev/null | node scripts/xml2json.js >> results/foo.jsonl
 *
 * Output format (one JSON line per run):
 * { "runName": "bench_hwire_sse42", "testCases": [
 *     { "name": "Header Count", "benchmarks": [
 *         { "name": "8 Headers, 433 B", "meanNs": 96.73,
 *           "lowMeanNs": 96.20, "highMeanNs": 97.63, "stdDevNs": 3.44 }
 *     ]}
 * ]}
 */
const { XMLParser } = require('fast-xml-parser');

const xmlParser = new XMLParser({
    ignoreAttributes: false,
    attributeNamePrefix: '@',
    isArray: (name) => name === 'TestCase' || name === 'BenchmarkResults'
});

let xml = '';
process.stdin.setEncoding('utf-8');
process.stdin.on('data', d => { xml += d; });
process.stdin.on('end', () => {
    try {
        const doc = xmlParser.parse(xml);
        const run = doc.Catch2TestRun;
        if (!run) throw new Error('No Catch2TestRun element found');

        const out = {
            runName: run['@name'] || '',
            testCases: (run.TestCase || []).map(tc => ({
                name: tc['@name'] || '',
                benchmarks: (tc.BenchmarkResults || []).map(bm => ({
                    name: bm['@name'] || '',
                    meanNs: parseFloat(bm.mean?.['@value'] || 0),
                    lowMeanNs: parseFloat(bm.mean?.['@lowerBound'] || 0),
                    highMeanNs: parseFloat(bm.mean?.['@upperBound'] || 0),
                    stdDevNs: parseFloat(bm.standardDeviation?.['@value'] || 0)
                }))
            }))
        };
        process.stdout.write(JSON.stringify(out) + '\n');
    } catch (e) {
        process.stderr.write(`xml2json error: ${e.message}\n`);
        process.exit(1);
    }
});
