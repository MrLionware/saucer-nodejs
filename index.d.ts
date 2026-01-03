/**
 * TypeScript definitions for saucer-nodejs
 * Node.js bindings for the saucer webview library
 */

/**
 * Application options
 */
export interface ApplicationOptions {
  /**
   * Application identifier (bundle id)
   * @default com.saucer.nodejs
   */
  id?: string;

  /**
   * Number of threads for the thread pool
   * @default CPU core count
   */
  threads?: number;
}

/**
 * Webview preferences
 */
export interface WebviewOptions {
  /**
   * Enable persistent cookies
   * @default false
   */
  persistentCookies?: boolean;

  /**
   * Enable hardware acceleration
   * @default true
   */
  hardwareAcceleration?: boolean;

  /**
   * Storage path for webview data
   */
  storagePath?: string;

  /**
   * User agent string
   */
  userAgent?: string;

  /**
   * Browser-specific flags (e.g., Chromium command-line switches)
   * Example: ['--disable-gpu', '--enable-features=SomeFeature']
   */
  browserFlags?: string[];

  /**
   * JavaScript code to inject before page loads (preload script)
   * This script runs at creation time and persists across navigations
   */
  preload?: string;
}

/**
 * Window size
 */
export interface WindowSize {
  width: number;
  height: number;
}

/**
 * Script injection options
 */
export interface InjectScript {
  /**
   * JavaScript code to inject
   */
  code: string;

  /**
   * When to inject the script
   * - 'creation': Before page loads
   * - 'ready': After DOM is ready
   * @default 'ready'
   */
  time?: "creation" | "ready";

  /**
   * Which frames to inject into
   * - 'top': Only the top-level frame
   * - 'all': All frames including iframes
   * @default 'top'
   */
  frame?: "top" | "all";

  /**
   * Whether the script persists across navigations
   * @default false
   */
  permanent?: boolean;
}

/**
 * Embedded file content
 */
export interface EmbeddedFile {
  /**
   * File content as string or Buffer
   */
  content: string | Buffer;

  /**
   * MIME type of the content
   */
  mime: string;
}

/**
 * Custom scheme request
 */
export interface SchemeRequest {
  /**
   * Full URL of the request
   */
  url: string;

  /**
   * HTTP method (GET, POST, etc.)
   */
  method: string;

  /**
   * Request body content (null if no body)
   */
  content: Buffer | null;

  /**
   * Request headers
   */
  headers: Record<string, string>;
}

/**
 * Custom scheme response
 */
export interface SchemeResponse {
  /**
   * Response data as string or Buffer
   */
  data: string | Buffer;

  /**
   * MIME type of the response
   */
  mime: string;

  /**
   * HTTP status code
   * @default 200
   */
  status?: number;

  /**
   * Response headers
   */
  headers?: Record<string, string>;
}

/**
 * Scheme handler function type
 */
export type SchemeHandler = (
  request: SchemeRequest,
) => SchemeResponse | Promise<SchemeResponse>;

/**
 * Launch policy for async operations
 */
export type LaunchPolicy = "sync" | "async";

/**
 * Window edge flags for resize operations
 */
export interface WindowEdge {
  top?: boolean;
  bottom?: boolean;
  left?: boolean;
  right?: boolean;
}

/**
 * Window edge as string (can combine with hyphen)
 * Examples: "top", "bottom-right", "left", "top-left"
 */
export type WindowEdgeString =
  | "top"
  | "bottom"
  | "left"
  | "right"
  | "top-left"
  | "top-right"
  | "bottom-left"
  | "bottom-right"
  | string;

/**
 * Window edge flags as number (bitwise)
 * top=1, bottom=2, left=4, right=8
 */
export type WindowEdgeFlags = number;

/**
 * Window event names
 */
export type WindowEvent =
  | "decorated"
  | "maximize"
  | "minimize"
  | "closed"
  | "resize"
  | "focus"
  | "close";

/**
 * Webview event names
 */
export type WebviewEvent =
  | "dom-ready"
  | "navigated"
  | "navigate"
  | "favicon"
  | "title"
  | "load";

/**
 * All event names
 */
export type EventName = WindowEvent | WebviewEvent;

/**
 * Saucer Application - manages the application lifecycle and event loop
 */
export class Application {
  /**
   * Initialize (or reuse) the shared application
   */
  static init(options?: ApplicationOptions): Application;

  /**
   * Get the active application if one exists
   */
  static active(): Application | null;

  /**
   * Create a new application
   * @param options Application options
   */
  constructor(options?: ApplicationOptions);

  /**
   * Check if the application is thread-safe
   * @returns true if thread-safe operations are supported
   */
  isThreadSafe(): boolean;

  /**
   * Quit the application
   */
  quit(): void;

  /**
   * Run the application event loop
   * Note: In Node.js bindings, this is automatically handled by libuv integration
   */
  run(): void;

  /**
   * Post a callback to run on the main thread
   * @param callback Function to execute on the main thread
   */
  post(callback: () => void): void;

  /**
   * Dispatch work to the UI thread and resolve with the return value
   */
  dispatch<T = unknown>(callback: () => T): Promise<T>;

  /**
   * Submit work to saucer's thread pool and await completion
   */
  poolSubmit<T = unknown>(callback: () => T): Promise<T>;

  /**
   * Enqueue work on the thread pool without awaiting it
   */
  poolEmplace(callback: () => void): void;

  /**
   * Create values on the UI thread (alias for dispatch)
   */
  make<T = unknown>(factory: () => T): Promise<T>;

  /**
   * Get the raw native application pointer (unsafe)
   */
  nativeHandle(): bigint | null;

  /**
   * Underlying native binding handle
   */
  readonly native: unknown;
}

/**
 * Saucer Webview - a native webview window
 */
export class Webview {
  /**
   * Register a custom URL scheme before creating webviews
   * Must be called before Application.init() or creating any Webview instances
   * @param name Scheme name to register (e.g., 'custom' for custom://)
   */
  static registerScheme(name: string): void;

  /**
   * Create a new webview
   * @param app Application instance
   * @param options Webview preferences
   */
  constructor(app: Application, options?: WebviewOptions);

  // ========================================================================
  // Window Properties
  // ========================================================================

  /**
   * Whether the window is currently focused
   */
  readonly focused: boolean;

  /**
   * Get or set window visibility
   */
  visible: boolean;

  /**
   * Get or set minimized state
   */
  minimized: boolean;

  /**
   * Get or set maximized state
   */
  maximized: boolean;

  /**
   * Whether the window can be resized by the user
   */
  resizable: boolean;

  /**
   * Enable or disable native window decorations
   */
  decorations: boolean;

  /**
   * Keep the window above others
   */
  alwaysOnTop: boolean;

  /**
   * Allow mouse clicks to fall through the window
   */
  clickThrough: boolean;

  /**
   * Get or set window title
   */
  title: string;

  /**
   * Get or set window size
   */
  size: WindowSize;

  /**
   * Maximum allowed window size
   */
  maxSize: WindowSize;

  /**
   * Minimum allowed window size
   */
  minSize: WindowSize;

  // ========================================================================
  // Extension Properties (platform-native implementations)
  // ========================================================================

  /**
   * Window position on screen { x, y }
   * Origin is top-left of the screen
   */
  position: { x: number; y: number };

  /**
   * Whether window is in fullscreen mode
   */
  fullscreen: boolean;

  /**
   * Webview zoom level (1.0 = 100%, 2.0 = 200%, etc.)
   */
  zoom: number;

  /**
   * Parent application instance
   */
  readonly parent: Application;

  // ========================================================================
  // Webview Properties
  // ========================================================================

  /**
   * Get or set current URL
   */
  url: string;

  /**
   * Get or set developer tools enabled state
   */
  devTools: boolean;

  /**
   * Get or set background color as RGBA array [r, g, b, a]
   * Each value is 0-255
   */
  backgroundColor: [number, number, number, number];

  /**
   * Get or set force dark mode
   */
  forceDarkMode: boolean;

  /**
   * Get or set context menu enabled state
   */
  contextMenu: boolean;

  /**
   * Get the current page favicon as raw image data (read-only)
   */
  readonly favicon: Buffer | null;

  /**
   * Get the current page title (read-only)
   */
  readonly pageTitle: string | null;

  // ========================================================================
  // Window Methods
  // ========================================================================

  /**
   * Show the window
   */
  show(): void;

  /**
   * Hide the window
   */
  hide(): void;

  /**
   * Close the window
   */
  close(): void;

  /**
   * Focus the window
   */
  focus(): void;

  /**
   * Start a window drag operation (for custom title bars)
   * Call this from a mousedown event handler
   */
  startDrag(): void;

  /**
   * Start a window resize operation
   * @param edge Edge(s) to resize from. Defaults to bottom-right corner.
   */
  startResize(edge?: WindowEdgeString | WindowEdgeFlags | WindowEdge): void;

  /**
   * Set the window icon
   * @param pathOrBuffer File path to icon image or Buffer containing image data
   */
  setIcon(pathOrBuffer: string | Buffer): void;

  // ========================================================================
  // Webview Methods
  // ========================================================================

  /**
   * Navigate to a URL
   * @param url URL to navigate to
   */
  navigate(url: string): void;

  /**
   * Load a local HTML file
   * @param filePath Path to the HTML file
   */
  setFile(filePath: string): void;

  /**
   * Load HTML content directly into the webview
   * @param html HTML content to load
   */
  loadHtml(html: string): void;

  /**
   * Execute JavaScript in the webview
   * @param code JavaScript code to execute
   */
  execute(code: string, ...args: any[]): void;

  /**
   * Evaluate JavaScript in the webview and resolve with the result
   * @param code JavaScript code to evaluate
   * @param args Optional parameters to serialize into the code placeholders
   */
  evaluate<T = unknown>(code: string, ...args: any[]): Promise<T>;

  /**
   * Expose a Node-side function to the webview (Smartview RPC)
   * @param name Function name to expose
   * @param handler Handler to invoke
   * @param options Execution options
   */
  expose(
    name: string,
    handler: (...args: any[]) => any,
    options?: { async?: boolean; launch?: "sync" | "async" },
  ): void;

  /**
   * Clear exposed RPC functions
   * @param name Specific function name to clear, or all if not specified
   */
  clearExposed(name?: string): void;

  /**
   * Reload the current page
   */
  reload(): void;

  /**
   * Go back in history
   */
  back(): void;

  /**
   * Go forward in history
   */
  forward(): void;

  /**
   * Inject a script into the webview
   * @param script Script configuration
   */
  inject(script: InjectScript): void;

  /**
   * Embed files into the webview for serving via saucer:// protocol
   * @param files Map of filename to embedded file content
   * @param policy Launch policy ('sync' or 'async')
   */
  embed(
    files: Record<string, EmbeddedFile>,
    policy?: LaunchPolicy,
  ): void;

  /**
   * Serve an embedded file (navigates to saucer://file)
   * @param file Embedded file name to serve
   */
  serve(file: string): void;

  /**
   * Clear embedded files
   * @param file Specific file to clear, or all if not specified
   */
  clearEmbedded(file?: string): void;

  /**
   * Clear all injected scripts
   */
  clearScripts(): void;

  /**
   * Register a custom URL scheme handler
   * @param name Scheme name (e.g., 'custom' for custom://)
   * @param handler Handler function receiving request object
   * @param policy Launch policy ('sync' or 'async')
   */
  handleScheme(
    name: string,
    handler: SchemeHandler,
    policy?: LaunchPolicy,
  ): void;

  /**
   * Remove a custom URL scheme handler
   * @param name Scheme name to remove
   */
  removeScheme(name: string): void;

  // ========================================================================
  // Event Handling
  // ========================================================================

  /**
   * Register an event listener
   * @param event Event name
   * @param callback Callback function
   */
  on(event: EventName, callback: (...args: any[]) => void): void;

  /**
   * Register a one-time event listener
   * @param event Event name
   * @param callback Callback function
   */
  once(event: EventName, callback: (...args: any[]) => void): void;

  /**
   * Remove an event listener
   * @param event Event name
   * @param callback Callback function
   */
  off(event: EventName, callback: (...args: any[]) => void): void;

  /**
   * Set message handler for messages from the webview
   * @param callback Callback function that receives messages
   */
  onMessage(callback: (message: string) => boolean | void): void;
}

/**
 * Saucer Stash - raw byte buffer wrapper for efficient data handling
 */
export class Stash {
  /**
   * Create a Stash from a Buffer (copies the data)
   * @param buffer Source buffer
   * @returns Stash instance or null on failure
   */
  static from(buffer: Buffer): Stash | null;

  /**
   * Create a Stash view of a Buffer (does not copy data)
   * Note: The source buffer must remain valid for the lifetime of the stash
   * @param buffer Source buffer
   * @returns Stash instance or null on failure
   */
  static view(buffer: Buffer): Stash | null;

  /**
   * Get the size of the stash in bytes
   */
  readonly size: number;

  /**
   * Get the stash data as a Buffer
   * @returns Buffer copy of the data or null if empty
   */
  getData(): Buffer | null;

  /**
   * Underlying native binding handle
   */
  readonly native: unknown;
}

/**
 * Saucer Icon - image wrapper for window icons and favicons
 */
export class Icon {
  /**
   * Create an Icon from a file path
   * @param path Path to the image file (PNG, JPG, etc.)
   * @returns Icon instance or null on failure
   */
  static fromFile(path: string): Icon | null;

  /**
   * Create an Icon from image data Buffer
   * @param buffer Image data buffer (PNG, JPG, etc.)
   * @returns Icon instance or null on failure
   */
  static fromData(buffer: Buffer): Icon | null;

  /**
   * Check if the icon is empty (no image data)
   * @returns true if the icon is empty
   */
  isEmpty(): boolean;

  /**
   * Get the icon data as a Buffer
   * @returns Buffer containing image data or null if empty
   */
  getData(): Buffer | null;

  /**
   * Save the icon to a file
   * @param path Path to save the image to
   */
  save(path: string): void;

  /**
   * Underlying native binding handle
   */
  readonly native: unknown;
}

/**
 * Picker dialog options
 */
export interface PickerOptions {
  /**
   * Initial directory to open
   */
  initial?: string;

  /**
   * File type filters (e.g., ["*.txt", "*.md"])
   * Only applicable for file pickers
   */
  filters?: string[];
}

/**
 * PDF print settings
 */
export interface PrintSettings {
  /**
   * Output file path
   */
  file?: string;

  /**
   * Page orientation
   * @default 'portrait'
   */
  orientation?: "portrait" | "landscape";

  /**
   * Page width in inches
   */
  width?: number;

  /**
   * Page height in inches
   */
  height?: number;
}

/**
 * Saucer Desktop - Native file dialogs and system integration
 */
export class Desktop {
  /**
   * Create a Desktop instance for system integration
   * @param app Application instance
   */
  constructor(app: Application);

  /**
   * Open a file or URL with the default system application
   * @param path File path or URL to open
   */
  open(path: string): void;

  /**
   * Show a file picker dialog
   * @param options Picker options
   * @returns Selected file path or null if cancelled
   */
  pickFile(options?: PickerOptions): string | null;

  /**
   * Show a folder picker dialog
   * @param options Picker options
   * @returns Selected folder path or null if cancelled
   */
  pickFolder(options?: PickerOptions): string | null;

  /**
   * Show a multi-file picker dialog
   * @param options Picker options
   * @returns Selected file paths or null if cancelled
   */
  pickFiles(options?: PickerOptions): string[] | null;

  /**
   * Show a multi-folder picker dialog
   * @param options Picker options
   * @returns Selected folder paths or null if cancelled
   */
  pickFolders(options?: PickerOptions): string[] | null;

  /**
   * Underlying native binding handle
   */
  readonly native: unknown;
}

/**
 * Saucer PDF - Export webview content to PDF
 */
export class PDF {
  /**
   * Create a PDF exporter for a webview
   * @param webview Webview instance
   */
  constructor(webview: Webview);

  /**
   * Save the webview content to a PDF file
   * @param options Print settings
   */
  save(options?: PrintSettings): void;

  /**
   * Underlying native binding handle
   */
  readonly native: unknown;
}

/**
 * Type schema for RPC parameters and return types
 */
export interface TypeSchema {
  /** TypeScript type string (e.g., 'string', 'Buffer', 'number[]') */
  type: string;
  /** Parameter name (for named parameters) */
  name?: string;
  /** Whether this parameter is optional */
  optional?: boolean;
  /** Whether this type requires binary transfer */
  binary?: boolean;
}

/**
 * RPC function definition schema
 */
export interface RPCSchema {
  /** Parameter type definitions */
  params?: TypeSchema[];
  /** Return type name (e.g., 'string', 'Buffer', 'void') */
  returns?: string;
  /** Whether the function is async */
  async?: boolean;
}

/**
 * Type schema builders for SmartviewRPC
 */
export const Types: {
  /** String type */
  readonly string: TypeSchema;
  /** Number type */
  readonly number: TypeSchema;
  /** Boolean type */
  readonly boolean: TypeSchema;
  /** Node.js Buffer type (uses zero-copy binary transfer) */
  readonly buffer: TypeSchema;
  /** Uint8Array type (uses zero-copy binary transfer) */
  readonly uint8: TypeSchema;
  /** Any type (no type checking) */
  readonly any: TypeSchema;
  /** Void type (no return value) */
  readonly void: TypeSchema;

  /**
   * Create an array type
   * @param itemType - Type of array elements
   */
  array(itemType: TypeSchema): TypeSchema;

  /**
   * Create an object type with named properties
   * @param props - Object mapping property names to types
   */
  object(props: Record<string, TypeSchema>): TypeSchema;

  /**
   * Mark a type as optional
   * @param type - Type to make optional
   */
  optional(type: TypeSchema): TypeSchema;

  /**
   * Create a named parameter with a type
   * @param name - Parameter name
   * @param type - Parameter type schema
   */
  param(name: string, type: TypeSchema): TypeSchema;
};

/**
 * Options for TypeScript generation
 */
export interface GenerateTypesOptions {
  /** Namespace for the generated interface (default: 'saucer') */
  namespace?: string;
}

/**
 * SmartviewRPC - Type-safe RPC builder with zero-copy Buffer support
 *
 * Provides a fluent API for defining exposed functions with type metadata.
 * Automatically routes binary data through scheme handlers for zero-copy transfer.
 *
 * @example
 * ```typescript
 * const rpc = createRPC(webview);
 *
 * rpc
 *   .define('greet', {
 *     params: [Types.param('name', Types.string)],
 *     returns: 'string',
 *   }, (name) => `Hello, ${name}!`)
 *
 *   .define('processImage', {
 *     params: [Types.param('data', Types.buffer)],
 *     returns: 'Buffer',
 *     async: true,
 *   }, async (imageBuffer) => {
 *     // Zero-copy: imageBuffer is a real Buffer
 *     return processedBuffer;
 *   });
 *
 * // Generate TypeScript definitions
 * writeFileSync('saucer-rpc.d.ts', rpc.generateTypes());
 * ```
 */
export class SmartviewRPC {
  /**
   * Register the saucer-rpc:// scheme for binary RPC transfers.
   * Must be called BEFORE Application.init() or creating any Webview instances.
   *
   * @example
   * ```typescript
   * // At the very start of your app, before any webviews:
   * SmartviewRPC.registerScheme();
   *
   * const app = Application.init();
   * const webview = new Webview(app);
   * const rpc = createRPC(webview);
   * ```
   */
  static registerScheme(): void;

  /**
   * Check if the binary RPC scheme has been registered
   */
  static isSchemeRegistered(): boolean;

  /**
   * Create a new SmartviewRPC builder
   * @param webview - Webview instance to attach RPC to
   */
  constructor(webview: Webview);

  /**
   * Define an exposed RPC function with type metadata
   * @param name - Function name to expose
   * @param schema - Type schema for the function
   * @param handler - Handler function
   * @returns This instance for chaining
   */
  define<TArgs extends any[], TReturn>(
    name: string,
    schema: RPCSchema,
    handler: (...args: TArgs) => TReturn | Promise<TReturn>,
  ): this;

  /**
   * Remove an exposed RPC function
   * @param name - Function name to remove
   * @returns This instance for chaining
   */
  undefine(name: string): this;

  /**
   * Get all defined function names
   * @returns Array of function names
   */
  getDefinedFunctions(): string[];

  /**
   * Generate TypeScript interface definitions for all exposed functions
   * @param options - Generation options
   * @returns TypeScript definition string
   */
  generateTypes(options?: GenerateTypesOptions): string;

  /**
   * Get the underlying webview
   */
  readonly webview: Webview;
}

/**
 * Create a new SmartviewRPC builder for a webview
 * @param webview - Webview instance
 * @returns SmartviewRPC builder
 *
 * @example
 * ```typescript
 * import { Application, Webview, createRPC, Types } from 'saucer-nodejs';
 *
 * const app = Application.init();
 * const webview = new Webview(app);
 * const rpc = createRPC(webview);
 *
 * rpc.define('add', {
 *   params: [Types.param('a', Types.number), Types.param('b', Types.number)],
 *   returns: 'number',
 * }, (a, b) => a + b);
 * ```
 */
export function createRPC(webview: Webview): SmartviewRPC;

// ============================================================================
// Premium Features
// ============================================================================

/**
 * Clipboard API for reading and writing to the system clipboard
 */
export interface ClipboardAPI {
  /**
   * Read text from the clipboard
   * @returns The clipboard text content
   */
  readText(): string;

  /**
   * Write text to the clipboard
   * @param text Text to write
   */
  writeText(text: string): void;

  /**
   * Check if clipboard contains text
   */
  hasText(): boolean;

  /**
   * Check if clipboard contains an image
   */
  hasImage(): boolean;

  /**
   * Clear the clipboard
   */
  clear(): void;
}

export const clipboard: ClipboardAPI;

/**
 * Notification options
 */
export interface NotificationOptions {
  /** Notification title */
  title?: string;
  /** Notification body text */
  body?: string;
  /** Path to notification icon */
  icon?: string;
}

/**
 * System notification class for displaying native notifications
 * 
 * @example
 * ```typescript
 * const notification = new Notification({
 *   title: 'Download Complete',
 *   body: 'Your file has been downloaded!'
 * });
 * notification.show();
 * ```
 */
export class Notification {
  constructor(options?: NotificationOptions);

  /**
   * Show the notification
   */
  show(): void;

  /**
   * Check if notifications are supported on this platform
   */
  static isSupported(): boolean;

  /**
   * Request notification permission
   * @returns Promise resolving to 'granted', 'denied', or 'default'
   */
  static requestPermission(): Promise<'granted' | 'denied' | 'default'>;
}

/**
 * Menu item for system tray
 */
export interface TrayMenuItem {
  /** Menu item label */
  label?: string;
  /** Item type ('separator' for separator line) */
  type?: 'normal' | 'separator';
  /** Click handler (not yet implemented) */
  click?: () => void;
}

/**
 * System tray icon class (menu bar extra on macOS)
 * 
 * @example
 * ```typescript
 * const tray = new SystemTray();
 * tray.setIcon('./icon.png');
 * tray.setTooltip('My App');
 * tray.setMenu([
 *   { label: 'Show Window' },
 *   { type: 'separator' },
 *   { label: 'Quit' }
 * ]);
 * tray.show();
 * ```
 */
export class SystemTray {
  constructor();

  /**
   * Set the tray icon
   * @param pathOrBuffer Path to icon file or Buffer containing icon data
   */
  setIcon(pathOrBuffer: string | Buffer): void;

  /**
   * Set the tray tooltip
   * @param tooltip Tooltip text
   */
  setTooltip(tooltip: string): void;

  /**
   * Set the tray menu
   * @param items Menu items
   */
  setMenu(items: TrayMenuItem[]): void;

  /**
   * Show the tray icon
   */
  show(): void;

  /**
   * Hide the tray icon
   */
  hide(): void;

  /**
   * Destroy the tray icon and clean up resources
   */
  destroy(): void;

  /**
   * Set click handler for tray icon
   * @param callback Function to call when tray is clicked
   */
  onClick(callback: () => void): void;
}

/**
 * Default export
 */
declare const _default: {
  Application: typeof Application;
  Webview: typeof Webview;
  Icon: typeof Icon;
  Stash: typeof Stash;
  Desktop: typeof Desktop;
  PDF: typeof PDF;
  SmartviewRPC: typeof SmartviewRPC;
  Types: typeof Types;
  createRPC: typeof createRPC;
  clipboard: ClipboardAPI;
  Notification: typeof Notification;
  SystemTray: typeof SystemTray;
  native: any;
};
export default _default;
