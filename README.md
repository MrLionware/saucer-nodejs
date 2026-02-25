<h1 align="center">saucer-nodejs</h1>

<p align="center">
  <strong>High-performance Node.js bindings for the saucer v8 webview runtime</strong>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#installation">Installation</a> •
  <a href="#quick-start">Quick Start</a> •
  <a href="#api-reference">API Reference</a> •
  <a href="#cli">CLI</a> •
  <a href="#testing-and-api-parity">Testing</a>
</p>

<p align="center">
  <img src="https://img.shields.io/npm/v/saucer-nodejs?style=flat-square&color=blue" alt="npm version">
  <img src="https://img.shields.io/badge/node-%3E%3D20.0.0-brightgreen?style=flat-square" alt="node version">
  <img src="https://img.shields.io/badge/platforms-macOS%20|%20Windows%20|%20Linux-lightgrey?style=flat-square" alt="platforms">
  <img src="https://img.shields.io/badge/license-MIT-green?style=flat-square" alt="license">
</p>

---

`saucer-nodejs` is an ESM-first desktop app runtime for Node.js that uses the native system webview (WebKit/WebView2/WebKitGTK) with non-blocking integration into Node's event loop.

## Features

- Native webview windows with window controls, navigation, script injection, and custom URL schemes
- Node <-> webview bridge via `expose`, `onMessage`, and typed `SmartviewRPC`
- Thread-safe scheduling APIs: `post`, `dispatch`, `make`, `poolSubmit`, `poolEmplace`
- Desktop integrations: clipboard, notifications, system tray, file pickers, PDF export
- CLI for scaffolding, diagnostics, development mode, and SEA builds
- Comprehensive parity test suite in `examples/application-features.js`

## Requirements

- Node.js `>= 20.0.0`
- macOS 11+, Windows 10+, or Linux with GTK3/WebKitGTK
- CMake only needed when building native bindings from source

## Installation

```bash
npm install saucer-nodejs
```

Verify local environment:

```bash
npx saucer doctor
```

## Quick Start

```js
import { Application, Webview } from "saucer-nodejs";

const app = Application.init({ id: "com.example.myapp", threads: 2 });

const webview = new Webview(app, {
  hardwareAcceleration: true,
  preload: "window.APP_VERSION = '1.0.0';",
});

webview.title = "My App";
webview.size = { width: 1000, height: 700 };
webview.loadHtml(`
  <!doctype html>
  <html>
    <body>
      <h1>Hello from saucer-nodejs</h1>
      <button onclick="window.saucer.internal.send_message('ping')">Send Message</button>
    </body>
  </html>
`);

webview.onMessage((msg) => {
  console.log("message from webview:", msg);
  return true;
});

webview.on("closed", () => app.quit());
webview.show();
```

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
  - [Exports](#exports)
  - [Application](#application)
  - [Webview](#webview)
  - [SmartviewRPC and Types](#smartviewrpc-and-types)
  - [Desktop and PDF](#desktop-and-pdf)
  - [Stash and Icon](#stash-and-icon)
  - [Clipboard](#clipboard)
  - [Notification](#notification)
  - [SystemTray](#systemtray)
- [CLI](#cli)
- [Debug Mode](#debug-mode)
- [Platform Notes](#platform-notes)
- [Testing and API Parity](#testing-and-api-parity)
- [Examples](#examples)
- [Building from Source](#building-from-source)
- [Contributing](#contributing)
- [License](#license)

## API Reference

### Exports

Top-level exports from `saucer-nodejs`:

- Classes: `Application`, `Webview`, `Stash`, `Icon`, `Desktop`, `PDF`, `SmartviewRPC`, `Notification`, `SystemTray`
- Functions/objects: `createRPC`, `Types`, `clipboard`

### Application

Create and manage the application instance.

```js
import { Application } from "saucer-nodejs";

const app = Application.init({ id: "com.example.myapp", threads: 4 });
```

Static methods:

- `Application.init(options?)`
- `Application.active()`

Instance methods:

- `isThreadSafe()`
- `quit()`
- `run()`
- `post(callback)`
- `dispatch(callback)`
- `make(factory)`
- `poolSubmit(callback)`
- `poolEmplace(callback)`
- `nativeHandle()`

Accessors:

- `native` (raw native handle)

### Webview

Create a native webview window.

```js
import { Webview } from "saucer-nodejs";

const webview = new Webview(app, {
  persistentCookies: false,
  hardwareAcceleration: true,
  storagePath: "./.webview-data",
  userAgent: "MyApp/1.0",
  browserFlags: ["--disable-gpu"],
  preload: "window.__PRELOADED = true;",
});
```

Static methods:

- `Webview.registerScheme(name)`

Properties:

- Window state: `focused` (readonly), `visible`, `minimized`, `maximized`, `resizable`, `decorations`, `alwaysOnTop`, `clickThrough`
- Window geometry: `title`, `size`, `position`, `maxSize`, `minSize`, `fullscreen`, `zoom`
- Webview/browser: `url`, `devTools`, `backgroundColor`, `forceDarkMode`, `contextMenu`, `favicon` (readonly), `pageTitle` (readonly)
- Handles: `parent` (readonly), `native` (readonly)

Methods:

- Window control: `show()`, `hide()`, `close()`, `focus()`, `startDrag()`, `startResize(edge?)`, `setIcon(pathOrBuffer)`
- Navigation/content: `navigate(url)`, `setFile(path)`, `loadHtml(html)`, `reload()`, `back()`, `forward()`
- JavaScript bridge: `execute(code, ...args)`, `evaluate(code, ...args)`, `expose(name, handler, options?)`, `clearExposed(name?)`, `onMessage(callback)`
- Scripts/embedded content: `inject(script)`, `clearScripts()`, `embed(files, policy?)`, `serve(file)`, `clearEmbedded(file?)`
- Custom schemes: `handleScheme(name, handler, policy?)`, `removeScheme(name)`
- Events: `on(event, cb)`, `once(event, cb)`, `off(event, cb)`

Window/Webview events:

- Window events: `resize`, `focus`, `close`, `closed`, `decorated`, `maximize`, `minimize`
- Webview events: `dom-ready`, `load`, `title`, `navigate`, `navigated`, `favicon`

Notes:

- `close` and `navigate` callbacks can return `true`/`false` to allow or deny the action.
- Register schemes with `Webview.registerScheme(...)` before creating `Application/Webview` instances.

### SmartviewRPC and Types

Typed RPC helper with optional zero-copy binary transfer support.

```js
import { Application, Webview, SmartviewRPC, createRPC, Types } from "saucer-nodejs";

SmartviewRPC.registerScheme(); // before Application/Webview creation

const app = Application.init();
const webview = new Webview(app);
const rpc = createRPC(webview);

rpc.define(
  "sum",
  {
    params: [Types.param("a", Types.number), Types.param("b", Types.number)],
    returns: "number",
  },
  (a, b) => a + b,
);
```

`SmartviewRPC` static methods:

- `registerScheme()`
- `isSchemeRegistered()`

`SmartviewRPC` instance methods:

- `define(name, schema, handler)`
- `undefine(name)`
- `getDefinedFunctions()`
- `generateTypes(options?)`

`SmartviewRPC` accessors:

- `webview`

`Types` helpers:

- Scalars: `Types.string`, `Types.number`, `Types.boolean`, `Types.buffer`, `Types.uint8`, `Types.any`, `Types.void`
- Builders: `Types.array(type)`, `Types.object(shape)`, `Types.optional(type)`, `Types.param(name, type)`

### Desktop and PDF

System dialogs and PDF export.

```js
import { Desktop, PDF } from "saucer-nodejs";

const desktop = new Desktop(app);
const pdf = new PDF(webview);
```

`Desktop` methods:

- `open(pathOrUrl)`
- `pickFile(options?)`
- `pickFolder(options?)`
- `pickFiles(options?)`
- `pickFolders(options?)`

`Desktop` accessors:

- `native`

`PDF` methods:

- `save(options?)`

`PDF` accessors:

- `native`

### Stash and Icon

Binary and icon utility wrappers.

```js
import { Stash, Icon } from "saucer-nodejs";

const stash = Stash.from(Buffer.from("hello"));
const icon = Icon.fromFile("./icon.png");
```

`Stash` static methods:

- `Stash.from(buffer)`
- `Stash.view(buffer)`

`Stash` methods/accessors:

- `getData()`
- `size` (readonly)
- `native` (readonly)

`Icon` static methods:

- `Icon.fromFile(path)`
- `Icon.fromData(buffer)`

`Icon` methods/accessors:

- `isEmpty()`
- `getData()`
- `save(path)`
- `native` (readonly)

### Clipboard

```js
import { clipboard } from "saucer-nodejs";

clipboard.writeText("hello");
const text = clipboard.readText();
const hasText = clipboard.hasText();
const hasImage = clipboard.hasImage();
clipboard.clear();
```

Methods:

- `readText()`
- `writeText(text)`
- `hasText()`
- `hasImage()`
- `clear()`

### Notification

```js
import { Notification } from "saucer-nodejs";

if (Notification.isSupported()) {
  const permission = await Notification.requestPermission();
  if (permission === "granted") {
    new Notification({ title: "Done", body: "Task complete" }).show();
  }
}
```

Static methods:

- `Notification.isSupported()`
- `Notification.requestPermission()`

Instance methods:

- `show()`

### SystemTray

```js
import { SystemTray } from "saucer-nodejs";

const tray = new SystemTray();
tray.setTooltip("My App");
tray.setMenu([
  { label: "Open" },
  { type: "separator" },
  { label: "Quit" },
]);
tray.onClick(() => console.log("tray clicked"));
tray.show();
```

Methods:

- `setIcon(pathOrBuffer)`
- `setTooltip(text)`
- `setMenu(items)`
- `onClick(callback)`
- `show()`
- `hide()`
- `destroy()`

## CLI

The package installs a `saucer` CLI.

```bash
# Show help
npx saucer help

# Initialize a new app
npx saucer init my-app

# Run with watch mode
npx saucer dev index.js

# Build a standalone SEA binary
npx saucer build index.js my-app

# Run diagnostics
npx saucer doctor

# Show local package/system info
npx saucer info
```

## Debug Mode

Use `saucer-nodejs/debug` for category logging.

```bash
SAUCER_DEBUG=1 node app.js
SAUCER_DEBUG=rpc,window node app.js
```

```js
import { createDebugger, measure } from "saucer-nodejs/debug";

const debug = createDebugger("rpc");
debug("request", { id: 1 });

await measure("fetch-data", async () => {
  // async work
});
```

Common categories: `rpc`, `window`, `webview`, `native`, `app`, `event`, `perf`.

## Platform Notes

API availability is cross-platform, but some OS integrations are currently partial outside macOS.

- Core app/webview APIs work on macOS, Windows, and Linux
- Some extension APIs (for example tray/menu behavior and selected window extensions) can be partial/stub depending on platform/runtime
- Run `npx saucer info` and `npx saucer doctor` on the target OS to validate behavior

## Testing and API Parity

`examples/application-features.js` is the source-of-truth parity suite for recent binding changes.

```bash
# Fully automated run (CI-safe)
node examples/application-features.js --mode=automated

# Interactive/manual run
node examples/application-features.js --mode=manual
```

Mode can also be provided via env var:

```bash
SAUCER_TEST_MODE=automated node examples/application-features.js
```

The suite validates:

- Public API parity against the v8 manifest
- Event behavior (`on`, `once`, `off`)
- Messaging, RPC, and core desktop integrations

## Examples

- `examples/basic.js` - minimal startup example
- `examples/application-features.js` - full feature and parity coverage

## Building from Source

### Prerequisites

- Node.js >= 20
- CMake >= 3.15
- C++20 compatible compiler
  - macOS: Xcode Command Line Tools
  - Windows: Visual Studio 2019+
  - Linux: GCC 10+ or Clang 12+

### Build

```bash
git clone https://github.com/MrLionware/saucer-nodejs.git
cd saucer-nodejs
npm install
npm run rebuild
```

Run feature/parity suite:

```bash
node examples/application-features.js --mode=automated
```

## Contributing

Contributions are welcome.

1. Fork the repository
2. Create a branch (`git checkout -b feature/my-change`)
3. Commit changes
4. Push branch
5. Open a pull request

## License

MIT - see [LICENSE](LICENSE).
