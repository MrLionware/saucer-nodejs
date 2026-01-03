#!/usr/bin/env node
/**
 * saucer-nodejs CLI
 * 
 * Commands:
 *   saucer build    - Package your app as a standalone executable
 *   saucer dev      - Run your app in development mode with hot reload
 *   saucer doctor   - Run platform diagnostics
 *   saucer info     - Show system information
 *   saucer init     - Initialize a new saucer-nodejs project
 */

import { fileURLToPath } from 'url';
import { dirname, join, resolve, basename } from 'path';
import { existsSync, readFileSync, writeFileSync, mkdirSync, copyFileSync, rmSync } from 'fs';
import { execSync, spawn } from 'child_process';
import { createRequire } from 'module';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

// ANSI colors
const colors = {
    reset: '\x1b[0m',
    bold: '\x1b[1m',
    dim: '\x1b[2m',
    red: '\x1b[31m',
    green: '\x1b[32m',
    yellow: '\x1b[33m',
    blue: '\x1b[34m',
    magenta: '\x1b[35m',
    cyan: '\x1b[36m',
};

function log(message, color = '') {
    console.log(`${color}${message}${colors.reset}`);
}

function success(message) {
    log(`✓ ${message}`, colors.green);
}

function error(message) {
    log(`✗ ${message}`, colors.red);
}

function warn(message) {
    log(`⚠ ${message}`, colors.yellow);
}

function info(message) {
    log(`ℹ ${message}`, colors.blue);
}

function banner() {
    console.log(`
${colors.cyan}${colors.bold}
   _____ ____  __  __________________
  / ___//   | / / / / ____/ ____/ __ \\
  \\__ \\/ /| |/ / / / /   / __/ / /_/ /
 ___/ / ___ / /_/ / /___/ /___/ _, _/
/____/_/  |_\\____/\\____/_____/_/ |_|
${colors.reset}
${colors.dim}saucer-nodejs CLI v1.0.0${colors.reset}
`);
}

// ============================================================================
// Command: doctor - Platform diagnostics
// ============================================================================

async function doctorCommand() {
    banner();
    log('Running platform diagnostics...', colors.bold);
    console.log();

    let allPassed = true;

    // Check Node.js version
    const nodeVersion = process.version;
    const majorVersion = parseInt(nodeVersion.slice(1).split('.')[0]);
    if (majorVersion >= 18) {
        success(`Node.js ${nodeVersion} (minimum: v18.0.0)`);
    } else {
        error(`Node.js ${nodeVersion} - requires v18.0.0 or higher`);
        allPassed = false;
    }

    // Check platform
    const platform = process.platform;
    const arch = process.arch;
    const supportedPlatforms = ['darwin', 'win32', 'linux'];
    if (supportedPlatforms.includes(platform)) {
        success(`Platform: ${platform}-${arch}`);
    } else {
        error(`Unsupported platform: ${platform}`);
        allPassed = false;
    }

    // Check if native module exists
    const nativeModulePath = join(__dirname, '..', 'build', 'Release', 'saucer-nodejs.node');
    if (existsSync(nativeModulePath)) {
        success('Native module: found');
    } else {
        error('Native module: not found (run npm run rebuild)');
        allPassed = false;
    }

    // Platform-specific checks
    if (platform === 'darwin') {
        // Check macOS version
        try {
            const swVers = execSync('sw_vers -productVersion', { encoding: 'utf8' }).trim();
            const majorMacOS = parseInt(swVers.split('.')[0]);
            if (majorMacOS >= 11) {
                success(`macOS ${swVers} (minimum: 11.0)`);
            } else {
                warn(`macOS ${swVers} - recommend 11.0 or higher`);
            }
        } catch {
            warn('Could not detect macOS version');
        }

        // Check for WebKit
        success('WebKit: available (system framework)');
    } else if (platform === 'linux') {
        // Check for WebKitGTK
        try {
            execSync('pkg-config --exists webkit2gtk-4.0', { encoding: 'utf8' });
            success('WebKitGTK: installed');
        } catch {
            error('WebKitGTK: not found (install webkit2gtk package)');
            allPassed = false;
        }

        // Check for GTK
        try {
            execSync('pkg-config --exists gtk+-3.0', { encoding: 'utf8' });
            success('GTK 3: installed');
        } catch {
            error('GTK 3: not found (install gtk3 package)');
            allPassed = false;
        }
    } else if (platform === 'win32') {
        // Windows uses WebView2
        success('WebView2: assumed available (Windows 10/11)');
        info('Note: WebView2 runtime is included in Windows 11 and recent Windows 10 updates');
    }

    // Check for build tools
    try {
        execSync('cmake --version', { encoding: 'utf8', stdio: 'pipe' });
        success('CMake: installed');
    } catch {
        if (existsSync(nativeModulePath)) {
            info('CMake: not found (not required - prebuilt binary available)');
        } else {
            error('CMake: not found (required for building from source)');
            allPassed = false;
        }
    }

    // Check npm packages
    const packageJson = JSON.parse(readFileSync(join(__dirname, '..', 'package.json'), 'utf8'));
    info(`Package version: ${packageJson.version}`);

    console.log();
    if (allPassed) {
        success('All checks passed! saucer-nodejs is ready to use.');
    } else {
        error('Some checks failed. Please fix the issues above.');
    }

    return allPassed;
}

// ============================================================================
// Command: info - System information
// ============================================================================

async function infoCommand() {
    banner();

    const require = createRequire(import.meta.url);
    const packageJson = require('../package.json');

    console.log(`${colors.bold}System Information${colors.reset}`);
    console.log('-'.repeat(40));
    console.log(`  Node.js:     ${process.version}`);
    console.log(`  Platform:    ${process.platform}`);
    console.log(`  Arch:        ${process.arch}`);
    console.log(`  Package:     saucer-nodejs@${packageJson.version}`);
    console.log();

    console.log(`${colors.bold}Paths${colors.reset}`);
    console.log('-'.repeat(40));
    console.log(`  Package:     ${join(__dirname, '..')}`);
    console.log(`  Native:      ${join(__dirname, '..', 'build', 'Release', 'saucer-nodejs.node')}`);
    console.log();

    console.log(`${colors.bold}Features${colors.reset}`);
    console.log('-'.repeat(40));

    const features = [
        ['Webview', '✓'],
        ['Clipboard', '✓'],
        ['Notifications', '✓'],
        ['System Tray', process.platform === 'darwin' ? '✓' : '⚠ (stub)'],
        ['Window Position', process.platform === 'darwin' ? '✓' : '⚠ (stub)'],
        ['Fullscreen', process.platform === 'darwin' ? '✓' : '⚠ (stub)'],
        ['Zoom', process.platform === 'darwin' ? '✓' : '⚠ (stub)'],
        ['SEA Build', '✓'],
    ];

    for (const [feature, status] of features) {
        const statusColor = status === '✓' ? colors.green : colors.yellow;
        console.log(`  ${feature.padEnd(18)} ${statusColor}${status}${colors.reset}`);
    }
}

// ============================================================================
// Command: build - Package as standalone executable (SEA)
// ============================================================================

async function buildCommand(args) {
    banner();

    const entryFile = args[0] || 'index.js';
    const outputName = args[1] || basename(entryFile, '.js');

    if (!existsSync(entryFile)) {
        error(`Entry file not found: ${entryFile}`);
        console.log();
        info('Usage: saucer build <entry.js> [output-name]');
        process.exit(1);
    }

    log(`Building standalone executable from ${entryFile}...`, colors.bold);
    console.log();

    const buildDir = '.saucer-build';
    const seaConfig = join(buildDir, 'sea-config.json');
    const seaBlob = join(buildDir, 'sea-prep.blob');
    const platform = process.platform;
    const outputFile = platform === 'win32' ? `${outputName}.exe` : outputName;

    // Step 1: Create build directory
    info('Creating build directory...');
    if (existsSync(buildDir)) {
        rmSync(buildDir, { recursive: true });
    }
    mkdirSync(buildDir, { recursive: true });

    // Step 2: Create SEA config
    info('Generating SEA configuration...');
    const config = {
        main: resolve(entryFile),
        output: seaBlob,
        disableExperimentalSEAWarning: true,
        useSnapshot: false,
        useCodeCache: true,
    };
    writeFileSync(seaConfig, JSON.stringify(config, null, 2));

    // Step 3: Generate blob
    info('Generating SEA blob...');
    try {
        execSync(`node --experimental-sea-config ${seaConfig}`, {
            stdio: 'inherit',
            cwd: process.cwd(),
        });
        success('SEA blob generated');
    } catch (err) {
        error('Failed to generate SEA blob');
        console.log(err.message);
        process.exit(1);
    }

    // Step 4: Copy Node.js binary
    info('Copying Node.js binary...');
    const nodeBin = process.execPath;
    const outputBin = join(buildDir, outputFile);
    copyFileSync(nodeBin, outputBin);
    success('Node.js binary copied');

    // Step 5: Inject blob into binary
    info('Injecting SEA blob into binary...');
    try {
        if (platform === 'darwin') {
            // macOS uses codesign
            execSync(`codesign --remove-signature "${outputBin}"`, { stdio: 'pipe' });
            execSync(`npx postject "${outputBin}" NODE_SEA_BLOB "${seaBlob}" --sentinel-fuse NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2 --macho-segment-name NODE_SEA`, { stdio: 'inherit' });
            execSync(`codesign --sign - "${outputBin}"`, { stdio: 'pipe' });
        } else if (platform === 'win32') {
            // Windows
            execSync(`npx postject "${outputBin}" NODE_SEA_BLOB "${seaBlob}" --sentinel-fuse NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2`, { stdio: 'inherit' });
        } else {
            // Linux
            execSync(`npx postject "${outputBin}" NODE_SEA_BLOB "${seaBlob}" --sentinel-fuse NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2`, { stdio: 'inherit' });
        }
        success('SEA blob injected');
    } catch (err) {
        error('Failed to inject SEA blob');
        console.log('Make sure postject is installed: npm install -g postject');
        process.exit(1);
    }

    // Step 6: Move to output
    const finalOutput = join(process.cwd(), outputFile);
    copyFileSync(outputBin, finalOutput);
    execSync(`chmod +x "${finalOutput}"`, { stdio: 'pipe' });

    console.log();
    success(`Build complete: ${outputFile}`);
    console.log();
    info('Note: The executable includes your JavaScript code but NOT native modules.');
    info('You must distribute the native addon (.node file) alongside the executable.');
    info('');
    info(`To run: ./${outputFile}`);
}

// ============================================================================
// Command: init - Initialize new project
// ============================================================================

async function initCommand(args) {
    banner();

    const projectName = args[0] || 'my-saucer-app';
    const projectDir = join(process.cwd(), projectName);

    if (existsSync(projectDir)) {
        error(`Directory already exists: ${projectName}`);
        process.exit(1);
    }

    log(`Creating new saucer-nodejs project: ${projectName}`, colors.bold);
    console.log();

    // Create directory
    info('Creating project directory...');
    mkdirSync(projectDir, { recursive: true });

    // Create package.json
    info('Generating package.json...');
    const packageJson = {
        name: projectName,
        version: '1.0.0',
        type: 'module',
        main: 'index.js',
        scripts: {
            start: 'node index.js',
            dev: 'node --watch index.js',
        },
        dependencies: {
            'saucer-nodejs': '^1.0.0',
        },
    };
    writeFileSync(join(projectDir, 'package.json'), JSON.stringify(packageJson, null, 2));

    // Create index.js
    info('Creating index.js...');
    const indexJs = `/**
 * ${projectName}
 * Built with saucer-nodejs
 */

import { Application, Webview } from 'saucer-nodejs';

// Create application
const app = new Application({ id: 'com.example.${projectName.replace(/-/g, '')}' });

// Create webview window
const webview = new Webview(app);
webview.title = '${projectName}';
webview.size = { width: 1024, height: 768 };

// Load your app content
const html = \`
<!DOCTYPE html>
<html>
<head>
  <title>${projectName}</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      color: white;
    }
    .container {
      text-align: center;
      padding: 2rem;
    }
    h1 { font-size: 3rem; margin-bottom: 1rem; }
    p { font-size: 1.2rem; opacity: 0.8; }
    .badge {
      display: inline-block;
      background: rgba(255,255,255,0.2);
      padding: 0.5rem 1rem;
      border-radius: 999px;
      margin-top: 2rem;
      font-size: 0.9rem;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🚀 ${projectName}</h1>
    <p>Your saucer-nodejs app is ready!</p>
    <div class="badge">Powered by saucer-nodejs</div>
  </div>
</body>
</html>
\`;

webview.loadHtml(html);
webview.show();

// Clean up on window close
webview.on('closed', () => {
  app.quit();
});
`;
    writeFileSync(join(projectDir, 'index.js'), indexJs);

    // Create .gitignore
    info('Creating .gitignore...');
    const gitignore = `node_modules/
.saucer-build/
*.node
`;
    writeFileSync(join(projectDir, '.gitignore'), gitignore);

    console.log();
    success(`Project created: ${projectName}`);
    console.log();
    info('Next steps:');
    console.log(`  cd ${projectName}`);
    console.log('  npm install');
    console.log('  npm start');
}

// ============================================================================
// Command: dev - Development mode with watch
// ============================================================================

async function devCommand(args) {
    const entryFile = args[0] || 'index.js';

    if (!existsSync(entryFile)) {
        error(`Entry file not found: ${entryFile}`);
        process.exit(1);
    }

    log(`Starting development server with ${entryFile}...`, colors.bold);
    info('Watching for changes. Press Ctrl+C to stop.');
    console.log();

    // Use Node.js --watch mode
    const child = spawn('node', ['--watch', entryFile], {
        stdio: 'inherit',
        cwd: process.cwd(),
    });

    child.on('exit', (code) => {
        process.exit(code || 0);
    });
}

// ============================================================================
// Command: help
// ============================================================================

function helpCommand() {
    banner();
    console.log(`${colors.bold}Commands:${colors.reset}

  ${colors.cyan}saucer build${colors.reset} <entry.js> [output]
      Package your app as a standalone executable using Node.js SEA

  ${colors.cyan}saucer dev${colors.reset} [entry.js]
      Run your app in development mode with file watching

  ${colors.cyan}saucer init${colors.reset} [project-name]
      Initialize a new saucer-nodejs project

  ${colors.cyan}saucer doctor${colors.reset}
      Run platform diagnostics and check requirements

  ${colors.cyan}saucer info${colors.reset}
      Show system and package information

  ${colors.cyan}saucer help${colors.reset}
      Show this help message

${colors.bold}Examples:${colors.reset}

  ${colors.dim}# Create a new project${colors.reset}
  saucer init my-app

  ${colors.dim}# Run in development mode${colors.reset}
  saucer dev index.js

  ${colors.dim}# Build standalone executable${colors.reset}
  saucer build index.js my-app

  ${colors.dim}# Check system compatibility${colors.reset}
  saucer doctor
`);
}

// ============================================================================
// Main entry point
// ============================================================================

const command = process.argv[2];
const args = process.argv.slice(3);

switch (command) {
    case 'build':
        await buildCommand(args);
        break;
    case 'dev':
        await devCommand(args);
        break;
    case 'doctor':
        await doctorCommand();
        break;
    case 'info':
        await infoCommand();
        break;
    case 'init':
        await initCommand(args);
        break;
    case 'help':
    case '--help':
    case '-h':
        helpCommand();
        break;
    default:
        if (command) {
            error(`Unknown command: ${command}`);
            console.log();
        }
        helpCommand();
        break;
}
