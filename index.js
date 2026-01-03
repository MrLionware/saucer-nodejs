import { native } from "./lib/native-loader.js";

let activeApp = null;

const wrapNativeApp = (nativeInstance) => {
  const app = Object.create(Application.prototype);
  app._native = nativeInstance;
  activeApp = app;
  return app;
};

/**
 * Saucer Application - manages the application lifecycle and event loop
 */
export class Application {
  constructor(options = {}) {
    this._native = new native.Application(options);
    activeApp = this;
  }

  /**
   * Initialize (or retrieve) the shared application instance
   * @param {ApplicationOptions} options
   * @returns {Application}
   */
  static init(options = {}) {
    if (activeApp) {
      return activeApp;
    }

    if (typeof native.Application.init === "function") {
      return wrapNativeApp(native.Application.init(options));
    }
    return new Application(options);
  }

  /**
   * Get the active application instance if it already exists
   * @returns {Application|null}
   */
  static active() {
    if (activeApp) {
      return activeApp;
    }

    if (typeof native.Application.active !== "function") {
      return null;
    }

    const handle = native.Application.active();
    if (!handle) {
      return null;
    }
    return wrapNativeApp(handle);
  }

  /**
   * Check if the application is thread-safe
   * @returns {boolean}
   */
  isThreadSafe() {
    return this._native.isThreadSafe();
  }

  /**
   * Quit the application
   */
  quit() {
    this._native.quit();
  }

  /**
   * Run the application event loop
   * Note: In Node.js bindings, this is automatically handled by libuv integration
   */
  run() {
    this._native.run();
  }

  /**
   * Post a callback to run on the main thread
   * @param {Function} callback
   */
  post(callback) {
    this._native.post(callback);
  }

  /**
   * Run the callback on the UI thread and resolve with its return value
   * @param {Function} callback
   * @returns {Promise<*>}
   */
  dispatch(callback) {
    return this._native.dispatch(callback);
  }

  /**
   * Submit a task to saucer's thread pool and await completion
   * @param {Function} callback
   * @returns {Promise<*>}
   */
  poolSubmit(callback) {
    return this._native.poolSubmit(callback);
  }

  /**
   * Enqueue a task onto the thread pool without awaiting it
   * @param {Function} callback
   */
  poolEmplace(callback) {
    return this._native.poolEmplace(callback);
  }

  /**
   * Create values on the UI thread (alias of dispatch)
   * @param {Function} factory
   * @returns {Promise<*>}
   */
  make(factory) {
    return this._native.make(factory);
  }

  /**
   * Get the native application handle (unsafe)
   * @returns {*}
   */
  get native() {
    return this._native;
  }

  /**
   * Get the raw native pointer (unsafe)
   * @returns {bigint|null}
   */
  nativeHandle() {
    if (typeof this._native.nativeHandle === "function") {
      return this._native.nativeHandle();
    }

    if (typeof this._native.native === "function") {
      return this._native.native();
    }

    return null;
  }
}

/**
 * Saucer Webview - a native webview window
 */
export class Webview {
  /**
   * Register a custom URL scheme before creating webviews
   * Must be called before Application.init() or creating any Webview instances
   * @param {string} name - Scheme name to register (e.g., 'custom' for custom://)
   */
  static registerScheme(name) {
    native.Webview.registerScheme(name);
  }

  constructor(app, options = {}) {
    if (!(app instanceof Application)) {
      throw new TypeError("First argument must be an Application instance");
    }
    this._native = new native.Webview(app._native, options);
    this._parent = app;
    this._messageHandler = null;
  }

  // ========================================================================
  // Window Properties
  // ========================================================================

  /**
   * Whether the window is focused (read-only)
   * @type {boolean}
   */
  get focused() {
    return this._native.focused;
  }

  /**
   * Get or set window visibility
   * @type {boolean}
   */
  get visible() {
    return this._native.visible;
  }

  set visible(value) {
    this._native.visible = value;
  }

  /**
   * Get or set minimized state
   * @type {boolean}
   */
  get minimized() {
    return this._native.minimized;
  }

  set minimized(value) {
    this._native.minimized = value;
  }

  /**
   * Get or set maximized state
   * @type {boolean}
   */
  get maximized() {
    return this._native.maximized;
  }

  set maximized(value) {
    this._native.maximized = value;
  }

  /**
   * Enable or disable manual resize
   * @type {boolean}
   */
  get resizable() {
    return this._native.resizable;
  }

  set resizable(value) {
    this._native.resizable = value;
  }

  /**
   * Toggle native window decorations
   * @type {boolean}
   */
  get decorations() {
    return this._native.decorations;
  }

  set decorations(value) {
    this._native.decorations = value;
  }

  /**
   * Keep the window always on top
   * @type {boolean}
   */
  get alwaysOnTop() {
    return this._native.alwaysOnTop;
  }

  set alwaysOnTop(value) {
    this._native.alwaysOnTop = value;
  }

  /**
   * Allow mouse clicks to fall through the window
   * @type {boolean}
   */
  get clickThrough() {
    return this._native.clickThrough;
  }

  set clickThrough(value) {
    this._native.clickThrough = value;
  }

  /**
   * Get or set window title
   * @type {string}
   */
  get title() {
    return this._native.title;
  }

  set title(value) {
    this._native.title = value;
  }

  /**
   * Get or set window size
   * @type {{width: number, height: number}}
   */
  get size() {
    return this._native.size;
  }

  set size(value) {
    this._native.size = value;
  }

  /**
   * Maximum window size constraint
   * @type {{width: number, height: number}}
   */
  get maxSize() {
    return this._native.maxSize;
  }

  set maxSize(value) {
    this._native.maxSize = value;
  }

  /**
   * Minimum window size constraint
   * @type {{width: number, height: number}}
   */
  get minSize() {
    return this._native.minSize;
  }

  set minSize(value) {
    this._native.minSize = value;
  }

  /**
   * Parent application instance (read-only)
   * @type {Application}
   */
  get parent() {
    // Prefer the original JS wrapper to keep referential equality
    return this._parent ?? this._native.parent;
  }

  // ========================================================================
  // Extension Properties (platform-native implementations)
  // ========================================================================

  /**
   * Window position on screen { x, y }
   * Origin is top-left of the screen
   * @type {{x: number, y: number}}
   */
  get position() {
    return this._native.position;
  }

  set position(value) {
    this._native.position = value;
  }

  /**
   * Whether window is in fullscreen mode
   * @type {boolean}
   */
  get fullscreen() {
    return this._native.fullscreen;
  }

  set fullscreen(value) {
    this._native.fullscreen = value;
  }

  /**
   * Webview zoom level (1.0 = 100%, 2.0 = 200%, etc.)
   * @type {number}
   */
  get zoom() {
    return this._native.zoom;
  }

  set zoom(value) {
    this._native.zoom = value;
  }

  // ========================================================================
  // Webview Properties
  // ========================================================================

  /**
   * Get or set current URL
   * @type {string}
   */
  get url() {
    return this._native.url;
  }

  set url(value) {
    this._native.url = value;
  }

  /**
   * Get or set developer tools enabled state
   * @type {boolean}
   */
  get devTools() {
    return this._native.devTools;
  }

  set devTools(value) {
    this._native.devTools = value;
  }

  /**
   * Get or set webview background color as RGBA array
   * @type {[number, number, number, number]}
   */
  get backgroundColor() {
    return this._native.backgroundColor;
  }

  set backgroundColor(value) {
    this._native.backgroundColor = value;
  }

  /**
   * Get or set force dark mode
   * @type {boolean}
   */
  get forceDarkMode() {
    return this._native.forceDarkMode;
  }

  set forceDarkMode(value) {
    this._native.forceDarkMode = value;
  }

  /**
   * Get or set context menu enabled state
   * @type {boolean}
   */
  get contextMenu() {
    return this._native.contextMenu;
  }

  set contextMenu(value) {
    this._native.contextMenu = value;
  }

  /**
   * Get the current page favicon as a Buffer (read-only)
   * @type {Buffer|null}
   */
  get favicon() {
    return this._native.favicon;
  }

  /**
   * Get the current page title (read-only)
   * @type {string|null}
   */
  get pageTitle() {
    return this._native.pageTitle;
  }

  // ========================================================================
  // Window Methods
  // ========================================================================

  /**
   * Show the window
   */
  show() {
    this._native.show();
  }

  /**
   * Hide the window
   */
  hide() {
    this._native.hide();
  }

  /**
   * Close the window
   */
  close() {
    this._native.close();
  }

  /**
   * Focus the window
   */
  focus() {
    this._native.focus();
  }

  /**
   * Start a window drag operation (for custom title bars)
   * Call this from a mousedown event handler
   */
  startDrag() {
    this._native.startDrag();
  }

  /**
   * Start a window resize operation
   * @param {string|number|{top?: boolean, bottom?: boolean, left?: boolean, right?: boolean}} [edge] - Edge(s) to resize from
   *   - String: "top", "bottom", "left", "right", "top-left", "bottom-right", etc.
   *   - Number: Bitwise flags (1=top, 2=bottom, 4=left, 8=right)
   *   - Object: { top: true, right: true } for top-right corner
   *   - Default: bottom-right corner
   */
  startResize(edge) {
    this._native.startResize(edge);
  }

  /**
   * Set the window icon
   * @param {string|Buffer} pathOrBuffer - File path to icon image or Buffer containing image data
   */
  setIcon(pathOrBuffer) {
    this._native.setIcon(pathOrBuffer);
  }

  // ========================================================================
  // Webview Methods
  // ========================================================================

  /**
   * Navigate to a URL
   * @param {string} url - URL to navigate to
   */
  navigate(url) {
    this._native.navigate(url);
  }

  /**
   * Load a local HTML file
   * @param {string} filePath - Path to the HTML file
   */
  setFile(filePath) {
    this._native.setFile(filePath);
  }

  /**
   * Load HTML content directly into the webview
   * @param {string} html - HTML content to load
   */
  loadHtml(html) {
    this._native.loadHtml(html);
  }

  /**
   * Execute JavaScript in the webview
   * @param {string} code - JavaScript code to execute
   */
  execute(code, ...args) {
    this._native.execute(code, ...args);
  }

  /**
   * Evaluate JavaScript in the webview and await the result
   * @param {string} code - JavaScript code to evaluate
   * @param {...any} args - Optional parameters to serialize into the code placeholders
   * @returns {Promise<*>}
   */
  evaluate(code, ...args) {
    return this._native.evaluate(code, ...args);
  }

  /**
   * Expose a Node-side function to the webview (Smartview RPC)
   * @param {string} name - Function name to expose
   * @param {Function} handler - Handler invoked from the webview
   * @param {{async?: boolean, launch?: 'sync' | 'async'}} [options]
   */
  expose(name, handler, options = {}) {
    this._native.expose(name, handler, options);
  }

  /**
   * Clear exposed RPC functions
   * @param {string} [name] - Specific function name to clear, or all if not specified
   */
  clearExposed(name) {
    this._native.clearExposed(name);
  }

  /**
   * Reload the current page
   */
  reload() {
    this._native.reload();
  }

  /**
   * Go back in history
   */
  back() {
    this._native.back();
  }

  /**
   * Go forward in history
   */
  forward() {
    this._native.forward();
  }

  /**
   * Inject a script into the webview
   * @param {Object} script - Script configuration
   * @param {string} script.code - JavaScript code to inject
   * @param {'creation' | 'ready'} [script.time='ready'] - When to inject ('creation' or 'ready')
   * @param {'top' | 'all'} [script.frame='top'] - Which frames to inject into
   * @param {boolean} [script.permanent=false] - Whether the script persists across navigations
   */
  inject(script) {
    this._native.inject(script);
  }

  /**
   * Embed files into the webview for serving via saucer:// protocol
   * @param {Object.<string, {content: string|Buffer, mime: string}>} files - Files to embed
   * @param {'sync' | 'async'} [policy='sync'] - Launch policy
   */
  embed(files, policy) {
    this._native.embed(files, policy);
  }

  /**
   * Serve an embedded file (navigates to saucer://file)
   * @param {string} file - Embedded file name to serve
   */
  serve(file) {
    this._native.serve(file);
  }

  /**
   * Clear embedded files
   * @param {string} [file] - Specific file to clear, or all if not specified
   */
  clearEmbedded(file) {
    this._native.clearEmbedded(file);
  }

  /**
   * Clear all injected scripts
   */
  clearScripts() {
    this._native.clearScripts();
  }

  /**
   * Register a custom URL scheme handler
   * @param {string} name - Scheme name (e.g., 'custom' for custom://)
   * @param {Function} handler - Handler function receiving request object
   * @param {'sync' | 'async'} [policy='sync'] - Launch policy
   */
  handleScheme(name, handler, policy) {
    this._native.handleScheme(name, handler, policy);
  }

  /**
   * Remove a custom URL scheme handler
   * @param {string} name - Scheme name to remove
   */
  removeScheme(name) {
    this._native.removeScheme(name);
  }

  // ========================================================================
  // Event Handling
  // ========================================================================

  /**
   * Register an event listener
   * @param {string} event - Event name
   * @param {Function} callback - Callback function
   */
  on(event, callback) {
    this._native.on(event, callback);
  }

  /**
   * Register a one-time event listener
   * @param {string} event - Event name
   * @param {Function} callback - Callback function
   */
  once(event, callback) {
    this._native.once(event, callback);
  }

  /**
   * Remove an event listener
   * @param {string} event - Event name
   * @param {Function} callback - Callback function
   */
  off(event, callback) {
    this._native.off(event, callback);
  }

  /**
   * Set message handler for messages from the webview
   * @param {Function} callback - Callback function that receives messages
   */
  onMessage(callback) {
    this._messageHandler = callback;
    this._native.onMessage(callback);
  }

  /**
   * Get the native webview handle (unsafe)
   * @returns {*}
   */
  get native() {
    return this._native;
  }
}

/**
 * Saucer Stash - raw byte buffer wrapper
 */
export class Stash {
  /**
   * Create a Stash from a Buffer (copies the data)
   * @param {Buffer} buffer - Source buffer
   * @returns {Stash|null}
   */
  static from(buffer) {
    const nativeStash = native.Stash.from(buffer);
    if (!nativeStash) return null;
    const stash = Object.create(Stash.prototype);
    stash._native = nativeStash;
    return stash;
  }

  /**
   * Create a Stash view of a Buffer (does not copy data)
   * @param {Buffer} buffer - Source buffer
   * @returns {Stash|null}
   */
  static view(buffer) {
    const nativeStash = native.Stash.view(buffer);
    if (!nativeStash) return null;
    const stash = Object.create(Stash.prototype);
    stash._native = nativeStash;
    return stash;
  }

  /**
   * Get the size of the stash in bytes
   * @type {number}
   */
  get size() {
    return this._native.size;
  }

  /**
   * Get the stash data as a Buffer
   * @returns {Buffer|null}
   */
  getData() {
    return this._native.getData();
  }

  /**
   * Get the native stash handle (unsafe)
   * @returns {*}
   */
  get native() {
    return this._native;
  }
}

/**
 * Saucer Icon - image wrapper for window icons and favicons
 */
export class Icon {
  /**
   * Create an Icon from a file path
   * @param {string} path - Path to the image file
   * @returns {Icon|null}
   */
  static fromFile(path) {
    const nativeIcon = native.Icon.fromFile(path);
    if (!nativeIcon) return null;
    const icon = Object.create(Icon.prototype);
    icon._native = nativeIcon;
    return icon;
  }

  /**
   * Create an Icon from image data Buffer
   * @param {Buffer} buffer - Image data buffer
   * @returns {Icon|null}
   */
  static fromData(buffer) {
    const nativeIcon = native.Icon.fromData(buffer);
    if (!nativeIcon) return null;
    const icon = Object.create(Icon.prototype);
    icon._native = nativeIcon;
    return icon;
  }

  /**
   * Check if the icon is empty
   * @returns {boolean}
   */
  isEmpty() {
    return this._native.isEmpty();
  }

  /**
   * Get the icon data as a Buffer
   * @returns {Buffer|null}
   */
  getData() {
    return this._native.getData();
  }

  /**
   * Save the icon to a file
   * @param {string} path - Path to save the image to
   */
  save(path) {
    this._native.save(path);
  }

  /**
   * Get the native icon handle (unsafe)
   * @returns {*}
   */
  get native() {
    return this._native;
  }
}

/**
 * Saucer Desktop - Native file dialogs and system integration
 */
export class Desktop {
  /**
   * Create a Desktop instance for system integration
   * @param {Application} app - Application instance
   */
  constructor(app) {
    if (!(app instanceof Application)) {
      throw new TypeError("First argument must be an Application instance");
    }
    this._native = new native.Desktop(app);
  }

  /**
   * Open a file or URL with the default system application
   * @param {string} path - File path or URL to open
   */
  open(path) {
    this._native.open(path);
  }

  /**
   * Show a file picker dialog
   * @param {{initial?: string, filters?: string[]}} [options] - Picker options
   * @returns {string|null} - Selected file path or null if cancelled
   */
  pickFile(options) {
    return this._native.pickFile(options);
  }

  /**
   * Show a folder picker dialog
   * @param {{initial?: string}} [options] - Picker options
   * @returns {string|null} - Selected folder path or null if cancelled
   */
  pickFolder(options) {
    return this._native.pickFolder(options);
  }

  /**
   * Show a multi-file picker dialog
   * @param {{initial?: string, filters?: string[]}} [options] - Picker options
   * @returns {string[]|null} - Selected file paths or null if cancelled
   */
  pickFiles(options) {
    return this._native.pickFiles(options);
  }

  /**
   * Show a multi-folder picker dialog
   * @param {{initial?: string}} [options] - Picker options
   * @returns {string[]|null} - Selected folder paths or null if cancelled
   */
  pickFolders(options) {
    return this._native.pickFolders(options);
  }

  /**
   * Get the native desktop handle (unsafe)
   * @returns {*}
   */
  get native() {
    return this._native;
  }
}

/**
 * Saucer PDF - Export webview content to PDF
 */
export class PDF {
  /**
   * Create a PDF exporter for a webview
   * @param {Webview} webview - Webview instance
   */
  constructor(webview) {
    if (!(webview instanceof Webview)) {
      throw new TypeError("First argument must be a Webview instance");
    }
    this._native = new native.PDF(webview);
  }

  /**
   * Save the webview content to a PDF file
   * @param {{file?: string, orientation?: 'portrait'|'landscape', width?: number, height?: number}} [options] - Print settings
   */
  save(options) {
    this._native.save(options);
  }

  /**
   * Get the native PDF handle (unsafe)
   * @returns {*}
   */
  get native() {
    return this._native;
  }
}

/**
 * Type schema builders for SmartviewRPC
 * Used to define parameter and return types for exposed functions
 */
export const Types = {
  /** String type */
  string: { type: "string" },
  /** Number type */
  number: { type: "number" },
  /** Boolean type */
  boolean: { type: "boolean" },
  /** Node.js Buffer type (binary, uses zero-copy transfer) */
  buffer: { type: "Buffer", binary: true },
  /** Uint8Array type (binary, uses zero-copy transfer) */
  uint8: { type: "Uint8Array", binary: true },
  /** Any type (no type checking) */
  any: { type: "any" },
  /** Void type (no return value) */
  void: { type: "void" },

  /**
   * Array type
   * @param {Object} itemType - Type of array elements
   */
  array: (itemType) => ({
    type: `${itemType.type}[]`,
    binary: itemType.binary,
  }),

  /**
   * Object type with named properties
   * @param {Object} props - Object mapping property names to types
   */
  object: (props) => ({
    type: `{ ${Object.entries(props)
      .map(([k, v]) => `${k}: ${v.type}`)
      .join("; ")} }`,
    binary: Object.values(props).some((v) => v.binary),
  }),

  /**
   * Mark a type as optional
   * @param {Object} t - Type to make optional
   */
  optional: (t) => ({ ...t, optional: true }),

  /**
   * Create a named parameter with a type
   * @param {string} name - Parameter name
   * @param {Object} type - Parameter type schema
   */
  param: (name, type) => ({ name, ...type }),
};

// Track if saucer-rpc scheme has been registered globally
let _saucerRpcSchemeRegistered = false;

/**
 * SmartviewRPC - Type-safe RPC builder with zero-copy Buffer support
 *
 * Provides a fluent API for defining exposed functions with type metadata.
 * Automatically routes binary data through scheme handlers for zero-copy transfer.
 *
 * IMPORTANT: If using binary types (Buffer/Uint8Array), you must call
 * SmartviewRPC.registerScheme() BEFORE creating any Webview instances.
 */
export class SmartviewRPC {
  /**
   * Register the saucer-rpc:// scheme for binary RPC transfers.
   * Must be called BEFORE Application.init() or creating any Webview instances.
   *
   * @example
   * ```javascript
   * // At the very start of your app, before any webviews:
   * SmartviewRPC.registerScheme();
   *
   * const app = Application.init();
   * const webview = new Webview(app);
   * const rpc = createRPC(webview);
   * ```
   */
  static registerScheme() {
    if (_saucerRpcSchemeRegistered) return;
    Webview.registerScheme("saucer-rpc");
    _saucerRpcSchemeRegistered = true;
  }

  /**
   * Check if the binary RPC scheme has been registered
   * @returns {boolean}
   */
  static isSchemeRegistered() {
    return _saucerRpcSchemeRegistered;
  }

  /**
   * Create a new SmartviewRPC builder
   * @param {Webview} webview - Webview instance to attach RPC to
   */
  constructor(webview) {
    if (!(webview instanceof Webview)) {
      throw new TypeError("First argument must be a Webview instance");
    }
    this._webview = webview;
    this._definitions = new Map();
    this._binaryHandlers = new Map();
    this._binarySchemeSetup = false;
    this._helperInjected = false;
  }

  /**
   * Check if a schema uses binary types
   * @private
   */
  _hasBinaryTypes(schema) {
    const params = schema.params || [];
    const hasBinaryParam = params.some((p) => p.binary);
    const hasBinaryReturn =
      schema.returns === "Buffer" || schema.returns === "Uint8Array";
    return hasBinaryParam || hasBinaryReturn;
  }

  /**
   * Set up the binary RPC scheme handler
   * @private
   */
  _setupBinaryScheme() {
    if (this._binarySchemeSetup) return;

    // Warn if scheme wasn't pre-registered
    if (!_saucerRpcSchemeRegistered) {
      console.warn(
        "[SmartviewRPC] Warning: Binary RPC scheme not registered. " +
        "Call SmartviewRPC.registerScheme() BEFORE creating any Webview instances " +
        "for binary transfers to work correctly.",
      );
    }

    // CORS headers to allow cross-origin requests from saucer:// pages
    const corsHeaders = {
      "Access-Control-Allow-Origin": "*",
      "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
      "Access-Control-Allow-Headers": "Content-Type",
    };

    this._webview.handleScheme(
      "saucer-rpc",
      async (request) => {
        // Handle CORS preflight
        if (request.method === "OPTIONS") {
          return {
            data: "",
            mime: "text/plain",
            status: 204,
            headers: corsHeaders,
          };
        }

        // Parse function name from URL: saucer-rpc://functionName/path?query
        let funcName;
        try {
          const url = new URL(request.url);
          funcName = url.hostname;
        } catch {
          // Fallback: extract from saucer-rpc://name format
          funcName = request.url.replace("saucer-rpc://", "").split("/")[0];
        }

        const handler = this._binaryHandlers.get(funcName);
        if (!handler) {
          return {
            data: JSON.stringify({ error: `Function '${funcName}' not found` }),
            mime: "application/json",
            status: 404,
            headers: corsHeaders,
          };
        }

        try {
          // request.content is already a Buffer (zero-copy via stash)
          const result = await handler(request.content, request.headers);

          // Return Buffer directly for zero-copy
          if (Buffer.isBuffer(result) || result instanceof Uint8Array) {
            return {
              data: result,
              mime: "application/octet-stream",
              status: 200,
              headers: corsHeaders,
            };
          }

          // Return JSON for non-binary results
          return {
            data: JSON.stringify(result),
            mime: "application/json",
            status: 200,
            headers: corsHeaders,
          };
        } catch (err) {
          return {
            data: JSON.stringify({ error: err.message }),
            mime: "application/json",
            status: 500,
            headers: corsHeaders,
          };
        }
      },
      "async",
    );

    this._binarySchemeSetup = true;
  }

  /**
   * Inject the webview helper for calling binary functions
   * @private
   */
  _injectBinaryHelper() {
    if (this._helperInjected) return;

    this._webview.inject({
      code: `
        (function() {
          if (window.saucer && window.saucer.callBinary) return;

          window.saucer = window.saucer || {};

          /**
           * Call a binary RPC function with zero-copy transfer
           * @param {string} name - Function name
           * @param {Uint8Array|ArrayBuffer} buffer - Binary data to send
           * @returns {Promise<Uint8Array>} - Binary response
           */
          window.saucer.callBinary = async function(name, buffer) {
            const response = await fetch('saucer-rpc://' + name, {
              method: 'POST',
              body: buffer instanceof ArrayBuffer ? new Uint8Array(buffer) : buffer,
              headers: {
                'Content-Type': 'application/octet-stream'
              }
            });

            if (!response.ok) {
              const error = await response.json().catch(() => ({ error: 'Unknown error' }));
              throw new Error(error.error || 'RPC call failed');
            }

            const contentType = response.headers.get('Content-Type') || '';
            if (contentType.includes('application/json')) {
              return response.json();
            }

            return new Uint8Array(await response.arrayBuffer());
          };
        })();
      `,
      time: "creation",
      permanent: true,
    });

    this._helperInjected = true;
  }

  /**
   * Define an exposed RPC function with type metadata
   * @param {string} name - Function name to expose
   * @param {Object} schema - Type schema for the function
   * @param {Array} [schema.params] - Parameter type definitions
   * @param {string} [schema.returns] - Return type name
   * @param {boolean} [schema.async] - Whether the function is async
   * @param {Function} handler - Handler function
   * @returns {SmartviewRPC} - Returns this for chaining
   */
  define(name, schema, handler) {
    if (typeof name !== "string" || !name) {
      throw new TypeError("Function name must be a non-empty string");
    }
    if (typeof handler !== "function") {
      throw new TypeError("Handler must be a function");
    }

    // Store definition for type generation
    this._definitions.set(name, { name, ...schema });

    // Check if this function uses binary types
    const usesBinary = this._hasBinaryTypes(schema);

    if (usesBinary) {
      // Set up binary transport
      this._setupBinaryScheme();
      this._injectBinaryHelper();
      this._binaryHandlers.set(name, handler);
    } else {
      // Use standard JSON RPC
      this._webview.expose(name, handler, { async: schema.async });
    }

    return this;
  }

  /**
   * Remove an exposed RPC function
   * @param {string} name - Function name to remove
   * @returns {SmartviewRPC} - Returns this for chaining
   */
  undefine(name) {
    this._definitions.delete(name);
    this._binaryHandlers.delete(name);
    this._webview.clearExposed(name);
    return this;
  }

  /**
   * Get all defined function names
   * @returns {string[]} - Array of function names
   */
  getDefinedFunctions() {
    return Array.from(this._definitions.keys());
  }

  /**
   * Generate TypeScript interface definitions for all exposed functions
   * @param {Object} [options] - Generation options
   * @param {string} [options.namespace='saucer'] - Namespace for the interface
   * @returns {string} - TypeScript definition string
   */
  generateTypes(options = {}) {
    const namespace = options.namespace || "saucer";

    let output = "// Auto-generated by saucer-nodejs SmartviewRPC\n";
    output += "// Do not edit manually\n\n";
    output += "declare global {\n";
    output += "  interface Window {\n";
    output += `    ${namespace}: {\n`;
    output += "      exposed: {\n";

    for (const [name, def] of this._definitions) {
      // Skip binary functions - they use callBinary instead
      if (this._hasBinaryTypes(def)) continue;

      // Build parameter string
      const params = (def.params || [])
        .map((p, i) => {
          const paramName = p.name || `arg${i}`;
          const optional = p.optional ? "?" : "";
          return `${paramName}${optional}: ${p.type}`;
        })
        .join(", ");

      // Build return type
      let returnType = def.returns || "void";
      if (def.async) {
        returnType = `Promise<${returnType}>`;
      }

      // Add JSDoc if we have parameter info
      if (def.params && def.params.length > 0) {
        output += "        /**\n";
        for (const p of def.params) {
          if (p.name) {
            output += `         * @param ${p.name} - ${p.type}${p.optional ? " (optional)" : ""}\n`;
          }
        }
        output += `         * @returns ${returnType}\n`;
        output += "         */\n";
      }

      output += `        ${name}(${params}): ${returnType};\n`;
    }

    output += "      };\n";

    // Add callBinary helper if any binary functions exist
    if (this._binaryHandlers.size > 0) {
      output += "\n";
      output += "      /**\n";
      output += "       * Call a binary RPC function with zero-copy transfer\n";
      output += "       * @param name - Function name\n";
      output += "       * @param buffer - Binary data to send\n";
      output += "       */\n";
      output +=
        "      callBinary(name: string, buffer: Uint8Array | ArrayBuffer): Promise<Uint8Array>;\n";
    }

    output += "    };\n";
    output += "  }\n";
    output += "}\n\n";
    output += "export {};\n";

    return output;
  }

  /**
   * Get the underlying webview
   * @returns {Webview}
   */
  get webview() {
    return this._webview;
  }
}

/**
 * Create a new SmartviewRPC builder for a webview
 * @param {Webview} webview - Webview instance
 * @returns {SmartviewRPC}
 */
export function createRPC(webview) {
  return new SmartviewRPC(webview);
}

// ========================================================================
// Premium Features - Clipboard, Notification, SystemTray
// ========================================================================

/**
 * Clipboard API for reading and writing to the system clipboard
 */
export const clipboard = native.clipboard;

/**
 * System notification class
 * @example
 * const notification = new Notification({
 *   title: 'Hello',
 *   body: 'World!'
 * });
 * notification.show();
 */
export const Notification = native.Notification;

/**
 * System tray icon class (menu bar on macOS)
 * @example
 * const tray = new SystemTray();
 * tray.setIcon('./icon.png');
 * tray.setTooltip('My App');
 * tray.setMenu([
 *   { label: 'Show' },
 *   { type: 'separator' },
 *   { label: 'Quit' }
 * ]);
 * tray.show();
 */
export const SystemTray = native.SystemTray;

// Export native module for advanced usage
export { native };

// Default export
export default {
  Application,
  Webview,
  Icon,
  Stash,
  Desktop,
  PDF,
  SmartviewRPC,
  Types,
  createRPC,
  clipboard,
  Notification,
  SystemTray,
  native,
};
