/**
 * Debug Logging Module for saucer-nodejs
 * 
 * Enable debug logging by setting environment variables:
 *   SAUCER_DEBUG=1           - Enable all debug output
 *   SAUCER_DEBUG=rpc         - Enable only RPC debugging
 *   SAUCER_DEBUG=window      - Enable only window debugging
 *   SAUCER_DEBUG=webview     - Enable only webview debugging
 *   SAUCER_DEBUG=native      - Enable only native module debugging
 *   SAUCER_DEBUG=*           - Enable all categories
 * 
 * Multiple categories: SAUCER_DEBUG=rpc,window
 */

const DEBUG = process.env.SAUCER_DEBUG || '';
const DEBUG_CATEGORIES = DEBUG.split(',').map(s => s.trim().toLowerCase());
const DEBUG_ALL = DEBUG === '1' || DEBUG === '*' || DEBUG === 'true';

// ANSI colors for terminal output
const COLORS = {
    reset: '\x1b[0m',
    dim: '\x1b[2m',
    red: '\x1b[31m',
    green: '\x1b[32m',
    yellow: '\x1b[33m',
    blue: '\x1b[34m',
    magenta: '\x1b[35m',
    cyan: '\x1b[36m',
    gray: '\x1b[90m',
};

// Category colors
const CATEGORY_COLORS = {
    rpc: COLORS.cyan,
    window: COLORS.green,
    webview: COLORS.blue,
    native: COLORS.magenta,
    app: COLORS.yellow,
    event: COLORS.gray,
    error: COLORS.red,
};

/**
 * Check if a category is enabled
 * @param {string} category 
 * @returns {boolean}
 */
function isEnabled(category) {
    if (!DEBUG) return false;
    if (DEBUG_ALL) return true;
    return DEBUG_CATEGORIES.includes(category.toLowerCase());
}

/**
 * Get current timestamp in HH:MM:SS.mmm format
 * @returns {string}
 */
function getTimestamp() {
    const now = new Date();
    const hours = String(now.getHours()).padStart(2, '0');
    const minutes = String(now.getMinutes()).padStart(2, '0');
    const seconds = String(now.getSeconds()).padStart(2, '0');
    const ms = String(now.getMilliseconds()).padStart(3, '0');
    return `${hours}:${minutes}:${seconds}.${ms}`;
}

/**
 * Format a debug message
 * @param {string} category 
 * @param {string} message 
 * @param {any} data 
 * @returns {string}
 */
function formatMessage(category, message, data) {
    const color = CATEGORY_COLORS[category] || COLORS.gray;
    const timestamp = getTimestamp();
    const prefix = `${COLORS.dim}[${timestamp}]${COLORS.reset} ${color}[${category.toUpperCase()}]${COLORS.reset}`;

    let output = `${prefix} ${message}`;

    if (data !== undefined) {
        if (typeof data === 'object') {
            output += '\n' + JSON.stringify(data, null, 2).split('\n').map(l => '         ' + l).join('\n');
        } else {
            output += ` ${COLORS.dim}${data}${COLORS.reset}`;
        }
    }

    return output;
}

/**
 * Create a debug logger for a specific category
 * @param {string} category 
 * @returns {function}
 */
export function createDebugger(category) {
    const enabled = isEnabled(category);

    return function debug(message, data) {
        if (!enabled) return;
        console.log(formatMessage(category, message, data));
    };
}

// Pre-configured debuggers for common categories
export const debugRPC = createDebugger('rpc');
export const debugWindow = createDebugger('window');
export const debugWebview = createDebugger('webview');
export const debugNative = createDebugger('native');
export const debugApp = createDebugger('app');
export const debugEvent = createDebugger('event');

/**
 * Log an error (always shown)
 * @param {string} message 
 * @param {Error} error 
 */
export function logError(message, error) {
    const timestamp = getTimestamp();
    console.error(`${COLORS.dim}[${timestamp}]${COLORS.reset} ${COLORS.red}[ERROR]${COLORS.reset} ${message}`);
    if (error) {
        console.error(`         ${COLORS.dim}${error.stack || error.message || error}${COLORS.reset}`);
    }
}

/**
 * Log a warning
 * @param {string} message 
 */
export function logWarn(message) {
    const timestamp = getTimestamp();
    console.warn(`${COLORS.dim}[${timestamp}]${COLORS.reset} ${COLORS.yellow}[WARN]${COLORS.reset} ${message}`);
}

/**
 * Measure execution time of an async function
 * @param {string} label 
 * @param {function} fn 
 * @returns {Promise<any>}
 */
export async function measure(label, fn) {
    const debug = createDebugger('perf');
    const start = performance.now();

    try {
        const result = await fn();
        const duration = (performance.now() - start).toFixed(2);
        debug(`${label} completed in ${duration}ms`);
        return result;
    } catch (error) {
        const duration = (performance.now() - start).toFixed(2);
        debug(`${label} failed after ${duration}ms`, error.message);
        throw error;
    }
}

/**
 * Check if debug mode is enabled
 * @returns {boolean}
 */
export function isDebugEnabled() {
    return !!DEBUG;
}

/**
 * Get enabled debug categories
 * @returns {string[]}
 */
export function getEnabledCategories() {
    if (DEBUG_ALL) return ['all'];
    return DEBUG_CATEGORIES;
}

export default {
    createDebugger,
    debugRPC,
    debugWindow,
    debugWebview,
    debugNative,
    debugApp,
    debugEvent,
    logError,
    logWarn,
    measure,
    isDebugEnabled,
    getEnabledCategories,
};
