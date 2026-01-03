#!/usr/bin/env node

/**
 * Install script for saucer-nodejs
 * 
 * This script runs during `npm install`. It checks if a prebuilt binary is available
 * for the current platform. If not, it builds from source using cmake-js.
 */

import { existsSync } from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';
import { execSync, spawn } from 'child_process';
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const __dirname = dirname(fileURLToPath(import.meta.url));
const rootDir = join(__dirname, '..');

// Platform package mapping
const platformPackages = {
    'darwin-arm64': '@aspect-build/saucer-nodejs-darwin-arm64',
    'darwin-x64': '@aspect-build/saucer-nodejs-darwin-x64',
    'win32-x64': '@aspect-build/saucer-nodejs-win32-x64',
    'win32-arm64': '@aspect-build/saucer-nodejs-win32-arm64',
    'linux-x64': '@aspect-build/saucer-nodejs-linux-x64-gnu',
    'linux-arm64': '@aspect-build/saucer-nodejs-linux-arm64-gnu',
};

function getPlatformKey() {
    return `${process.platform}-${process.arch}`;
}

function log(message) {
    console.log(`[saucer-nodejs] ${message}`);
}

function error(message) {
    console.error(`[saucer-nodejs] ERROR: ${message}`);
}

/**
 * Check if prebuilt binary is available via optionalDependency
 */
function hasPrebuiltBinary() {
    const platformKey = getPlatformKey();
    const packageName = platformPackages[platformKey];

    if (!packageName) {
        log(`No prebuilt package available for ${platformKey}`);
        return false;
    }

    try {
        // Try to resolve the package
        require.resolve(packageName);
        log(`Found prebuilt binary: ${packageName}`);
        return true;
    } catch (e) {
        log(`Prebuilt binary not installed: ${packageName}`);
        return false;
    }
}

/**
 * Check if local build already exists
 */
function hasLocalBuild() {
    const buildPath = join(rootDir, 'build', 'Release', 'saucer-nodejs.node');
    return existsSync(buildPath);
}

/**
 * Build from source using cmake-js
 */
async function buildFromSource() {
    log('Building from source...');

    // Check if cmake is available
    try {
        execSync('cmake --version', { stdio: 'pipe' });
    } catch (e) {
        error('CMake is not installed. Please install CMake to build from source.');
        error('Installation instructions: https://cmake.org/install/');
        process.exit(1);
    }

    // Run cmake-js rebuild
    return new Promise((resolve, reject) => {
        const args = ['rebuild'];
        const cmakeJs = join(rootDir, 'node_modules', '.bin', 'cmake-js');

        const child = spawn(cmakeJs, args, {
            cwd: rootDir,
            stdio: 'inherit',
            shell: process.platform === 'win32',
        });

        child.on('close', (code) => {
            if (code === 0) {
                log('Build completed successfully!');
                resolve();
            } else {
                error(`Build failed with exit code ${code}`);
                reject(new Error(`Build failed with exit code ${code}`));
            }
        });

        child.on('error', (err) => {
            error(`Build error: ${err.message}`);
            reject(err);
        });
    });
}

/**
 * Main install logic
 */
async function main() {
    const platformKey = getPlatformKey();
    log(`Platform: ${platformKey}`);

    // Skip if explicitly disabled
    if (process.env.SAUCER_SKIP_INSTALL === '1') {
        log('Skipping install (SAUCER_SKIP_INSTALL=1)');
        return;
    }

    // Check for prebuilt binary first
    if (hasPrebuiltBinary()) {
        log('Using prebuilt binary');
        return;
    }

    // Check if we should skip build
    if (process.env.SAUCER_SKIP_BUILD === '1') {
        log('Skipping build (SAUCER_SKIP_BUILD=1)');
        return;
    }

    // Check if local build exists
    if (hasLocalBuild()) {
        log('Using existing local build');
        return;
    }

    // Build from source
    log('No prebuilt binary available, building from source...');

    try {
        await buildFromSource();
    } catch (err) {
        error('Failed to build from source');
        error('');
        error('Prerequisites:');
        error('  - Node.js 20+');
        error('  - CMake 3.20+');
        error('  - C++23 compiler');
        error('');
        error('Platform-specific:');
        error('  macOS: Xcode Command Line Tools');
        error('  Linux: GCC 14+ or Clang 18+, WebKitGTK 4.0');
        error('  Windows: Visual Studio 2022, WebView2 SDK');
        error('');
        error('See https://github.com/aspect-build/saucer-nodejs#installation');
        process.exit(1);
    }
}

main().catch((err) => {
    error(err.message);
    process.exit(1);
});
