#!/usr/bin/env node

/**
 * Prepublish script for saucer-nodejs
 * 
 * Verifies that everything is ready for publishing to npm.
 */

import { existsSync, readFileSync } from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const rootDir = join(__dirname, '..');

const requiredFiles = [
    'index.js',
    'index.d.ts',
    'lib/native-loader.js',
    'README.md',
    'package.json',
];

const platformPackages = [
    'darwin-arm64',
    'darwin-x64',
    'win32-x64',
    'win32-arm64',
    'linux-x64-gnu',
    'linux-arm64-gnu',
];

function log(message) {
    console.log(`[prepublish] ${message}`);
}

function error(message) {
    console.error(`[prepublish] ERROR: ${message}`);
}

function check(condition, message) {
    if (!condition) {
        error(message);
        process.exit(1);
    }
    log(`✓ ${message}`);
}

// Check required files
log('Checking required files...');
for (const file of requiredFiles) {
    check(existsSync(join(rootDir, file)), `${file} exists`);
}

// Check package.json
log('\nChecking package.json...');
const pkg = JSON.parse(readFileSync(join(rootDir, 'package.json'), 'utf-8'));
check(pkg.name, 'package name is set');
check(pkg.version, 'package version is set');
check(pkg.main, 'main entry point is set');
check(pkg.types, 'TypeScript definitions are set');
check(pkg.optionalDependencies, 'optionalDependencies are configured');

// Check platform packages exist
log('\nChecking platform packages...');
for (const platform of platformPackages) {
    const pkgPath = join(rootDir, 'npm', platform, 'package.json');
    const exists = existsSync(pkgPath);
    if (exists) {
        const platformPkg = JSON.parse(readFileSync(pkgPath, 'utf-8'));
        log(`✓ ${platform} package.json exists (${platformPkg.name})`);
    } else {
        log(`⚠ ${platform} package.json missing (will fail on that platform)`);
    }
}

// Summary
log('\n========================================');
log('Pre-publish checks passed!');
log('========================================');
log('');
log('To publish:');
log('1. Create a git tag: git tag v' + pkg.version);
log('2. Push the tag: git push origin v' + pkg.version);
log('3. CI will build and publish automatically');
log('');
log('Or manually:');
log('1. npm run prebuild (on each platform)');
log('2. npm publish (for each platform package)');
log('3. npm publish (for main package)');
