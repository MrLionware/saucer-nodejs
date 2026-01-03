/**
 * Native binding loader for saucer-nodejs
 * 
 * This module automatically loads the correct prebuilt native binary for the current platform.
 * If no prebuilt binary is available, it falls back to the locally built binary.
 */

import { createRequire } from 'module';
import { existsSync } from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';

const require = createRequire(import.meta.url);
const __dirname = dirname(fileURLToPath(import.meta.url));

/**
 * Platform configuration for prebuilt packages
 */
const platformPackages = {
    'darwin-arm64': '@aspect-build/saucer-nodejs-darwin-arm64',
    'darwin-x64': '@aspect-build/saucer-nodejs-darwin-x64',
    'win32-x64': '@aspect-build/saucer-nodejs-win32-x64',
    'win32-arm64': '@aspect-build/saucer-nodejs-win32-arm64',
    'linux-x64': '@aspect-build/saucer-nodejs-linux-x64-gnu',
    'linux-arm64': '@aspect-build/saucer-nodejs-linux-arm64-gnu',
};

/**
 * Gets the platform key for the current system
 */
function getPlatformKey() {
    const platform = process.platform;
    const arch = process.arch;
    return `${platform}-${arch}`;
}

/**
 * Attempts to load the native binding
 */
function loadNativeBinding() {
    const platformKey = getPlatformKey();
    const packageName = platformPackages[platformKey];
    const rootDir = join(__dirname, '..');

    // Strategy 1: Try bundled prebuild (included in npm package)
    const bundledPath = join(rootDir, 'prebuilds', platformKey, 'saucer-nodejs.node');
    if (existsSync(bundledPath)) {
        try {
            return require(bundledPath);
        } catch (e) {
            console.error('Failed to load bundled prebuild:', e.message);
        }
    }

    // Strategy 2: Try prebuilt platform package (npm optionalDependency)
    if (packageName) {
        try {
            const binding = require(packageName);
            if (binding) {
                return binding;
            }
        } catch (e) {
            // Prebuilt package not installed, continue to fallback
        }
    }

    // Strategy 3: Try local build directory
    const localBuildPath = join(rootDir, 'build', 'Release', 'saucer-nodejs.node');
    if (existsSync(localBuildPath)) {
        try {
            return require(localBuildPath);
        } catch (e) {
            // Local build exists but failed to load
            console.error('Failed to load local build:', e.message);
        }
    }

    // Strategy 4: Try bindings package (development fallback)
    try {
        const bindings = require('bindings');
        return bindings('saucer-nodejs');
    } catch (e) {
        // Final fallback failed
    }

    // No native binding found
    throw new Error(
        `Failed to load native binding for saucer-nodejs.\n` +
        `Platform: ${platformKey}\n` +
        `\n` +
        `Possible solutions:\n` +
        `1. Install with build tools: npm install saucer-nodejs --build-from-source\n` +
        `2. Check if prebuilt binaries are available for your platform\n` +
        `3. Ensure you have CMake, C++23 compiler, and platform dependencies installed\n` +
        `\n` +
        `For more info: https://github.com/aspect-build/saucer-nodejs#installation`
    );
}

// Export the loaded binding
export const native = loadNativeBinding();
export default native;
