#!/usr/bin/env node
/**
 * Generate Markdown report from benchmark JSONL result files.
 * Each .jsonl file contains one JSON line per binary run (Catch2 XML → xml-js compact JSON).
 * Usage: node report.js <file.jsonl> [...]
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

    const memBytes = os.totalmem();
    info.memory = `${Math.round(memBytes / (1024 * 1024 * 1024))} GB`;

    try {
        if (process.platform === 'darwin') {
            info.osVersion = execSync('sw_vers -productVersion', { encoding: 'utf-8' }).trim();
            const osBuild = execSync('sw_vers -buildVersion', { encoding: 'utf-8' }).trim();
            info.osVersion += ` (${osBuild})`;
            info.cpu = execSync('sysctl -n machdep.cpu.brand_string', { encoding: 'utf-8' }).trim();
            try {
                info.compiler = execSync('clang --version 2>/dev/null | head -1', { encoding: 'utf-8', shell: '/bin/bash' }).trim();
            } catch (e) { info.compiler = 'Unknown'; }
        } else if (process.platform === 'linux') {
            try {
                const osRelease = fs.readFileSync('/etc/os-release', 'utf-8');
                const nameMatch = osRelease.match(/PRETTY_NAME="([^"]+)"/);
                info.osVersion = nameMatch ? nameMatch[1] : 'Linux';
            } catch (e) { info.osVersion = 'Linux'; }
            try {
                const cpuInfo = fs.readFileSync('/proc/cpuinfo', 'utf-8');
                const m = cpuInfo.match(/model name\s*:\s*(.+)/);
                if (m) info.cpu = m[1].trim();
            } catch (e) { }
            try {
                info.compiler = execSync('gcc --version 2>/dev/null | head -1', { encoding: 'utf-8', shell: '/bin/bash' }).trim();
            } catch (e) {
                try {
                    info.compiler = execSync('clang --version 2>/dev/null | head -1', { encoding: 'utf-8', shell: '/bin/bash' }).trim();
                } catch (e2) { info.compiler = 'Unknown'; }
            }
        } else {
            info.osVersion = os.release();
            info.cpu = os.cpus()[0]?.model || 'Unknown';
            info.compiler = 'Unknown';
        }
    } catch (e) {
        info.osVersion = os.release();
        info.cpu = os.cpus()[0]?.model || 'Unknown';
        info.compiler = 'Unknown';
    }

    return info;
}

// Category metadata (keyed by TestCase name)
const CATEGORY_META = {
    'Header Count': {
        description: 'Measures parsing time scaling with increasing header counts. hwire `(LC)` variants include lowercase key conversion.'
    },
    'Header Value Length': {
        description: 'Measures how parsing time scales with header value size. hwire `(LC)` variants include lowercase key conversion.'
    },
    'Case Sensitivity': {
        description: 'Measures header name normalization cost (lowercase vs mixed case). hwire `(LC)` variants include lowercase key conversion.'
    },
    'Real-World Requests': {
        description: 'Typical requests from browsers, REST APIs, and mobile apps. hwire `(LC)` variants include lowercase key conversion.'
    },
    'Baseline': {
        description: 'Minimum parsing cost for comparison. hwire `(LC)` variants include lowercase key conversion.'
    },
    'Real-World Responses': {
        description: 'Typical responses from web servers and CDNs. hwire `(LC)` variants include lowercase key conversion.'
    }
};

// Display order of categories in report
const CATEGORY_ORDER = [
    'Header Count',
    'Header Value Length',
    'Case Sensitivity',
    'Real-World Requests',
    'Baseline',
    'Real-World Responses'
];

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
        case 'simd': return 'SIMD';
        default: return variant.toUpperCase();
    }
}

// Extract parser and variant from binary name
// bench_hwire_sse42 -> { parser: 'hwire', variant: 'sse42' }
// bench_hwire_resp_sse42 -> { parser: 'hwire', variant: 'sse42' }
// bench_httparse_simd -> { parser: 'httparse', variant: 'simd' }
function extractParserVariant(exeName) {
    const parts = exeName.split('_');
    // parts[0] = 'bench', parts[1] = parser
    if (parts.length < 3) return { parser: parts[1] || exeName, variant: 'default' };
    // bench_X_resp_Y -> parser=X, variant=Y
    if (parts[2] === 'resp') {
        return { parser: parts[1], variant: parts[3] || 'default' };
    }
    // bench_X_Y -> parser=X, variant=Y
    // bench_X_Y_Z -> parser=X, variant=Y_Z (e.g. future multi-part variant)
    return { parser: parts[1], variant: parts.slice(2).join('_') };
}

// Parse all .jsonl files and return flat results array
function parseJsonlFiles(files) {
    const results = [];

    for (const filepath of files) {
        const lines = fs.readFileSync(filepath, 'utf-8').split('\n').filter(l => l.trim());
        for (const line of lines) {
            let run;
            try {
                run = JSON.parse(line);
            } catch (e) {
                console.warn(`Warning: failed to parse JSON line in ${filepath}: ${e.message}`);
                continue;
            }

            if (!run.runName || !run.testCases) continue;

            const { parser, variant } = extractParserVariant(run.runName);
            const variantDisplay = getVariantDisplayName(variant);

            for (const tc of run.testCases) {
                // tc.name = "Category, Data Label" (e.g. "Header Count, 8 Headers")
                const tcParts = tc.name.split(', ');
                const category = tcParts[0] || 'Unknown';
                const dataLabel = tcParts[1] || '';

                for (const bm of tc.benchmarks || []) {
                    // bm.name = "433 B" or "433 B, LC"
                    const bmParts = bm.name.split(', ');
                    const sizeStr = bmParts[0];
                    const opts = bmParts.slice(1);
                    const effectiveVariantDisplay = opts.length > 0
                        ? `${variantDisplay} (${opts.join(', ')})`
                        : variantDisplay;

                    // benchName = "Data Label, Size" keeps the same table display
                    const benchName = dataLabel ? `${dataLabel}, ${sizeStr}` : sizeStr;

                    const sizeMatch = sizeStr.match(/^(\d+)\s*B$/);
                    const dataSize = sizeMatch ? parseInt(sizeMatch[1], 10) : 0;

                    results.push({
                        category,
                        benchName,
                        parser,
                        variant,
                        variantDisplay: effectiveVariantDisplay,
                        meanNs: bm.meanNs,
                        lowMeanNs: bm.lowMeanNs,
                        highMeanNs: bm.highMeanNs,
                        stdDevNs: bm.stdDevNs,
                        dataSize
                    });
                }
            }
        }
    }

    return results;
}

// Group results by category -> benchName -> [entries]
function groupResults(results) {
    const groups = {};
    for (const r of results) {
        if (!groups[r.category]) groups[r.category] = {};
        if (!groups[r.category][r.benchName]) groups[r.category][r.benchName] = [];
        groups[r.category][r.benchName].push(r);
    }
    return groups;
}

// Generate Markdown table for one benchmark
function generateTable(entries, benchName) {
    entries.sort((a, b) => a.meanNs - b.meanNs);
    const dataSize = entries[0]?.dataSize || 0;

    const maxParserLen = Math.max(8, ...entries.map(r => r.parser.length));
    const maxVariantLen = Math.max(7, ...entries.map(r => r.variantDisplay.length));

    let table = `### ${benchName}\n\n`;
    if (dataSize > 0) {
        table += `Data Size: ${formatDataSize(dataSize)}\n\n`;
    }

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
        ' | ' + '-'.repeat(10) +
        ' | ' + '-'.repeat(9) +
        ' | ' + '-'.repeat(21) +
        ' | ' + '-'.repeat(8) +
        ' | ' + '-'.repeat(7) +
        ' |\n';

    for (const r of entries) {
        const meanStr = formatTimeShort(r.meanNs);
        const stdDevStr = formatTimeShort(r.stdDevNs);
        const ciStr = formatTimeShort(r.lowMeanNs) + ' - ' + formatTimeShort(r.highMeanNs);
        const reqPerSec = 1_000_000_000 / r.meanNs;
        const mbPerSec = dataSize > 0 ? (dataSize * reqPerSec) / 1_000_000 : 0;

        table += '| ' + r.parser.padEnd(maxParserLen) +
            ' | ' + r.variantDisplay.padEnd(maxVariantLen) +
            ' | ' + meanStr.padStart(10) +
            ' | ' + stdDevStr.padStart(9) +
            ' | ' + ciStr.padEnd(21) +
            ' | ' + formatReqPerSec(reqPerSec).padStart(8) +
            ' | ' + (dataSize > 0 ? mbPerSec.toFixed(2) : '-').padStart(7) +
            ' |\n';
    }

    return table;
}

// Generate ASCII bar chart
function generateBarChart(entries, maxBarLength = 30) {
    entries.sort((a, b) => a.meanNs - b.meanNs);
    const maxNs = Math.max(...entries.map(r => r.meanNs));
    const baselineNs = entries[0].meanNs;
    const maxLabelLen = Math.max(...entries.map(r => (r.parser + ' ' + r.variantDisplay).length));
    const lines = [];

    for (let i = 0; i < entries.length; i++) {
        const r = entries[i];
        const label = (r.parser + ' ' + r.variantDisplay).padEnd(maxLabelLen);
        const barLength = Math.round((r.meanNs / maxNs) * maxBarLength);
        const bar = '█'.repeat(barLength);
        const timeStr = r.meanNs >= 1000
            ? (r.meanNs / 1000).toFixed(1) + ' μs'
            : r.meanNs.toFixed(1) + ' ns';
        const annotation = i === 0
            ? '(baseline)'
            : `(${((r.meanNs - baselineNs) / baselineNs * 100).toFixed(1)}% slower)`;
        lines.push(label + ' ' + bar + ' ' + timeStr + ' ' + annotation);
    }

    return lines.join('\n');
}

// Generate section for one category
function generateCategorySection(category, benchGroups) {
    const meta = CATEGORY_META[category] || {};
    let section = `## ${category}\n\n`;
    if (meta.description) section += `> ${meta.description}\n\n`;

    for (const [benchName, entries] of Object.entries(benchGroups)) {
        section += generateTable(entries, benchName);
        section += '\n**Comparison** (lower is better):\n```\n';
        section += generateBarChart(entries);
        section += '\n```\n\n';
    }

    return section;
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

    const groups = groupResults(results);

    // Emit categories in defined order, then any remaining
    const emitted = new Set();
    for (const cat of CATEGORY_ORDER) {
        if (groups[cat]) {
            report += generateCategorySection(cat, groups[cat]);
            report += '---\n\n';
            emitted.add(cat);
        }
    }
    for (const cat of Object.keys(groups)) {
        if (!emitted.has(cat)) {
            report += generateCategorySection(cat, groups[cat]);
            report += '---\n\n';
        }
    }

    report += '_Generated by report.js_\n';
    return report;
}

// Main
function main() {
    const args = process.argv.slice(2);
    if (args.length < 1) {
        console.error('Usage: node report.js <file.jsonl> [...]');
        console.error('  node report.js results/req_*.jsonl   # all request results (shell glob)');
        console.error('  node report.js results/rsp_*.jsonl   # all response results');
        console.error('  node report.js results/req_hwire.jsonl  # hwire only');
        process.exit(1);
    }

    const files = args.filter(f => {
        if (!fs.existsSync(f)) {
            console.warn(`Warning: file not found: ${f}`);
            return false;
        }
        return true;
    });

    const results = parseJsonlFiles(files);
    if (results.length === 0) {
        console.log('# HTTP Parser Benchmark Results\n\n_No benchmark results found_\n');
        process.exit(0);
    }

    console.log(generateReport(results));
}

main();
