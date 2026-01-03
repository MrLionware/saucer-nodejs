#!/usr/bin/env node
/**
 * Saucer Build Script
 *
 * Creates distributable executables from saucer-nodejs applications using Node.js SEA.
 *
 * Usage:
 *   npm run build:app
 *
 * Configuration:
 *   Create a saucer.config.js in your project root (see documentation)
 */

import { build as esbuildBundle } from "esbuild";
import { execSync } from "child_process";
import {
  copyFileSync,
  mkdirSync,
  writeFileSync,
  readFileSync,
  existsSync,
  rmSync,
  readdirSync,
  chmodSync,
} from "fs";
import { join, dirname, resolve, basename } from "path";
import { fileURLToPath } from "url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

// Colors for console output
const colors = {
  reset: "\x1b[0m",
  bright: "\x1b[1m",
  green: "\x1b[32m",
  yellow: "\x1b[33m",
  blue: "\x1b[34m",
  red: "\x1b[31m",
  cyan: "\x1b[36m",
};

function log(msg, color = colors.reset) {
  console.log(`${color}${msg}${colors.reset}`);
}

function logStep(step, msg) {
  console.log(
    `${colors.blue}[${step}]${colors.reset} ${colors.bright}${msg}${colors.reset}`,
  );
}

function logSuccess(msg) {
  console.log(`${colors.green}✅ ${msg}${colors.reset}`);
}

function logError(msg) {
  console.error(`${colors.red}❌ ${msg}${colors.reset}`);
}

function logWarn(msg) {
  console.log(`${colors.yellow}⚠️  ${msg}${colors.reset}`);
}

/**
 * Default configuration
 */
const defaultConfig = {
  entry: "./src/main.js",
  name: "SaucerApp",
  output: "./dist",
  mode: "bundle", // 'bundle' | 'single-file'
  bundleId: null, // Auto-generated if not provided
  icon: null,
  iconWin: null,
  version: "1.0.0",
  nodeOptions: [],
  esbuildOptions: {},
};

/**
 * Load user configuration
 */
async function loadConfig() {
  const cwd = process.cwd();
  const configPaths = [
    join(cwd, "saucer.config.js"),
    join(cwd, "saucer.config.mjs"),
    join(cwd, "saucer.config.cjs"),
  ];

  for (const configPath of configPaths) {
    if (existsSync(configPath)) {
      try {
        const userConfig = await import(configPath);
        const config = { ...defaultConfig, ...userConfig.default };

        // Auto-generate bundleId if not provided
        if (!config.bundleId) {
          config.bundleId = `com.saucer.${config.name.toLowerCase().replace(/[^a-z0-9]/g, "")}`;
        }

        return config;
      } catch (err) {
        logError(`Failed to load config from ${configPath}: ${err.message}`);
        process.exit(1);
      }
    }
  }

  logWarn("No saucer.config.js found, using default configuration");
  logWarn("Create a saucer.config.js file to customize your build");
  return defaultConfig;
}

/**
 * Find the native addon (.node file)
 */
function findNativeAddon() {
  const saucerPkgDir = join(__dirname, "..");

  // Check common locations
  const locations = [
    join(saucerPkgDir, "build", "Release", "saucer-nodejs.node"),
    join(saucerPkgDir, "prebuilds", `${process.platform}-${process.arch}`, "saucer-nodejs.node"),
    join(saucerPkgDir, "build", "Debug", "saucer-nodejs.node"),
  ];

  // Also check prebuilds with different naming
  const prebuildsDir = join(saucerPkgDir, "prebuilds");
  if (existsSync(prebuildsDir)) {
    const platforms = readdirSync(prebuildsDir);
    for (const platform of platforms) {
      if (platform.startsWith(process.platform)) {
        const dir = join(prebuildsDir, platform);
        const files = readdirSync(dir);
        for (const file of files) {
          if (file.endsWith(".node")) {
            locations.unshift(join(dir, file));
          }
        }
      }
    }
  }

  for (const loc of locations) {
    if (existsSync(loc)) {
      return loc;
    }
  }

  logError("Could not find saucer-nodejs.node native addon");
  logError("Make sure the package is properly built (npm run rebuild)");
  process.exit(1);
}

/**
 * Create the native addon loader code
 */
function createNativeLoader(config) {
  const version = config.version || "1.0.0";

  return `
// ============================================================================
// Saucer Native Addon Loader (auto-generated)
// ============================================================================
var __saucerAddonPath = null;
var __saucerNativeAddon = null;

(function() {
  'use strict';

  const isSea = (() => {
    try {
      return require('node:sea').isSea();
    } catch {
      return false;
    }
  })();

  if (!isSea) return; // Not running as SEA, use normal require

  const fs = require('fs');
  const path = require('path');
  const os = require('os');

  // Determine addon location based on platform and mode
  function findAddonPath() {
    const execDir = path.dirname(process.execPath);

    if (process.platform === 'darwin') {
      // macOS: Check .app bundle structure first
      const resourcesPath = path.join(execDir, '..', 'Resources', 'saucer-nodejs.node');
      if (fs.existsSync(resourcesPath)) {
        return resourcesPath;
      }
      // Fallback: same directory as executable
      const sameDirPath = path.join(execDir, 'saucer-nodejs.node');
      if (fs.existsSync(sameDirPath)) {
        return sameDirPath;
      }
    } else {
      // Windows/Linux: same directory as executable
      const sameDirPath = path.join(execDir, 'saucer-nodejs.node');
      if (fs.existsSync(sameDirPath)) {
        return sameDirPath;
      }
    }

    // Single-file mode: extract from SEA asset
    return extractFromAsset();
  }

  function extractFromAsset() {
    try {
      const sea = require('node:sea');
      const cacheDir = path.join(os.homedir(), '.cache', 'saucer-nodejs', '${version}');
      const addonPath = path.join(cacheDir, 'saucer-nodejs.node');

      // Check if already extracted
      if (fs.existsSync(addonPath)) {
        return addonPath;
      }

      // Extract from SEA asset
      const addonData = sea.getRawAsset('saucer-nodejs.node');
      if (!addonData) {
        throw new Error('Native addon not found in SEA assets');
      }

      fs.mkdirSync(cacheDir, { recursive: true });
      fs.writeFileSync(addonPath, Buffer.from(addonData));

      // Make executable on Unix
      if (process.platform !== 'win32') {
        fs.chmodSync(addonPath, 0o755);
      }

      return addonPath;
    } catch (err) {
      console.error('[Saucer] Failed to extract native addon:', err.message);
      process.exit(1);
    }
  }

  // Find the addon path and pre-load the native addon
  __saucerAddonPath = findAddonPath();

  // Load the native addon using dlopen
  const addonModule = { exports: {} };
  process.dlopen(addonModule, __saucerAddonPath);
  __saucerNativeAddon = addonModule.exports;
})();

`;
}

/**
 * Create a bindings shim that uses the pre-loaded addon
 */
function createBindingsShim() {
  return `
// Bindings shim for SEA - returns the pre-loaded native addon
export default function bindings(name) {
  if (typeof __saucerNativeAddon !== 'undefined' && __saucerNativeAddon) {
    return __saucerNativeAddon;
  }
  // Fallback for non-SEA mode (shouldn't happen in bundled app)
  throw new Error('Native addon not loaded. Are you running as a SEA?');
}
`;
}

/**
 * Bundle JavaScript with esbuild
 */
async function bundleJS(config, buildDir) {
  logStep("1/5", "Bundling JavaScript with esbuild...");

  const entryPath = resolve(process.cwd(), config.entry);

  if (!existsSync(entryPath)) {
    logError(`Entry file not found: ${entryPath}`);
    process.exit(1);
  }

  // Create a bindings shim file (use .mjs for ESM)
  const bindingsShimPath = join(buildDir, "bindings-shim.mjs");
  writeFileSync(bindingsShimPath, createBindingsShim());

  // Bundle with esbuild, replacing 'bindings' with our shim
  await esbuildBundle({
    entryPoints: [entryPath],
    bundle: true,
    platform: "node",
    target: "node20",
    format: "cjs", // SEA requires CommonJS
    outfile: join(buildDir, "bundle.js"),
    minify: false, // Keep readable for debugging
    sourcemap: false,
    alias: {
      bindings: bindingsShimPath,
    },
    ...config.esbuildOptions,
  });

  // Prepend the native loader
  const bundlePath = join(buildDir, "bundle.js");
  const bundle = readFileSync(bundlePath, "utf-8");
  const loader = createNativeLoader(config);
  writeFileSync(bundlePath, loader + bundle);

  logSuccess("JavaScript bundled");
}

/**
 * Generate SEA configuration and create blob
 */
function createSEABlob(config, buildDir) {
  logStep("2/5", "Creating SEA blob...");

  const seaConfig = {
    main: join(buildDir, "bundle.js"),
    output: join(buildDir, "sea-prep.blob"),
    disableExperimentalSEAWarning: true,
    useCodeCache: true,
    useSnapshot: false,
    assets: {},
  };

  // For single-file mode, embed the native addon
  if (config.mode === "single-file") {
    const addonPath = findNativeAddon();
    seaConfig.assets["saucer-nodejs.node"] = addonPath;
    logStep("", `  Embedding native addon (${Math.round(readFileSync(addonPath).length / 1024)}KB)`);
  }

  // Write SEA config
  const seaConfigPath = join(buildDir, "sea-config.json");
  writeFileSync(seaConfigPath, JSON.stringify(seaConfig, null, 2));

  // Generate blob
  try {
    execSync(`node --experimental-sea-config "${seaConfigPath}"`, {
      stdio: "inherit",
      cwd: process.cwd(),
    });
  } catch (err) {
    logError("Failed to create SEA blob");
    logError("Make sure you're using Node.js 20.1.0 or later");
    process.exit(1);
  }

  logSuccess("SEA blob created");
}

/**
 * Create the executable by injecting SEA blob into Node.js binary
 */
function createExecutable(config, buildDir) {
  logStep("3/5", "Creating executable...");

  const nodePath = process.execPath;
  const exeName = config.name + (process.platform === "win32" ? ".exe" : "");
  const exePath = join(buildDir, exeName);
  const blobPath = join(buildDir, "sea-prep.blob");

  // Copy Node.js binary
  copyFileSync(nodePath, exePath);

  // Ensure the copied binary is writable
  chmodSync(exePath, 0o755);

  // Remove existing code signature (macOS)
  if (process.platform === "darwin") {
    try {
      execSync(`codesign --remove-signature "${exePath}"`, { stdio: "pipe" });
    } catch {
      // Ignore if not signed
    }
  }

  // Inject SEA blob using postject
  const sentinel = "NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2";

  // Determine postject flags based on platform
  let postjectFlags = `--sentinel-fuse ${sentinel}`;
  if (process.platform === "darwin") {
    postjectFlags += " --macho-segment-name NODE_SEA";
  }

  try {
    execSync(
      `npx postject "${exePath}" NODE_SEA_BLOB "${blobPath}" ${postjectFlags}`,
      { stdio: "inherit" },
    );
  } catch (err) {
    logError("Failed to inject SEA blob");
    process.exit(1);
  }

  // Re-sign (macOS)
  if (process.platform === "darwin") {
    try {
      execSync(`codesign --sign - "${exePath}"`, { stdio: "pipe" });
    } catch (err) {
      logWarn("Ad-hoc code signing failed (may require manual signing)");
    }
  }

  logSuccess(`Executable created: ${exeName}`);
  return exePath;
}

/**
 * Create macOS .app bundle
 */
function createMacOSBundle(config, buildDir, exePath) {
  logStep("4/5", "Creating macOS .app bundle...");

  const outputDir = resolve(process.cwd(), config.output);
  const appName = `${config.name}.app`;
  const appPath = join(outputDir, appName);

  // Clean existing bundle
  if (existsSync(appPath)) {
    rmSync(appPath, { recursive: true });
  }

  // Create bundle structure
  const contentsDir = join(appPath, "Contents");
  const macOSDir = join(contentsDir, "MacOS");
  const resourcesDir = join(contentsDir, "Resources");

  mkdirSync(macOSDir, { recursive: true });
  mkdirSync(resourcesDir, { recursive: true });

  // Copy executable
  const destExePath = join(macOSDir, config.name);
  copyFileSync(exePath, destExePath);
  chmodSync(destExePath, 0o755);

  // Copy native addon
  const addonPath = findNativeAddon();
  copyFileSync(addonPath, join(resourcesDir, "saucer-nodejs.node"));

  // Create Info.plist
  const plist = `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key>
  <string>${config.name}</string>
  <key>CFBundleDisplayName</key>
  <string>${config.name}</string>
  <key>CFBundleIdentifier</key>
  <string>${config.bundleId}</string>
  <key>CFBundleVersion</key>
  <string>${config.version}</string>
  <key>CFBundleShortVersionString</key>
  <string>${config.version}</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleExecutable</key>
  <string>${config.name}</string>
  <key>CFBundleIconFile</key>
  <string>icon</string>
  <key>NSHighResolutionCapable</key>
  <true/>
  <key>LSMinimumSystemVersion</key>
  <string>10.15</string>
  <key>NSSupportsAutomaticGraphicsSwitching</key>
  <true/>
</dict>
</plist>`;

  writeFileSync(join(contentsDir, "Info.plist"), plist);

  // Copy icon if provided
  if (config.icon && existsSync(config.icon)) {
    const iconDest = join(resourcesDir, "icon.icns");
    copyFileSync(config.icon, iconDest);
  }

  // Code sign the entire bundle
  try {
    execSync(`codesign --force --deep --sign - "${appPath}"`, { stdio: "pipe" });
  } catch (err) {
    logWarn("Bundle code signing failed (may require manual signing for distribution)");
  }

  logSuccess(`macOS bundle created: ${appPath}`);
  return appPath;
}

/**
 * Create Windows/Linux folder bundle
 */
function createFolderBundle(config, buildDir, exePath) {
  logStep("4/5", `Creating ${process.platform === "win32" ? "Windows" : "Linux"} bundle...`);

  const outputDir = resolve(process.cwd(), config.output);
  const bundlePath = join(outputDir, config.name);

  // Clean existing bundle
  if (existsSync(bundlePath)) {
    rmSync(bundlePath, { recursive: true });
  }

  mkdirSync(bundlePath, { recursive: true });

  // Copy executable
  const exeName = config.name + (process.platform === "win32" ? ".exe" : "");
  const destExePath = join(bundlePath, exeName);
  copyFileSync(exePath, destExePath);

  if (process.platform !== "win32") {
    chmodSync(destExePath, 0o755);
  }

  // Copy native addon
  const addonPath = findNativeAddon();
  copyFileSync(addonPath, join(bundlePath, "saucer-nodejs.node"));

  logSuccess(`Bundle created: ${bundlePath}`);
  return bundlePath;
}

/**
 * Create single-file executable (addon extracted at runtime)
 */
function createSingleFile(config, buildDir, exePath) {
  logStep("4/5", "Creating single-file executable...");

  const outputDir = resolve(process.cwd(), config.output);
  mkdirSync(outputDir, { recursive: true });

  const exeName = config.name + (process.platform === "win32" ? ".exe" : "");
  const destPath = join(outputDir, exeName);

  copyFileSync(exePath, destPath);

  if (process.platform !== "win32") {
    chmodSync(destPath, 0o755);
  }

  logSuccess(`Single-file executable created: ${destPath}`);
  return destPath;
}

/**
 * Main build function
 */
async function build() {
  console.log("");
  log("╔══════════════════════════════════════════╗", colors.cyan);
  log("║     Saucer Build System                  ║", colors.cyan);
  log("║     Node.js SEA + Native Addon           ║", colors.cyan);
  log("╚══════════════════════════════════════════╝", colors.cyan);
  console.log("");

  // Load configuration
  const config = await loadConfig();

  log(`Building: ${colors.bright}${config.name}${colors.reset}`);
  log(`Mode: ${colors.bright}${config.mode}${colors.reset}`);
  log(`Entry: ${colors.bright}${config.entry}${colors.reset}`);
  log(`Output: ${colors.bright}${config.output}${colors.reset}`);
  console.log("");

  // Create build directory
  const buildDir = join(process.cwd(), ".saucer-build");
  if (existsSync(buildDir)) {
    rmSync(buildDir, { recursive: true });
  }
  mkdirSync(buildDir, { recursive: true });

  try {
    // Step 1: Bundle JavaScript
    await bundleJS(config, buildDir);

    // Step 2: Create SEA blob
    createSEABlob(config, buildDir);

    // Step 3: Create executable
    const exePath = createExecutable(config, buildDir);

    // Step 4: Create platform bundle or single-file
    let outputPath;
    if (config.mode === "single-file") {
      outputPath = createSingleFile(config, buildDir, exePath);
    } else if (process.platform === "darwin") {
      outputPath = createMacOSBundle(config, buildDir, exePath);
    } else {
      outputPath = createFolderBundle(config, buildDir, exePath);
    }

    // Step 5: Cleanup
    logStep("5/5", "Cleaning up...");
    rmSync(buildDir, { recursive: true });
    logSuccess("Build complete!");

    // Summary
    console.log("");
    log("╔══════════════════════════════════════════╗", colors.green);
    log("║     Build Summary                        ║", colors.green);
    log("╚══════════════════════════════════════════╝", colors.green);
    console.log("");
    log(`Output: ${colors.bright}${outputPath}${colors.reset}`);

    if (process.platform === "darwin" && config.mode === "bundle") {
      log("");
      log("To run your app:", colors.yellow);
      log(`  open "${outputPath}"`, colors.bright);
    } else if (config.mode === "single-file") {
      log("");
      log("To run your app:", colors.yellow);
      log(`  "${outputPath}"`, colors.bright);
      log("");
      log("Note: First run extracts native addon to ~/.cache/saucer-nodejs/", colors.yellow);
    }

    console.log("");
  } catch (err) {
    logError(`Build failed: ${err.message}`);
    console.error(err.stack);
    process.exit(1);
  }
}

// Run build
build();
