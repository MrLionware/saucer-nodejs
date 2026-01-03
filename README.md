<h1 align="center">saucer-nodejs</h1>

<p align="center">
  <strong>High-performance Node.js webview library with native system integration</strong>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#installation">Installation</a> •
  <a href="#quick-start">Quick Start</a> •
  <a href="#api-reference">API Reference</a> •
  <a href="#cli">CLI</a> •
  <a href="#examples">Examples</a>
</p>

<p align="center">
  <img src="https://img.shields.io/npm/v/saucer-nodejs?style=flat-square&color=blue" alt="npm version">
  <img src="https://img.shields.io/badge/node-%3E%3D20.0.0-brightgreen?style=flat-square" alt="node version">
  <img src="https://img.shields.io/badge/platforms-macOS%20|%20Windows%20|%20Linux-lightgrey?style=flat-square" alt="platforms">
  <img src="https://img.shields.io/badge/license-MIT-green?style=flat-square" alt="license">
</p>

---

**saucer-nodejs** is a modern, lightweight alternative to Electron for building cross-platform desktop applications with web technologies. Built on the [saucer](https://github.com/aspect-build/saucer) C++ webview library, it provides native webview integration with a **non-blocking event loop** that works seamlessly with Node.js async patterns.

## Why saucer-nodejs?

| Feature | saucer-nodejs | Electron | Tauri |
|---------|--------------|----------|-------|
| **Bundle Size** | ~5 MB | ~150 MB | ~3 MB |
| **Memory Usage** | Low | High | Low |
| **Startup Time** | Fast | Slow | Fast |
| **Node.js Integration** | Native | Native | Bridge |
| **Non-blocking Event Loop** | ✅ | ❌ | ❌ |
| **System Tray** | ✅ | ✅ | ✅ |
| **Notifications** | ✅ | ✅ | ✅ |
| **Clipboard** | ✅ | ✅ | ✅ |

---

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Project Structure](#project-structure)
- [API Reference](#api-reference)
  - [Application](#application)
  - [Webview](#webview)
  - [Clipboard](#clipboard)
  - [Notification](#notification)
  - [SystemTray](#systemtray)
- [CLI](#cli)
- [Debug Mode](#debug-mode)
- [Platform Support](#platform-support)
- [Examples](#examples)
- [Building from Source](#building-from-source)
- [Contributing](#contributing)
- [License](#license)

---

## Features

### Core Features
- 🚀 **Non-blocking Event Loop** - Native Node.js async/await integration
- 🪟 **Native Webview** - Uses system webview (WebKit on macOS, WebView2 on Windows, WebKitGTK on Linux)
- 📦 **Small Bundle Size** - ~5MB vs Electron's ~150MB
- ⚡ **Fast Startup** - Native performance without runtime overhead
- 🔄 **Bidirectional RPC** - Call Node.js functions from JavaScript and vice versa

### Window Management
- 📐 **Window Controls** - Size, position, title, resizable, decorations
- 🖥️ **Fullscreen Mode** - Toggle fullscreen with single property
- 🔍 **Zoom Control** - Adjustable webview zoom level
- 📍 **Window Position** - Get/set window position programmatically
- 🎨 **Customization** - Background color, transparency, always-on-top

### System Integration
- 📋 **Clipboard API** - Read/write text and images
- 🔔 **System Notifications** - Native OS notifications
- 🖱️ **System Tray** - Menu bar (macOS) / system tray (Windows/Linux)
- 📁 **File Dialogs** - Open/save file pickers

### Developer Experience
- 🛠️ **CLI Tool** - Build, diagnose, and scaffold projects
- 🐛 **Debug Logging** - Category-based debug output
- 📝 **TypeScript Support** - Full type definitions included
- 🧪 **Comprehensive Tests** - 200+ automated test assertions

---

## Requirements

- **Node.js** >= 20.0.0
- **Operating System**: macOS 11+, Windows 10+, or Linux (with GTK3 and WebKitGTK)

### Platform-Specific Requirements

| Platform | Webview Engine | Additional Requirements |
|----------|---------------|------------------------|
| macOS | WebKit (system) | None |
| Windows | WebView2 | WebView2 Runtime (included in Windows 11) |
| Linux | WebKitGTK | `webkit2gtk-4.0`, `gtk+-3.0` |

---

## Installation

```bash
npm install saucer-nodejs
```

The package automatically downloads prebuilt binaries for your platform. If no prebuilt binary is available, it will attempt to build from source (requires CMake).

### Verify Installation

```bash
npx saucer doctor
```

This runs platform diagnostics to ensure everything is set up correctly.

---

## Quick Start

### Basic Example

```javascript
import { Application, Webview } from 'saucer-nodejs';

// Create application
const app = new Application({ id: 'com.example.myapp' });

// Create webview window
const webview = new Webview(app);
webview.title = 'My App';
webview.size = { width: 1024, height: 768 };

// Load HTML directly
webview.loadHtml(`
  <!DOCTYPE html>
  <html>
    <head><title>Hello</title></head>
    <body>
      <h1>Hello from saucer-nodejs!</h1>
      <button onclick="greet()">Click me</button>
      <script>
        async function greet() {
          const result = await window.saucer.exposed.sayHello('World');
          alert(result);
        }
      </script>
    </body>
  </html>
`);

// Expose Node.js function to JavaScript
webview.expose('sayHello', (name) => {
  return `Hello, ${name}!`;
});

// Show window
webview.show();

// Handle close
webview.on('closed', () => app.quit());
```

### Using the CLI

```bash
# Create a new project
npx saucer init my-app
cd my-app
npm install
npm start

# Run diagnostics
npx saucer doctor

# Build standalone executable
npx saucer build index.js my-app
```

---

## Project Structure

```
saucer-nodejs/
├── index.js              # Main module entry point
├── index.d.ts            # TypeScript definitions
├── cli/
│   └── saucer.js         # CLI tool
├── lib/
│   ├── native-loader.js  # Native module loader
│   └── debug.js          # Debug logging utilities
├── examples/
│   ├── basic.js          # Simple demo
│   └── application-features.js  # Comprehensive test suite
├── src/                  # C++ source files
├── vendor/               # saucer C++ library
└── scripts/              # Build and install scripts
```

---

## API Reference

### Application

The main application class that manages the event loop and application lifecycle.

```javascript
import { Application } from 'saucer-nodejs';

const app = new Application({
  id: 'com.example.myapp',  // Bundle identifier
  threads: 4                 // Thread pool size (default: CPU cores)
});
```

#### Methods

| Method | Description |
|--------|-------------|
| `quit()` | Terminate the application |
| `isThreadSafe()` | Check if called from main thread |
| `nativeHandle()` | Get native application handle |
| `post(callback)` | Execute callback on main thread |
| `dispatch(callback)` | Execute and wait for result |

---

### Webview

The webview window class with full browser capabilities.

```javascript
import { Webview } from 'saucer-nodejs';

const webview = new Webview(app, {
  hardwareAcceleration: true,
  persistentCookies: false,
  userAgent: 'MyApp/1.0',
  preload: 'window.myAPI = { version: "1.0" };'
});
```

#### Properties

| Property | Type | Description |
|----------|------|-------------|
| `title` | `string` | Window title |
| `size` | `{ width, height }` | Window dimensions |
| `position` | `{ x, y }` | Window position |
| `visible` | `boolean` | Window visibility |
| `resizable` | `boolean` | Allow resizing |
| `decorations` | `boolean` | Show window decorations |
| `alwaysOnTop` | `boolean` | Keep window on top |
| `fullscreen` | `boolean` | Fullscreen mode |
| `zoom` | `number` | Zoom level (1.0 = 100%) |
| `url` | `string` | Current URL |
| `devTools` | `boolean` | Show developer tools |
| `contextMenu` | `boolean` | Enable context menu |

#### Methods

| Method | Description |
|--------|-------------|
| `show()` | Show the window |
| `hide()` | Hide the window |
| `close()` | Close the window |
| `focus()` | Focus the window |
| `navigate(url)` | Navigate to URL |
| `loadHtml(html)` | Load HTML directly |
| `reload()` | Reload current page |
| `back()` | Navigate back |
| `forward()` | Navigate forward |
| `expose(name, fn)` | Expose function to JavaScript |
| `evaluate(script)` | Execute JavaScript |
| `inject(options)` | Inject script |
| `embed(files)` | Embed files |
| `serve(filename)` | Serve embedded file |

#### Events

```javascript
webview.on('closed', () => console.log('Window closed'));
webview.on('resize', (width, height) => console.log(`Resized: ${width}x${height}`));
webview.on('focus', (focused) => console.log(`Focus: ${focused}`));
webview.on('navigated', (url) => console.log(`Navigated to: ${url}`));
webview.on('load', (state) => console.log(`Load state: ${state}`));
webview.on('dom-ready', () => console.log('DOM ready'));
webview.on('title', (title) => console.log(`Title: ${title}`));
```

---

### Clipboard

System clipboard access.

```javascript
import { clipboard } from 'saucer-nodejs';

// Write text
clipboard.writeText('Hello, World!');

// Read text
const text = clipboard.readText();

// Check availability
const hasText = clipboard.hasText();
const hasImage = clipboard.hasImage();

// Clear clipboard
clipboard.clear();
```

---

### Notification

System notifications.

```javascript
import { Notification } from 'saucer-nodejs';

// Check support
if (Notification.isSupported()) {
  // Request permission
  const permission = await Notification.requestPermission();
  
  if (permission === 'granted') {
    // Show notification
    const notification = new Notification({
      title: 'Hello!',
      body: 'This is a notification'
    });
    notification.show();
  }
}
```

---

### SystemTray

System tray / menu bar integration.

```javascript
import { SystemTray } from 'saucer-nodejs';

const tray = new SystemTray();

// Set tooltip
tray.setTooltip('My App');

// Set menu
tray.setMenu([
  { label: 'Show Window', click: () => webview.show() },
  { type: 'separator' },
  { label: 'Quit', click: () => app.quit() }
]);

// Show tray icon
tray.show();

// Cleanup
tray.destroy();
```

---

## CLI

The `saucer` CLI provides developer tools for building and debugging applications.

```bash
# Show help
npx saucer help

# Create new project
npx saucer init my-app

# Run in development mode with watch
npx saucer dev index.js

# Build standalone executable (SEA)
npx saucer build index.js my-app

# Run platform diagnostics
npx saucer doctor

# Show system information
npx saucer info
```

---

## Debug Mode

Enable debug logging with environment variables:

```bash
# Enable all debug output
SAUCER_DEBUG=1 node app.js

# Enable specific categories
SAUCER_DEBUG=rpc,window node app.js
```

### Debug Categories

| Category | Description |
|----------|-------------|
| `rpc` | RPC/expose function calls |
| `window` | Window operations |
| `webview` | Webview operations |
| `native` | Native module calls |
| `app` | Application lifecycle |
| `event` | Event handling |
| `perf` | Performance timing |

### Programmatic Debug

```javascript
import { createDebugger, measure } from 'saucer-nodejs/debug';

const debug = createDebugger('myapp');
debug('Something happened', { data: 123 });

// Measure async operations
const result = await measure('loadData', async () => {
  // ... operation
  return data;
});
```

---

## Platform Support

| Feature | macOS | Windows | Linux |
|---------|-------|---------|-------|
| Webview | ✅ WebKit | ✅ WebView2 | ✅ WebKitGTK |
| Clipboard | ✅ | ✅ | ✅ |
| Notifications | ✅ | ✅ | ✅ |
| System Tray | ✅ | ⚠️ Stub | ⚠️ Stub |
| Window Position | ✅ | ⚠️ Stub | ⚠️ Stub |
| Fullscreen | ✅ | ⚠️ Stub | ⚠️ Stub |
| Zoom | ✅ | ⚠️ Stub | ⚠️ Stub |

**Note**: Features marked as "Stub" have the API implemented but return placeholder values. Full implementations coming soon.

---

## Examples

### Load URL

```javascript
webview.navigate('https://example.com');
```

### Embed Files

```javascript
webview.embed({
  'index.html': { content: '<h1>Hello</h1>', mime: 'text/html' },
  'style.css': { content: 'body { color: blue; }', mime: 'text/css' }
});
webview.serve('index.html');
```

### Custom URL Scheme

```javascript
Webview.registerScheme('myapp');

webview.handleScheme('myapp', (request) => {
  if (request.url === 'myapp://api/data') {
    return {
      data: JSON.stringify({ hello: 'world' }),
      mime: 'application/json',
      status: 200
    };
  }
});
```

### Script Injection

```javascript
webview.inject({
  code: 'console.log("Injected!");',
  time: 'ready',      // 'creation' or 'ready'
  permanent: true     // Persist across navigations
});
```

---

## Building from Source

### Prerequisites

- Node.js >= 20
- CMake >= 3.15
- C++20 compatible compiler
  - macOS: Xcode Command Line Tools
  - Windows: Visual Studio 2019+
  - Linux: GCC 10+ or Clang 12+

### Build Steps

```bash
# Clone repository
git clone https://github.com/aspect-build/saucer-nodejs.git
cd saucer-nodejs

# Install dependencies
npm install

# Build native module
npm run rebuild

# Run tests
node examples/application-features.js
```

---

## Contributing

Contributions are welcome! Please read our [Contributing Guide](CONTRIBUTING.md) for details.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

---

## License

MIT License - see [LICENSE](LICENSE) for details.

---
