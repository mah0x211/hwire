#!/usr/bin/env node
/**
 * Generate Markdown report from Catch2 benchmark results
 * Usage: node generate_report.js <results_file>
 */

const fs = require('fs');
const os = require('os');
const { execSync } = require('child_process');

// Format large numbers (requests/sec)
function formatReqPerSec(reqPerSec) {
  if (reqPerSec >= 1000000) {
    return (reqPerSec / 1000000).toFixed(2) + 'M';
  } else if (reqPerSec >= 1000) {
    return (reqPerSec / 1000).toFixed(2) + 'K';
  }
  return reqPerSec.toFixed(0);
}

// Format data size
function formatDataSize(bytes) {
  if (bytes >= 1024) {
    return (bytes / 1024).toFixed(2) + ' KB';
  }
  return bytes + ' bytes';
}

// Get system information
function getSystemInfo() {
  const info = {
    platform: os.type(),
    arch: os.machine() || os.arch(),
    osVersion: '',
    cpu: '',
    cpuCores: os.cpus().length,
    memory: '',
    compiler: ''
  };

  // Format memory (GB)
  const memBytes = os.totalmem();
  const memGB = Math.round(memBytes / (1024 * 1024 * 1024));
  info.memory = `${memGB} GB`;

  // OS version and CPU info (platform specific)
  try {
    if (process.platform === 'darwin') {
      // macOS
      info.osVersion = execSync('sw_vers -productVersion', { encoding: 'utf-8' }).trim();
      const osBuild = execSync('sw_vers -buildVersion', { encoding: 'utf-8' }).trim();
      info.osVersion += ` (${osBuild})`;

      // CPU info
      info.cpu = execSync('sysctl -n machdep.cpu.brand_string', { encoding: 'utf-8' }).trim();

      // Compiler
      try {
        const clangVersion = execSync('clang --version 2>/dev/null | head -1', { encoding: 'utf-8', shell: '/bin/bash' }).trim();
        info.compiler = clangVersion;
      } catch (e) {
        info.compiler = 'Unknown';
      }
    } else if (process.platform === 'linux') {
      // Linux
      try {
        const osRelease = fs.readFileSync('/etc/os-release', 'utf-8');
        const nameMatch = osRelease.match(/PRETTY_NAME="([^"]+)"/);
        if (nameMatch) {
          info.osVersion = nameMatch[1];
        } else {
          const idMatch = osRelease.match(/ID=([^\n]+)/);
          const versionMatch = osRelease.match(/VERSION_ID=([^\n]+)/);
          info.osVersion = (idMatch ? idMatch[1] : 'Linux') + (versionMatch ? ' ' + versionMatch[1].replace(/"/g, '') : '');
        }
      } catch (e) {
        info.osVersion = 'Linux';
      }

      // CPU info
      try {
        const cpuInfo = fs.readFileSync('/proc/cpuinfo', 'utf-8');
        const modelMatch = cpuInfo.match(/model name\s*:\s*(.+)/);
        if (modelMatch) {
          info.cpu = modelMatch[1].trim();
        }
      } catch (e) {}

      // Compiler
      try {
        const gccVersion = execSync('gcc --version 2>/dev/null | head -1', { encoding: 'utf-8', shell: '/bin/bash' }).trim();
        info.compiler = gccVersion;
      } catch (e) {
        try {
          const clangVersion = execSync('clang --version 2>/dev/null | head -1', { encoding: 'utf-8', shell: '/bin/bash' }).trim();
          info.compiler = clangVersion;
        } catch (e2) {
          info.compiler = 'Unknown';
        }
      }
    } else {
      info.osVersion = os.release();
      info.cpu = os.cpus()[0]?.model || 'Unknown';
      info.compiler = 'Unknown';
    }
  } catch (e) {
    // Fallback
    info.osVersion = os.release();
    info.cpu = os.cpus()[0]?.model || 'Unknown';
    info.compiler = 'Unknown';
  }

  return info;
}

// Benchmark categories with metadata
const CATEGORIES = {
  'hdr': {
    name: 'Header Count (Modern Browser)',
    description: 'Measures scaling with realistic header configurations (A→A+B→A+B+C→A+B+C+D)',
    control: 'Progressive addition of header groups based on modern browser + CDN structure',
    tests: {
      'hdr_8': '8 headers (Browser basic)',
      'hdr_15': '15 headers (Browser + Security)',
      'hdr_20': '20 headers (Browser + Security + Auth)',
      'hdr_28': '28 headers (Full modern + CDN)'
    }
  },
  'val': {
    name: 'Header Value Length',
    description: 'Measures how parsing time scales with header value size',
    control: 'Same 28-header structure, only value lengths vary',
    tests: {
      'val_short': '~20 chars (short values)',
      'val_medium': '~100 chars (User-Agent)',
      'val_long': '~300 chars (Cookie)',
      'val_xlong': '~1000 chars (JWT + Cookie)'
    }
  },
  'case': {
    name: 'Case Sensitivity',
    description: 'Measures header name normalization cost',
    control: 'Same 28-header structure, different case styles',
    tests: {
      'case_lower': 'All lowercase (HTTP/2 style)',
      'case_mixed': 'Mixed case (HTTP/1.1 proxy)'
    }
  },
  'real': {
    name: 'Real-World Requests',
    description: 'Reference for actual usage patterns from modern browsers and APIs',
    control: 'Typical requests from browsers, APIs, and mobile apps',
    tests: {
      'real_browser': 'Modern browser + Cloudflare (28 headers)',
      'real_api': 'REST API request (15 headers)',
      'real_mobile': 'Mobile app request (20 headers)'
    }
  },
  'minimal': {
    name: 'Baseline',
    description: 'Minimum parsing cost for comparison',
    control: 'Minimal HTTP/1.1 requests',
    tests: {
      'minimal': 'No headers (parser overhead only)',
      'minimal_host': 'Host header only (HTTP/1.1 minimum)'
    }
  }
};

// Parse time value with unit to nanoseconds
function parseTimeToNs(value, unit) {
  const num = parseFloat(value);
  switch (unit) {
    case 'ns': return num;
    case 'us': return num * 1000;
    case 'ms': return num * 1000000;
    case 's': return num * 1000000000;
    default: return num;
  }
}

// Format time for table (shorter format)
function formatTimeShort(ns) {
  if (ns >= 1000000) {
    return (ns / 1000000).toFixed(2) + ' ms';
  } else if (ns >= 1000) {
    return (ns / 1000).toFixed(2) + ' μs';
  } else {
    return ns.toFixed(2) + ' ns';
  }
}

// Parse variant name to display name
function getVariantDisplayName(variant) {
  switch (variant) {
    case 'nosimd': return 'No-SIMD';
    case 'sse2': return 'SSE2';
    case 'sse42': return 'SSE4.2';
    case 'avx2': return 'AVX2';
    case 'neon': return 'NEON';
    default: return variant.toUpperCase();
  }
}

// Get category key from test name
function getCategoryKey(testName) {
  if (testName.startsWith('hdr_')) return 'hdr';
  if (testName.startsWith('val_')) return 'val';
  if (testName.startsWith('case_')) return 'case';
  if (testName.startsWith('real_')) return 'real';
  if (testName === 'minimal') return 'minimal';
  return 'other';
}

// Get display label for test name
function getTestLabel(testName) {
  const catKey = getCategoryKey(testName);
  const category = CATEGORIES[catKey];
  if (category && category.tests[testName]) {
    return category.tests[testName];
  }
  return testName;
}

// Parse Catch2 benchmark output
function parseResults(content) {
  const lines = content.split('\n');
  const results = [];
  const dataSizes = {};
  let currentParser = '';
  let currentVariant = '';

  const headerPattern = /(bench_[a-z]+_[a-z0-9]+) is a Catch2/;
  const benchmarkLinePattern = /^([a-z_0-9]+)\s+(\d+)\s+(\d+)\s+([\d.]+)\s*(ms|us|ns|s)?\s*$/;
  const valueLinePattern = /^\s+([\d.]+)\s*(ns|us|ms|s)\s+([\d.]+)\s*(ns|us|ms|s)\s+([\d.]+)\s*(ns|us|ms|s)\s*$/;
  const dataSizePattern = /^DATA_SIZE:\s*(\w+)\s*=\s*(\d+)/;

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];

    // Parse DATA_SIZE lines
    const sizeMatch = line.match(dataSizePattern);
    if (sizeMatch) {
      dataSizes[sizeMatch[1]] = parseInt(sizeMatch[2], 10);
      continue;
    }

    const headerMatch = line.match(headerPattern);
    if (headerMatch) {
      const exeName = headerMatch[1];
      const parts = exeName.split('_');
      if (parts.length >= 3) {
        currentParser = parts[1];
        currentVariant = parts[2];
      }
      continue;
    }

    if (line.length > 0 && !line.startsWith(' ') && !line.startsWith('-') && !line.startsWith('=') && !line.startsWith('~')) {
      const benchMatch = line.match(benchmarkLinePattern);
      if (benchMatch && currentParser) {
        const testName = benchMatch[1];
        const samples = parseInt(benchMatch[2], 10);
        const iterations = parseInt(benchMatch[3], 10);

        if (i + 1 < lines.length) {
          const meanLine = lines[i + 1];
          const meanMatch = meanLine.match(valueLinePattern);
          if (meanMatch) {
            const meanNs = parseTimeToNs(meanMatch[1], meanMatch[2]);
            const lowMeanNs = parseTimeToNs(meanMatch[3], meanMatch[4]);
            const highMeanNs = parseTimeToNs(meanMatch[5], meanMatch[6]);

            let stdDevNs = 0;
            if (i + 2 < lines.length) {
              const stdDevLine = lines[i + 2];
              const stdDevMatch = stdDevLine.match(valueLinePattern);
              if (stdDevMatch) {
                stdDevNs = parseTimeToNs(stdDevMatch[1], stdDevMatch[2]);
              }
            }

            results.push({
              test: testName,
              testLabel: getTestLabel(testName),
              category: getCategoryKey(testName),
              parser: currentParser,
              variant: currentVariant,
              variantDisplay: getVariantDisplayName(currentVariant),
              samples,
              iterations,
              meanNs,
              lowMeanNs,
              highMeanNs,
              stdDevNs,
              dataSize: dataSizes[testName] || 0
            });
          }
        }
      }
    }
  }

  return results;
}

// Group results by category
function groupByCategory(results) {
  const groups = {};
  for (const result of results) {
    const cat = result.category;
    if (!groups[cat]) {
      groups[cat] = {};
    }
    if (!groups[cat][result.test]) {
      groups[cat][result.test] = [];
    }
    groups[cat][result.test].push(result);
  }
  return groups;
}

// Generate Markdown table for a test
function generateTable(results, testLabel, testName) {
  results.sort((a, b) => a.meanNs - b.meanNs);

  // Get data size from first result (all results for same test have same size)
  const dataSize = results[0]?.dataSize || 0;

  const maxParserLen = Math.max(8, ...results.map(r => r.parser.length));
  const maxVariantLen = Math.max(7, ...results.map(r => r.variantDisplay.length));

  let table = `**${testLabel}**\n\n`;
  if (dataSize > 0) {
    table += `Data Size: ${formatDataSize(dataSize)}\n\n`;
  }

  // Table with Req/sec and MB/sec columns
  table += '| ' + 'Parser'.padEnd(maxParserLen) +
           ' | ' + 'Variant'.padEnd(maxVariantLen) +
           ' | ' + 'Mean'.padStart(10) +
           ' | ' + 'Std Dev'.padStart(9) +
           ' | ' + '95% CI'.padEnd(21) +
           ' | ' + 'Req/sec'.padStart(8) +
           ' | ' + 'MB/sec'.padStart(7) +
           ' |\n';

  table += '| ' + '-'.repeat(maxParserLen) +
           ' | ' + '-'.repeat(maxVariantLen) +
           ' |' + '-'.repeat(11) +
           '|' + '-'.repeat(10) +
           '|' + '-'.repeat(23) +
           '|' + '-'.repeat(9) +
           '|' + '-'.repeat(8) +
           '|\n';

  for (const r of results) {
    const meanStr = formatTimeShort(r.meanNs);
    const stdDevStr = formatTimeShort(r.stdDevNs);
    const ciStr = formatTimeShort(r.lowMeanNs) + ' - ' + formatTimeShort(r.highMeanNs);

    // Calculate throughput
    const reqPerSec = 1_000_000_000 / r.meanNs;
    const mbPerSec = (dataSize * reqPerSec) / 1_000_000;

    table += '| ' + r.parser.padEnd(maxParserLen) +
             ' | ' + r.variantDisplay.padEnd(maxVariantLen) +
             ' | ' + meanStr.padStart(10) +
             ' | ' + stdDevStr.padStart(9) +
             ' | ' + ciStr.padEnd(21) +
             ' | ' + formatReqPerSec(reqPerSec).padStart(8) +
             ' | ' + mbPerSec.toFixed(2).padStart(7) +
             ' |\n';
  }

  return table;
}

// Generate ASCII bar chart
function generateBarChart(results, maxBarLength = 30) {
  results.sort((a, b) => a.meanNs - b.meanNs);
  const maxNs = Math.max(...results.map(r => r.meanNs));
  const baselineNs = results[0].meanNs; // Fastest result
  const lines = [];

  for (let i = 0; i < results.length; i++) {
    const r = results[i];
    const label = (r.parser + ' ' + r.variantDisplay).padEnd(16);
    const barLength = Math.round((r.meanNs / maxNs) * maxBarLength);
    const bar = '█'.repeat(barLength);

    let timeStr;
    if (r.meanNs >= 1000) {
      timeStr = (r.meanNs / 1000).toFixed(1) + ' μs';
    } else {
      timeStr = r.meanNs.toFixed(1) + ' ns';
    }

    // Add baseline/slower annotation
    let annotation;
    if (i === 0) {
      annotation = '(baseline)';
    } else {
      const percentSlower = ((r.meanNs - baselineNs) / baselineNs * 100).toFixed(1);
      annotation = `(${percentSlower}% slower)`;
    }

    lines.push(label + ' ' + bar + ' ' + timeStr + ' ' + annotation);
  }

  return lines.join('\n');
}

// Generate category report
function generateCategoryReport(categoryKey, testGroups, allResults) {
  const category = CATEGORIES[categoryKey];
  if (!category) return '';

  let report = `## ${category.name}\n\n`;
  report += `> ${category.description}\n`;
  report += `> **Control:** ${category.control}\n\n`;

  // Get test order from category definition
  const testOrder = Object.keys(category.tests);
  const hasResults = testOrder.some(t => testGroups[t]);

  if (!hasResults) {
    return '';
  }

  // Generate tables for each test with bar chart
  for (const testName of testOrder) {
    if (testGroups[testName] && testGroups[testName].length > 0) {
      const testLabel = category.tests[testName];
      report += generateTable(testGroups[testName], testLabel, testName);
      report += '\n**Comparison** (lower is better):\n```\n';
      report += generateBarChart(testGroups[testName]);
      report += '\n```\n\n';
    }
  }

  return report;
}

// Generate full report
function generateReport(results) {
  const sysInfo = getSystemInfo();

  let report = '# HTTP Parser Benchmark Results\n\n';
  report += '## System Information\n\n';
  report += '| Item | Value |\n';
  report += '|------|-------|\n';
  report += `| OS | ${sysInfo.platform} ${sysInfo.osVersion} |\n`;
  report += `| Architecture | ${sysInfo.arch} |\n`;
  report += `| CPU | ${sysInfo.cpu} (${sysInfo.cpuCores} cores) |\n`;
  report += `| Memory | ${sysInfo.memory} |\n`;
  report += `| Compiler | ${sysInfo.compiler} |\n`;
  report += `| Build Flags | -O2 |\n`;
  report += '\n---\n\n';

  // Group by category and test
  const categoryGroups = groupByCategory(results);

  // Category order for report
  const categoryOrder = ['hdr', 'val', 'case', 'real', 'minimal'];

  for (const catKey of categoryOrder) {
    if (categoryGroups[catKey]) {
      report += generateCategoryReport(catKey, categoryGroups[catKey], results);
      report += '---\n\n';
    }
  }

  report += '_Generated by generate_report.js_\n';

  return report;
}

// Main
function main() {
  const args = process.argv.slice(2);
  if (args.length < 1) {
    console.error('Usage: node generate_report.js <results_file>');
    process.exit(1);
  }

  const inputFile = args[0];

  if (!fs.existsSync(inputFile)) {
    console.error(`Error: File not found: ${inputFile}`);
    process.exit(1);
  }

  const content = fs.readFileSync(inputFile, 'utf-8');
  const results = parseResults(content);

  if (results.length === 0) {
    console.log('# HTTP Parser Benchmark Results\n\n_No benchmark results found_\n');
    process.exit(0);
  }

  console.log(generateReport(results));
}

main();
