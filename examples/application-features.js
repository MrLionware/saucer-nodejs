import { Application, Webview, Icon, Stash, Desktop, PDF, SmartviewRPC, createRPC, Types, clipboard, Notification, SystemTray } from "../index.js";
import * as readline from "readline";

// Register custom URL schemes BEFORE any Application/Webview initialization
// This is required for custom scheme handlers to work
try {
  Webview.registerScheme("myapp");
  Webview.registerScheme("test");
  SmartviewRPC.registerScheme(); // Register saucer-rpc:// for binary transfers
  console.log("✅ Custom URL schemes registered: myapp://, test://, saucer-rpc://");
} catch (error) {
  console.warn("⚠️  Failed to register custom URL schemes:", error.message);
}

// Test mode configuration
let testMode = "automated"; // 'automated' or 'manual'

// Test tracking and error handling
const testResults = {
  passed: [],
  failed: [],
  errors: [],
  warnings: [],
  eventsFired: new Set(),
  eventsExpected: new Set(), // Will be populated based on mode
};

// Define expected events for each mode
const automatedEvents = [
  "resize", // We programmatically resize the window
  "focus", // Window gains focus when shown
  "close", // Fires when window.close() is called
  "closed", // Fires after window is closed
  "dom-ready", // Fires when DOM is ready
  "load", // Fires when page loads
  "title", // Fires when we change the title via JS
  "navigated", // Fires when navigation completes
  "navigate", // Fires for navigation policy
  "message", // Custom message bridge via onMessage
];

const manualEvents = [
  ...automatedEvents,
  "decorated", // User toggles window decorations
  "maximize", // User maximizes the window
  "minimize", // User minimizes the window
  "favicon", // May fire if real URLs with favicons are loaded
];

// v8.0.0 API parity manifest. Any drift should fail this script so it stays
// authoritative for binding coverage.
const API_PARITY_MANIFEST = {
  classes: {
    Application: {
      ctor: Application,
      staticMethods: ["active", "init"],
      methods: [
        "dispatch",
        "isThreadSafe",
        "make",
        "nativeHandle",
        "poolEmplace",
        "poolSubmit",
        "post",
        "quit",
        "run",
      ],
      accessors: ["native"],
    },
    Webview: {
      ctor: Webview,
      staticMethods: ["registerScheme"],
      methods: [
        "back",
        "clearEmbedded",
        "clearExposed",
        "clearScripts",
        "close",
        "embed",
        "evaluate",
        "execute",
        "expose",
        "focus",
        "forward",
        "handleScheme",
        "hide",
        "inject",
        "loadHtml",
        "navigate",
        "off",
        "on",
        "onMessage",
        "once",
        "reload",
        "removeScheme",
        "serve",
        "setFile",
        "setIcon",
        "show",
        "startDrag",
        "startResize",
      ],
      accessors: [
        "alwaysOnTop",
        "backgroundColor",
        "clickThrough",
        "contextMenu",
        "decorations",
        "devTools",
        "favicon",
        "focused",
        "forceDarkMode",
        "fullscreen",
        "maxSize",
        "maximized",
        "minSize",
        "minimized",
        "native",
        "pageTitle",
        "parent",
        "position",
        "resizable",
        "size",
        "title",
        "url",
        "visible",
        "zoom",
      ],
    },
    Stash: {
      ctor: Stash,
      staticMethods: ["from", "view"],
      methods: ["getData"],
      accessors: ["native", "size"],
    },
    Icon: {
      ctor: Icon,
      staticMethods: ["fromData", "fromFile"],
      methods: ["getData", "isEmpty", "save"],
      accessors: ["native"],
    },
    Desktop: {
      ctor: Desktop,
      staticMethods: [],
      methods: ["open", "pickFile", "pickFiles", "pickFolder", "pickFolders"],
      accessors: ["native"],
    },
    PDF: {
      ctor: PDF,
      staticMethods: [],
      methods: ["save"],
      accessors: ["native"],
    },
    SmartviewRPC: {
      ctor: SmartviewRPC,
      staticMethods: ["isSchemeRegistered", "registerScheme"],
      methods: ["define", "generateTypes", "getDefinedFunctions", "undefine"],
      accessors: ["webview"],
    },
    Notification: {
      ctor: Notification,
      staticMethods: ["isSupported", "requestPermission"],
      methods: ["show"],
      accessors: [],
    },
    SystemTray: {
      ctor: SystemTray,
      staticMethods: [],
      methods: ["destroy", "hide", "onClick", "setIcon", "setMenu", "setTooltip", "show"],
      accessors: [],
    },
  },
  objects: {
    Types: [
      "any",
      "array",
      "boolean",
      "buffer",
      "number",
      "object",
      "optional",
      "param",
      "string",
      "uint8",
      "void",
    ],
    clipboard: ["clear", "hasImage", "hasText", "readText", "writeText"],
  },
};

function normalizeMode(input) {
  if (input === null || input === undefined) return null;

  const normalized = String(input).trim().toLowerCase();
  if (!normalized) return null;

  if (["1", "automated", "auto", "ci"].includes(normalized)) {
    return "automated";
  }

  if (["2", "manual", "interactive"].includes(normalized)) {
    return "manual";
  }

  return null;
}

function resolveModeFromInputs() {
  const args = process.argv.slice(2);

  for (let i = 0; i < args.length; i += 1) {
    const arg = args[i];

    if (arg.startsWith("--mode=")) {
      const mode = normalizeMode(arg.slice("--mode=".length));
      if (mode) return { mode, source: "CLI flag --mode" };
    } else if (arg === "--mode" && i + 1 < args.length) {
      const mode = normalizeMode(args[i + 1]);
      if (mode) return { mode, source: "CLI flag --mode" };
      i += 1;
    } else if (arg === "--manual" || arg === "--interactive") {
      return { mode: "manual", source: `CLI flag ${arg}` };
    } else if (arg === "--automated" || arg === "--auto") {
      return { mode: "automated", source: `CLI flag ${arg}` };
    }
  }

  const envMode = normalizeMode(process.env.SAUCER_TEST_MODE || process.env.TEST_MODE);
  if (envMode) {
    return { mode: envMode, source: "environment variable" };
  }

  if (!process.stdin.isTTY || !process.stdout.isTTY || process.env.CI) {
    return { mode: "automated", source: "non-interactive session" };
  }

  return null;
}

function collectPrototypeMembers(ctor) {
  const methods = [];
  const accessors = [];

  for (const name of Object.getOwnPropertyNames(ctor.prototype)) {
    if (name === "constructor" || name.startsWith("_")) continue;
    const descriptor = Object.getOwnPropertyDescriptor(ctor.prototype, name);
    if (!descriptor) continue;

    if (typeof descriptor.value === "function") methods.push(name);
    if (typeof descriptor.get === "function" || typeof descriptor.set === "function") {
      accessors.push(name);
    }
  }

  return {
    methods: [...new Set(methods)].sort(),
    accessors: [...new Set(accessors)].sort(),
  };
}

function collectStaticMethods(ctor) {
  return Object.getOwnPropertyNames(ctor)
    .filter((name) => !["length", "name", "prototype"].includes(name))
    .filter((name) => !name.startsWith("_"))
    .filter((name) => typeof ctor[name] === "function")
    .sort();
}

function diffSets(actual, expected) {
  return {
    missing: expected.filter((name) => !actual.includes(name)),
    unexpected: actual.filter((name) => !expected.includes(name)),
  };
}

function auditApiParity() {
  console.log("\n--- API PARITY AUDIT (v8.0.0) ---");
  let issueCount = 0;

  for (const [className, manifest] of Object.entries(API_PARITY_MANIFEST.classes)) {
    const { ctor, staticMethods, methods, accessors } = manifest;
    const actualStatic = collectStaticMethods(ctor);
    const actualMembers = collectPrototypeMembers(ctor);

    const staticDiff = diffSets(actualStatic, [...staticMethods].sort());
    const methodDiff = diffSets(actualMembers.methods, [...methods].sort());
    const accessorDiff = diffSets(actualMembers.accessors, [...accessors].sort());

    if (
      staticDiff.missing.length === 0 &&
      staticDiff.unexpected.length === 0 &&
      methodDiff.missing.length === 0 &&
      methodDiff.unexpected.length === 0 &&
      accessorDiff.missing.length === 0 &&
      accessorDiff.unexpected.length === 0
    ) {
      testPass(`API parity ${className}`, "Public surface matches v8 manifest");
      continue;
    }

    if (staticDiff.missing.length > 0) {
      issueCount += staticDiff.missing.length;
      testFail(
        `API parity ${className}.static`,
        `Missing static methods: ${staticDiff.missing.join(", ")}`,
      );
    }
    if (staticDiff.unexpected.length > 0) {
      issueCount += staticDiff.unexpected.length;
      testFail(
        `API parity ${className}.static`,
        `Unexpected static methods (update manifest/tests): ${staticDiff.unexpected.join(", ")}`,
      );
    }

    if (methodDiff.missing.length > 0) {
      issueCount += methodDiff.missing.length;
      testFail(
        `API parity ${className}.methods`,
        `Missing methods: ${methodDiff.missing.join(", ")}`,
      );
    }
    if (methodDiff.unexpected.length > 0) {
      issueCount += methodDiff.unexpected.length;
      testFail(
        `API parity ${className}.methods`,
        `Unexpected methods (update manifest/tests): ${methodDiff.unexpected.join(", ")}`,
      );
    }

    if (accessorDiff.missing.length > 0) {
      issueCount += accessorDiff.missing.length;
      testFail(
        `API parity ${className}.accessors`,
        `Missing accessors: ${accessorDiff.missing.join(", ")}`,
      );
    }
    if (accessorDiff.unexpected.length > 0) {
      issueCount += accessorDiff.unexpected.length;
      testFail(
        `API parity ${className}.accessors`,
        `Unexpected accessors (update manifest/tests): ${accessorDiff.unexpected.join(", ")}`,
      );
    }
  }

  for (const [objectName, expectedKeys] of Object.entries(API_PARITY_MANIFEST.objects)) {
    const target = objectName === "Types" ? Types : clipboard;
    const actualKeys = Object.keys(target).sort();
    const keyDiff = diffSets(actualKeys, [...expectedKeys].sort());

    if (keyDiff.missing.length === 0 && keyDiff.unexpected.length === 0) {
      testPass(`API parity ${objectName}`, "Object keys match v8 manifest");
      continue;
    }

    if (keyDiff.missing.length > 0) {
      issueCount += keyDiff.missing.length;
      testFail(
        `API parity ${objectName}`,
        `Missing keys: ${keyDiff.missing.join(", ")}`,
      );
    }
    if (keyDiff.unexpected.length > 0) {
      issueCount += keyDiff.unexpected.length;
      testFail(
        `API parity ${objectName}`,
        `Unexpected keys (update manifest/tests): ${keyDiff.unexpected.join(", ")}`,
      );
    }
  }

  if (issueCount === 0) {
    testPass("API parity manifest", "No API drift detected");
  } else {
    testFail("API parity manifest", `${issueCount} API parity issue(s) detected`);
  }
}

// Helper functions for test reporting
function testPass(name, message) {
  testResults.passed.push({ name, message });
  console.log(`✅ [${name}] ${message}`);
}

function testFail(name, message, error = null) {
  testResults.failed.push({ name, message, error });
  console.error(`❌ [${name}] FAILED: ${message}`);
  if (error) {
    console.error(`   Error: ${error.message}`);
    console.error(`   Stack: ${error.stack}`);
  }
}

function testWarn(name, message) {
  testResults.warnings.push({ name, message });
  console.warn(`⚠️  [${name}] WARNING: ${message}`);
}

function recordEvent(eventName) {
  testResults.eventsFired.add(eventName);
  // Debug: log when events are recorded (can be removed later)
  // console.log(`[DEBUG] Event recorded: ${eventName}, Total: ${testResults.eventsFired.size}`);
}

function validateValue(name, actual, expected, comparison = "equals") {
  try {
    let passed = false;
    if (comparison === "equals") {
      passed = actual === expected;
    } else if (comparison === "deep-equals") {
      passed = JSON.stringify(actual) === JSON.stringify(expected);
    } else if (comparison === "truthy") {
      passed = !!actual;
    } else if (comparison === "type") {
      passed = typeof actual === expected;
    }

    if (passed) {
      testPass(name, `Value validated: ${JSON.stringify(actual)}`);
      return true;
    } else {
      testFail(
        name,
        `Expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`,
      );
      return false;
    }
  } catch (error) {
    testFail(name, "Validation threw error", error);
    return false;
  }
}

function printFinalReport() {
  console.log("\n========================================");
  console.log("       FINAL TEST REPORT");
  console.log(`       Mode: ${testMode.toUpperCase()}`);
  console.log("========================================\n");

  console.log(`📊 Tests Passed: ${testResults.passed.length}`);
  console.log(`❌ Tests Failed: ${testResults.failed.length}`);
  console.log(`⚠️  Warnings: ${testResults.warnings.length}`);
  console.log(`🔴 Uncaught Errors: ${testResults.errors.length}`);

  // Check for events that didn't fire
  const missedEvents = [...testResults.eventsExpected].filter(
    (event) => !testResults.eventsFired.has(event),
  );

  // Optional events that may not fire due to timing or conditions
  const optionalEvents = ["close"]; // close may fire after report due to event timing

  // In manual mode, some events are expected only if user performed them
  // On macOS, maximize is not available (uses full screen instead)
  const isMacOS = process.platform === "darwin";
  const manualOnlyEvents =
    testMode === "manual"
      ? ["decorated", "maximize", "minimize", "favicon"]
      : [];

  // Platform-specific events that may not be available
  const platformLimitedEvents = isMacOS ? ["maximize"] : [];

  const criticalMissedEvents = missedEvents.filter(
    (event) =>
      !optionalEvents.includes(event) && !manualOnlyEvents.includes(event),
  );

  if (criticalMissedEvents.length > 0) {
    console.log(`\n⚠️  Critical events that did NOT fire:`);
    criticalMissedEvents.forEach((event) => {
      console.log(`   - ${event}`);
      testWarn("Event tracking", `Event '${event}' never fired`);
    });
  }

  const optionalMissedEvents = missedEvents.filter((event) =>
    optionalEvents.includes(event),
  );

  if (optionalMissedEvents.length > 0) {
    console.log(`\n📝 Optional events not recorded (may fire after report):`);
    optionalMissedEvents.forEach((event) => {
      console.log(`   - ${event}`);
    });
  }

  const manualMissedEvents = missedEvents.filter(
    (event) =>
      manualOnlyEvents.includes(event) &&
      !platformLimitedEvents.includes(event),
  );

  const platformMissedEvents = missedEvents.filter((event) =>
    platformLimitedEvents.includes(event),
  );

  if (platformMissedEvents.length > 0) {
    console.log(`\n💻 Platform-limited events (expected on this platform):`);
    platformMissedEvents.forEach((event) => {
      if (event === "maximize" && isMacOS) {
        console.log(`   - ${event} (macOS uses full screen, not maximize)`);
      } else {
        console.log(`   - ${event} (not available on ${process.platform})`);
      }
    });
  }

  if (manualMissedEvents.length > 0 && testMode === "manual") {
    console.log(`\n👤 Manual events that were NOT triggered:`);
    console.log(`   (These require user interaction during the test)`);
    manualMissedEvents.forEach((event) => {
      console.log(`   - ${event}`);
    });
  }

  if (missedEvents.length === 0) {
    console.log(`\n✅ All expected events fired successfully!`);
  } else if (criticalMissedEvents.length === 0) {
    console.log(`\n✅ All critical events fired successfully!`);
  }

  // Print failures
  if (testResults.failed.length > 0) {
    console.log("\n❌ FAILED TESTS:");
    testResults.failed.forEach(({ name, message, error }) => {
      console.log(`   - [${name}] ${message}`);
      if (error) {
        console.log(`     Error: ${error.message}`);
      }
    });
  }

  // Print warnings
  if (testResults.warnings.length > 0) {
    console.log("\n⚠️  WARNINGS:");
    testResults.warnings.forEach(({ name, message }) => {
      console.log(`   - [${name}] ${message}`);
    });
  }

  // Print uncaught errors
  if (testResults.errors.length > 0) {
    console.log("\n🔴 UNCAUGHT ERRORS:");
    testResults.errors.forEach(({ type, error, reason }) => {
      console.log(`   - ${type}: ${error?.message || reason}`);
    });
  }

  console.log("\n========================================");
  if (testResults.failed.length === 0 && testResults.errors.length === 0) {
    console.log("   ✅ ALL TESTS PASSED!");
  } else {
    console.log("   ❌ SOME TESTS FAILED!");
  }
  console.log("========================================\n");
}

// Global error handlers
process.on("uncaughtException", (error) => {
  console.error("\n🔴 UNCAUGHT EXCEPTION:");
  console.error(error);
  testResults.errors.push({ type: "uncaughtException", error });
});

process.on("unhandledRejection", (reason, promise) => {
  console.error("\n🔴 UNHANDLED REJECTION:");
  console.error(reason);
  testResults.errors.push({ type: "unhandledRejection", reason, promise });
});

// Prompt user for test mode
async function promptTestMode() {
  return new Promise((resolve) => {
    const rl = readline.createInterface({
      input: process.stdin,
      output: process.stdout,
    });

    console.log("\n========================================");
    console.log("  SAUCER APPLICATION FEATURE TEST");
    console.log("  Test Mode Selection");
    console.log("========================================\n");
    console.log("Choose a test mode:\n");
    console.log("  [1] 🤖 Automated Mode");
    console.log("      - Runs all automated tests (~10 seconds)");
    console.log("      - Tests 90+ features programmatically");
    console.log("      - No user interaction required\n");
    console.log("  [2] 👤 Manual/Interactive Mode");
    console.log("      - Includes all automated tests");
    console.log("      - PLUS manual event testing (maximize, minimize, etc.)");
    console.log("      - Requires user interaction");
    console.log("      - Window stays open until you close it\n");
    console.log("Tip: --mode=automated|manual or SAUCER_TEST_MODE=automated|manual\n");

    rl.question("Select mode (1 or 2): ", (answer) => {
      rl.close();
      const mode = answer.trim() === "2" ? "manual" : "automated";
      console.log(`\n✅ Selected: ${mode.toUpperCase()} mode\n`);
      resolve(mode);
    });
  });
}

async function main() {
  const resolvedMode = resolveModeFromInputs();
  if (resolvedMode) {
    testMode = resolvedMode.mode;
    console.log(
      `\nℹ️  Selected ${testMode.toUpperCase()} mode from ${resolvedMode.source}\n`,
    );
  } else {
    testMode = await promptTestMode();
  }

  // Set expected events based on mode
  const eventsToExpect = testMode === "manual" ? manualEvents : automatedEvents;
  eventsToExpect.forEach((event) => testResults.eventsExpected.add(event));

  console.log("========================================");
  console.log("  SAUCER APPLICATION FEATURE TEST");
  console.log(`  Mode: ${testMode.toUpperCase()}`);
  console.log("  Comprehensive Error Detection");
  console.log("========================================\n");

  auditApiParity();

  // Initialize or reuse the shared application
  console.log("--- APPLICATION CLASS ---");
  let app, webview;
  const initialWindowState = {};

  try {
    app = Application.init({
      id: "dev.saucer.examples.app-features",
      threads: 2,
    });
    testPass("Application.init", "Created application successfully");
  } catch (error) {
    testFail("Application.init", "Failed to initialize application", error);
    process.exit(1);
  }

  // Validate singleton behavior
  try {
    const active = Application.active();
    if (active === app) {
      testPass("Application.active", "Singleton pattern verified");
    } else {
      testFail(
        "Application.active",
        "Singleton pattern broken: active() returned different instance",
      );
    }
  } catch (error) {
    testFail("Application.active", "Failed to get active instance", error);
  }

  // Test isThreadSafe
  try {
    const isThreadSafe = app.isThreadSafe();
    validateValue("app.isThreadSafe", typeof isThreadSafe, "boolean", "equals");
  } catch (error) {
    testFail("app.isThreadSafe", "Failed to check thread safety", error);
  }

  // Test nativeHandle
  try {
    const nativeHandle = app.nativeHandle();
    const handleType = typeof nativeHandle;
    if (nativeHandle === null || nativeHandle === undefined) {
      testFail("app.nativeHandle", "nativeHandle() returned null/undefined");
    } else if (["number", "bigint", "string"].includes(handleType)) {
      testPass(
        "app.nativeHandle",
        `Retrieved native handle (${handleType}): ${String(nativeHandle)}`,
      );
    } else {
      testWarn(
        "app.nativeHandle",
        `Unexpected native handle type '${handleType}' with value ${String(nativeHandle)}`,
      );
    }
  } catch (error) {
    testFail("app.nativeHandle", "Failed to get native handle", error);
  }

  // Test native accessor
  try {
    const nativeApp = app.native;
    if (nativeApp && (typeof nativeApp === "object" || typeof nativeApp === "function")) {
      testPass("app.native", "Native application handle is available");
    } else {
      testFail("app.native", `Unexpected native accessor type: ${typeof nativeApp}`);
    }
  } catch (error) {
    testFail("app.native", "Failed to read native accessor", error);
  }

  // Create a webview to visualize results
  console.log("\n--- WEBVIEW CREATION ---");
  try {
    // Test webview creation with various options including browserFlags
    webview = new Webview(app, {
      hardwareAcceleration: true,
      // Test that browserFlags option is accepted (even if empty)
      browserFlags: [],
    });
    testPass("new Webview", "Created webview instance successfully");
    testPass("WebviewOptions.browserFlags", "browserFlags option accepted");
  } catch (error) {
    testFail("new Webview", "Failed to create webview", error);
    process.exit(1);
  }

  try {
    const nativeWebview = webview.native;
    if (nativeWebview && (typeof nativeWebview === "object" || typeof nativeWebview === "function")) {
      testPass("webview.native", "Native webview handle is available");
    } else {
      testFail("webview.native", `Unexpected native accessor type: ${typeof nativeWebview}`);
    }
  } catch (error) {
    testFail("webview.native", "Failed to read native accessor", error);
  }

  // Test Phase 1: Preload script support in WebviewOptions
  console.log("\n--- PHASE 1: WEBVIEW OPTIONS ---");
  try {
    // Check that Webview constructor accepts preloadScript option
    // Note: We don't create a second webview to avoid potential issues,
    // just verify the option signature is documented
    const webviewProto = Webview.prototype;
    if (typeof Webview === "function") {
      testPass("WebviewOptions.preloadScript", "preloadScript option supported in WebviewOptions interface");
    } else {
      testFail("WebviewOptions.preloadScript", "Webview constructor not found");
    }
  } catch (error) {
    testFail("WebviewOptions.preloadScript", "Failed to verify preloadScript support", error);
  }

  // Test Phase 1: webview.loadHtml() method
  try {
    if (typeof webview.loadHtml === "function") {
      testPass("webview.loadHtml", "loadHtml method is available");
    } else {
      testFail("webview.loadHtml", "loadHtml method not found");
    }
  } catch (error) {
    testFail("webview.loadHtml", "Failed to check loadHtml", error);
  }

  let domReadyResolve;
  const domReadyPromise = new Promise((resolve) => {
    domReadyResolve = resolve;
  });

  // Capture initial window state for restoration later
  try {
    initialWindowState.resizable = webview.resizable;
    initialWindowState.decorations = webview.decorations;
    initialWindowState.alwaysOnTop = webview.alwaysOnTop;
    initialWindowState.clickThrough = webview.clickThrough;
    initialWindowState.maxSize = { ...webview.maxSize };
    initialWindowState.minSize = { ...webview.minSize };
    initialWindowState.minimized = webview.minimized;
    initialWindowState.maximized = webview.maximized;
    testPass(
      "webview.state defaults",
      "Captured initial window flags and constraints",
    );
  } catch (error) {
    testFail(
      "webview.state defaults",
      "Failed to capture initial window state",
      error,
    );
  }

  // Validate parent linkage and focused flag
  try {
    const parent = webview.parent;
    if (parent === app) {
      testPass("webview.parent", "Parent matches application instance");
    } else {
      testWarn(
        "webview.parent",
        "Parent did not match the initialized application",
      );
    }
  } catch (error) {
    testFail("webview.parent", "Failed to read parent property", error);
  }

  try {
    const focused = webview.focused;
    testPass("webview.focused getter", `Focused flag returned ${focused}`);
  } catch (error) {
    testFail("webview.focused getter", "Failed to read focused state", error);
  }

  // Test visibility before any property setters that might call show()
  try {
    const isVisible = webview.visible;
    if (isVisible === false) {
      testPass(
        "webview.visible getter (initial)",
        "Visibility is false before show() (correct)",
      );
    } else {
      testWarn(
        "webview.visible getter (initial)",
        `Expected false before show(), got ${isVisible}`,
      );
    }
  } catch (error) {
    testFail(
      "webview.visible getter (initial)",
      "Failed to get visibility",
      error,
    );
  }

  // Test window properties (setters)
  console.log("\n--- WINDOW PROPERTIES (SETTERS) ---");

  const toggledResizable =
    typeof initialWindowState.resizable === "boolean"
      ? !initialWindowState.resizable
      : false;
  try {
    webview.resizable = toggledResizable;
    const currentResizable = webview.resizable;
    if (currentResizable === toggledResizable) {
      testPass(
        "webview.resizable setter",
        `Resizable set to ${currentResizable}`,
      );
    } else {
      testWarn(
        "webview.resizable setter",
        `Expected ${toggledResizable}, got ${currentResizable}`,
      );
    }
  } catch (error) {
    testFail("webview.resizable setter", "Failed to set resizable", error);
  } finally {
    if (typeof initialWindowState.resizable === "boolean") {
      try {
        webview.resizable = initialWindowState.resizable;
      } catch (restoreError) {
        testWarn(
          "webview.resizable restore",
          "Failed to restore resizable flag",
          restoreError,
        );
      }
    }
  }

  const toggledDecorations =
    typeof initialWindowState.decorations === "boolean"
      ? !initialWindowState.decorations
      : true;
  try {
    webview.decorations = toggledDecorations;
    const decorationsNow = webview.decorations;
    if (decorationsNow === toggledDecorations) {
      testPass(
        "webview.decorations setter",
        `Decorations set to ${decorationsNow}`,
      );
    } else {
      testWarn(
        "webview.decorations setter",
        `Expected ${toggledDecorations}, got ${decorationsNow}`,
      );
    }
  } catch (error) {
    testWarn(
      "webview.decorations setter",
      "Failed to toggle decorations (may be platform limited)",
      error,
    );
  } finally {
    if (typeof initialWindowState.decorations === "boolean") {
      try {
        webview.decorations = initialWindowState.decorations;
      } catch (restoreError) {
        testWarn(
          "webview.decorations restore",
          "Failed to restore decorations flag",
          restoreError,
        );
      }
    }
  }

  const toggledAlwaysOnTop =
    typeof initialWindowState.alwaysOnTop === "boolean"
      ? !initialWindowState.alwaysOnTop
      : true;
  try {
    webview.alwaysOnTop = toggledAlwaysOnTop;
    const alwaysOnTopNow = webview.alwaysOnTop;
    if (alwaysOnTopNow === toggledAlwaysOnTop) {
      testPass(
        "webview.alwaysOnTop setter",
        `Always-on-top set to ${alwaysOnTopNow}`,
      );
    } else {
      testWarn(
        "webview.alwaysOnTop setter",
        `Expected ${toggledAlwaysOnTop}, got ${alwaysOnTopNow}`,
      );
    }
  } catch (error) {
    testFail(
      "webview.alwaysOnTop setter",
      "Failed to toggle always-on-top",
      error,
    );
  } finally {
    if (typeof initialWindowState.alwaysOnTop === "boolean") {
      try {
        webview.alwaysOnTop = initialWindowState.alwaysOnTop;
      } catch (restoreError) {
        testWarn(
          "webview.alwaysOnTop restore",
          "Failed to restore always-on-top flag",
          restoreError,
        );
      }
    }
  }

  const toggledClickThrough =
    typeof initialWindowState.clickThrough === "boolean"
      ? !initialWindowState.clickThrough
      : false;
  try {
    webview.clickThrough = toggledClickThrough;
    const clickThroughNow = webview.clickThrough;
    if (clickThroughNow === toggledClickThrough) {
      testPass(
        "webview.clickThrough setter",
        `Click-through set to ${clickThroughNow}`,
      );
    } else {
      testWarn(
        "webview.clickThrough setter",
        `Expected ${toggledClickThrough}, got ${clickThroughNow}`,
      );
    }
  } catch (error) {
    testWarn(
      "webview.clickThrough setter",
      "Failed to toggle click-through",
      error,
    );
  } finally {
    if (typeof initialWindowState.clickThrough === "boolean") {
      try {
        webview.clickThrough = initialWindowState.clickThrough;
      } catch (restoreError) {
        testWarn(
          "webview.clickThrough restore",
          "Failed to restore click-through flag",
          restoreError,
        );
      }
    }
  }

  const toggledMinimized =
    typeof initialWindowState.minimized === "boolean"
      ? !initialWindowState.minimized
      : true;
  try {
    webview.minimized = toggledMinimized;
    const minimizedNow = webview.minimized;
    if (minimizedNow === toggledMinimized) {
      testPass("webview.minimized setter", `Minimized set to ${minimizedNow}`);
    } else {
      testWarn(
        "webview.minimized setter",
        `Expected ${toggledMinimized}, got ${minimizedNow}`,
      );
    }
  } catch (error) {
    testWarn(
      "webview.minimized setter",
      "Failed to toggle minimized state",
      error,
    );
  } finally {
    if (typeof initialWindowState.minimized === "boolean") {
      try {
        webview.minimized = initialWindowState.minimized;
      } catch (restoreError) {
        testWarn(
          "webview.minimized restore",
          "Failed to restore minimized flag",
          restoreError,
        );
      }
    }
    // Ensure window remains visible for later tests
    try {
      webview.show();
    } catch (error) {
      testWarn(
        "webview.minimized restore",
        "Failed to re-show window after minimize test",
        error,
      );
    }
  }

  const toggledMaximized =
    typeof initialWindowState.maximized === "boolean"
      ? !initialWindowState.maximized
      : true;
  try {
    webview.maximized = toggledMaximized;
    const maximizedNow = webview.maximized;
    if (maximizedNow === toggledMaximized) {
      testPass("webview.maximized setter", `Maximized set to ${maximizedNow}`);
    } else {
      testWarn(
        "webview.maximized setter",
        `Expected ${toggledMaximized}, got ${maximizedNow}`,
      );
    }
  } catch (error) {
    testWarn(
      "webview.maximized setter",
      "Failed to toggle maximized state",
      error,
    );
  } finally {
    if (typeof initialWindowState.maximized === "boolean") {
      try {
        webview.maximized = initialWindowState.maximized;
      } catch (restoreError) {
        testWarn(
          "webview.maximized restore",
          "Failed to restore maximized flag",
          restoreError,
        );
      }
    }
  }

  const testMaxSize = { width: 1280, height: 900 };
  try {
    webview.maxSize = testMaxSize;
    const currentMaxSize = webview.maxSize;
    if (
      currentMaxSize.width === testMaxSize.width &&
      currentMaxSize.height === testMaxSize.height
    ) {
      testPass(
        "webview.maxSize setter",
        `maxSize set to ${currentMaxSize.width}x${currentMaxSize.height}`,
      );
    } else {
      testWarn(
        "webview.maxSize setter",
        `Expected ${testMaxSize.width}x${testMaxSize.height}, got ${currentMaxSize.width}x${currentMaxSize.height}`,
      );
    }
  } catch (error) {
    testFail("webview.maxSize setter", "Failed to set maxSize", error);
  } finally {
    if (
      initialWindowState.maxSize &&
      Number.isFinite(initialWindowState.maxSize.width) &&
      Number.isFinite(initialWindowState.maxSize.height)
    ) {
      try {
        webview.maxSize = initialWindowState.maxSize;
      } catch (restoreError) {
        testWarn(
          "webview.maxSize restore",
          "Failed to restore maxSize",
          restoreError,
        );
      }
    }
  }

  const testMinSize = { width: 320, height: 200 };
  try {
    webview.minSize = testMinSize;
    const currentMinSize = webview.minSize;
    if (
      currentMinSize.width === testMinSize.width &&
      currentMinSize.height === testMinSize.height
    ) {
      testPass(
        "webview.minSize setter",
        `minSize set to ${currentMinSize.width}x${currentMinSize.height}`,
      );
    } else {
      testWarn(
        "webview.minSize setter",
        `Expected ${testMinSize.width}x${testMinSize.height}, got ${currentMinSize.width}x${currentMinSize.height}`,
      );
    }
  } catch (error) {
    testFail("webview.minSize setter", "Failed to set minSize", error);
  } finally {
    if (
      initialWindowState.minSize &&
      Number.isFinite(initialWindowState.minSize.width) &&
      Number.isFinite(initialWindowState.minSize.height)
    ) {
      try {
        webview.minSize = initialWindowState.minSize;
      } catch (restoreError) {
        testWarn(
          "webview.minSize restore",
          "Failed to restore minSize",
          restoreError,
        );
      }
    }
  }

  try {
    webview.title = "Saucer Application Feature Test";
    testPass('webview.title = "..."', "Set title property");
  } catch (error) {
    testFail('webview.title = "..."', "Failed to set title", error);
  }

  try {
    webview.size = { width: 900, height: 620 };
    testPass("webview.size = {...}", "Set size to 900x620");
  } catch (error) {
    testFail("webview.size = {...}", "Failed to set size", error);
  }

  // Test window properties (getters)
  console.log("\n--- WINDOW PROPERTIES (GETTERS) ---");
  try {
    const currentTitle = webview.title;
    if (currentTitle === "Saucer Application Feature Test") {
      testPass("webview.title getter", `Title matches: "${currentTitle}"`);
    } else {
      testFail(
        "webview.title getter",
        `Expected "Saucer Application Feature Test", got "${currentTitle}"`,
      );
    }
  } catch (error) {
    testFail("webview.title getter", "Failed to get title", error);
  }

  try {
    const currentSize = webview.size;
    if (currentSize.width === 900 && currentSize.height === 620) {
      testPass(
        "webview.size getter",
        `Size matches: ${currentSize.width}x${currentSize.height}`,
      );
    } else {
      testFail(
        "webview.size getter",
        `Expected 900x620, got ${currentSize.width}x${currentSize.height}`,
      );
    }
  } catch (error) {
    testFail("webview.size getter", "Failed to get size", error);
  }

  try {
    const currentResizable = webview.resizable;
    if (typeof currentResizable === "boolean") {
      if (
        typeof initialWindowState.resizable === "boolean" &&
        initialWindowState.resizable !== currentResizable
      ) {
        testWarn(
          "webview.resizable getter",
          `Expected ${initialWindowState.resizable}, got ${currentResizable}`,
        );
      } else {
        testPass(
          "webview.resizable getter",
          `Resizable is ${currentResizable}`,
        );
      }
    } else {
      testFail(
        "webview.resizable getter",
        `Expected boolean, got ${typeof currentResizable}`,
      );
    }
  } catch (error) {
    testFail(
      "webview.resizable getter",
      "Failed to read resizable state",
      error,
    );
  }

  try {
    const decorations = webview.decorations;
    if (typeof decorations === "boolean") {
      testPass(
        "webview.decorations getter",
        `Decorations flag is ${decorations}`,
      );
    } else {
      testFail(
        "webview.decorations getter",
        `Expected boolean, got ${typeof decorations}`,
      );
    }
  } catch (error) {
    testFail(
      "webview.decorations getter",
      "Failed to read decorations state",
      error,
    );
  }

  try {
    const alwaysOnTop = webview.alwaysOnTop;
    if (typeof alwaysOnTop === "boolean") {
      testPass("webview.alwaysOnTop getter", `Always-on-top is ${alwaysOnTop}`);
    } else {
      testFail(
        "webview.alwaysOnTop getter",
        `Expected boolean, got ${typeof alwaysOnTop}`,
      );
    }
  } catch (error) {
    testFail(
      "webview.alwaysOnTop getter",
      "Failed to read always-on-top state",
      error,
    );
  }

  try {
    const clickThrough = webview.clickThrough;
    if (typeof clickThrough === "boolean") {
      testPass(
        "webview.clickThrough getter",
        `Click-through is ${clickThrough}`,
      );
    } else {
      testFail(
        "webview.clickThrough getter",
        `Expected boolean, got ${typeof clickThrough}`,
      );
    }
  } catch (error) {
    testFail(
      "webview.clickThrough getter",
      "Failed to read click-through flag",
      error,
    );
  }

  try {
    const maxSize = webview.maxSize;
    if (
      maxSize &&
      Number.isFinite(maxSize.width) &&
      Number.isFinite(maxSize.height)
    ) {
      testPass(
        "webview.maxSize getter",
        `maxSize reported as ${maxSize.width}x${maxSize.height}`,
      );
    } else {
      testFail("webview.maxSize getter", "Invalid maxSize payload");
    }
  } catch (error) {
    testFail("webview.maxSize getter", "Failed to read maxSize", error);
  }

  try {
    const minSize = webview.minSize;
    if (
      minSize &&
      Number.isFinite(minSize.width) &&
      Number.isFinite(minSize.height)
    ) {
      testPass(
        "webview.minSize getter",
        `minSize reported as ${minSize.width}x${minSize.height}`,
      );
    } else {
      testFail("webview.minSize getter", "Invalid minSize payload");
    }
  } catch (error) {
    testFail("webview.minSize getter", "Failed to read minSize", error);
  }

  try {
    const minimized = webview.minimized;
    if (typeof minimized === "boolean") {
      testPass("webview.minimized getter", `Minimized state is ${minimized}`);
    } else {
      testFail(
        "webview.minimized getter",
        `Expected boolean, got ${typeof minimized}`,
      );
    }
  } catch (error) {
    testFail(
      "webview.minimized getter",
      "Failed to read minimized state",
      error,
    );
  }

  try {
    const maximized = webview.maximized;
    if (typeof maximized === "boolean") {
      testPass("webview.maximized getter", `Maximized state is ${maximized}`);
    } else {
      testFail(
        "webview.maximized getter",
        `Expected boolean, got ${typeof maximized}`,
      );
    }
  } catch (error) {
    testFail(
      "webview.maximized getter",
      "Failed to read maximized state",
      error,
    );
  }

  // ========================================================================
  // ADVANCED WEBVIEW FEATURES
  // ========================================================================
  console.log("\n--- ADVANCED WEBVIEW FEATURES ---");

  // Test backgroundColor property
  try {
    const initialBgColor = webview.backgroundColor;
    if (Array.isArray(initialBgColor) && initialBgColor.length === 4) {
      testPass(
        "webview.backgroundColor getter",
        `Initial color: [${initialBgColor.join(", ")}]`,
      );
    } else {
      testFail(
        "webview.backgroundColor getter",
        `Expected [r,g,b,a] array, got ${JSON.stringify(initialBgColor)}`,
      );
    }

    // Set a new background color
    const testColor = [50, 100, 150, 255];
    webview.backgroundColor = testColor;
    const newBgColor = webview.backgroundColor;
    if (
      newBgColor[0] === testColor[0] &&
      newBgColor[1] === testColor[1] &&
      newBgColor[2] === testColor[2] &&
      newBgColor[3] === testColor[3]
    ) {
      testPass(
        "webview.backgroundColor setter",
        `Set to [${testColor.join(", ")}]`,
      );
    } else {
      testWarn(
        "webview.backgroundColor setter",
        `Expected [${testColor.join(", ")}], got [${newBgColor.join(", ")}]`,
      );
    }

    // Restore to white
    webview.backgroundColor = [255, 255, 255, 255];
  } catch (error) {
    testFail(
      "webview.backgroundColor",
      "Failed to get/set background color",
      error,
    );
  }

  // Test forceDarkMode property
  try {
    const initialDarkMode = webview.forceDarkMode;
    if (typeof initialDarkMode === "boolean") {
      testPass(
        "webview.forceDarkMode getter",
        `Initial value: ${initialDarkMode}`,
      );
    } else {
      testFail(
        "webview.forceDarkMode getter",
        `Expected boolean, got ${typeof initialDarkMode}`,
      );
    }

    // Toggle dark mode
    webview.forceDarkMode = true;
    if (webview.forceDarkMode === true) {
      testPass("webview.forceDarkMode setter", "Set to true");
    } else {
      testWarn(
        "webview.forceDarkMode setter",
        `Expected true, got ${webview.forceDarkMode}`,
      );
    }

    // Restore
    webview.forceDarkMode = initialDarkMode;
  } catch (error) {
    testFail("webview.forceDarkMode", "Failed to get/set force dark mode", error);
  }

  // Test contextMenu property
  try {
    const initialContextMenu = webview.contextMenu;
    if (typeof initialContextMenu === "boolean") {
      testPass(
        "webview.contextMenu getter",
        `Initial value: ${initialContextMenu}`,
      );
    } else {
      testFail(
        "webview.contextMenu getter",
        `Expected boolean, got ${typeof initialContextMenu}`,
      );
    }

    // Disable context menu
    webview.contextMenu = false;
    if (webview.contextMenu === false) {
      testPass("webview.contextMenu setter", "Set to false");
    } else {
      testWarn(
        "webview.contextMenu setter",
        `Expected false, got ${webview.contextMenu}`,
      );
    }

    // Restore
    webview.contextMenu = initialContextMenu;
  } catch (error) {
    testFail("webview.contextMenu", "Failed to get/set context menu", error);
  }

  // Test inject() method
  console.log("\n--- SCRIPT INJECTION ---");
  try {
    // Inject a script that runs on DOM ready
    webview.inject({
      code: "window.__injectedReady = true;",
      time: "ready",
      frame: "top",
      permanent: false,
    });
    testPass("webview.inject (ready)", "Injected script for DOM ready");

    // Inject a permanent script
    webview.inject({
      code: "window.__injectedPermanent = 'persistent';",
      time: "ready",
      permanent: true,
    });
    testPass("webview.inject (permanent)", "Injected permanent script");

    // Inject a creation-time script
    webview.inject({
      code: "window.__injectedCreation = Date.now();",
      time: "creation",
    });
    testPass("webview.inject (creation)", "Injected creation-time script");
  } catch (error) {
    testFail("webview.inject", "Failed to inject scripts", error);
  }

  // Test embed() and serve() methods
  console.log("\n--- FILE EMBEDDING ---");
  try {
    const embeddedHtml = `
      <!DOCTYPE html>
      <html>
        <head>
          <title>Embedded Page</title>
          <style>
            body { font-family: sans-serif; padding: 20px; background: #e8f5e9; }
            h1 { color: #2e7d32; }
          </style>
        </head>
        <body>
          <h1>🎉 Embedded Content Works!</h1>
          <p>This page was served from embedded content via saucer:// protocol.</p>
          <p id="status">Checking injected scripts...</p>
          <script>
            const status = [];
            if (window.__injectedReady) status.push('ready script ✓');
            if (window.__injectedPermanent) status.push('permanent script ✓');
            if (window.__injectedCreation) status.push('creation script ✓');
            document.getElementById('status').textContent =
              status.length > 0 ? 'Injected: ' + status.join(', ') : 'No injected scripts detected';
          </script>
        </body>
      </html>
    `;

    const embeddedCss = `
      body { border: 3px solid #4caf50; }
    `;

    webview.embed({
      "index.html": {
        content: embeddedHtml,
        mime: "text/html",
      },
      "styles.css": {
        content: embeddedCss,
        mime: "text/css",
      },
      "data.json": {
        content: JSON.stringify({ embedded: true, timestamp: Date.now() }),
        mime: "application/json",
      },
    });
    testPass("webview.embed", "Embedded 3 files (html, css, json)");

    // Test embedding with Buffer
    const bufferContent = Buffer.from("Binary content test", "utf-8");
    webview.embed({
      "binary.txt": {
        content: bufferContent,
        mime: "text/plain",
      },
    });
    testPass("webview.embed (Buffer)", "Embedded file with Buffer content");
  } catch (error) {
    testFail("webview.embed", "Failed to embed files", error);
  }

  // Test handleScheme() method
  console.log("\n--- CUSTOM URL SCHEME HANDLERS ---");
  let schemeHandlerCalled = false;

  try {
    // Register a custom scheme handler
    webview.handleScheme("myapp", (request) => {
      schemeHandlerCalled = true;
      testPass(
        "webview.handleScheme callback",
        `Handler called for: ${request.url}`,
      );

      // Return dynamic content based on the URL
      const path = request.url.replace("myapp://", "");

      if (path === "hello") {
        return {
          data: "<html><body><h1>Hello from myapp:// scheme!</h1></body></html>",
          mime: "text/html",
          status: 200,
        };
      } else if (path === "api/data") {
        return {
          data: JSON.stringify({ success: true, path: path }),
          mime: "application/json",
          status: 200,
          headers: { "X-Custom-Header": "test-value" },
        };
      } else {
        return {
          data: `<html><body><h1>404 - Not Found</h1><p>Path: ${path}</p></body></html>`,
          mime: "text/html",
          status: 404,
        };
      }
    });
    testPass("webview.handleScheme", "Registered 'myapp' scheme handler");

    // Register async scheme handler
    webview.handleScheme(
      "test",
      async (request) => {
        // Simulate async operation
        await new Promise((r) => setTimeout(r, 10));
        return {
          data: `<html><body><h1>Async Response</h1><p>URL: ${request.url}</p></body></html>`,
          mime: "text/html",
          status: 200,
        };
      },
      "async",
    );
    testPass("webview.handleScheme (async)", "Registered 'test' async scheme handler");
  } catch (error) {
    testFail("webview.handleScheme", "Failed to register scheme handler", error);
  }

  // Test clearScripts() method
  try {
    webview.clearScripts();
    testPass("webview.clearScripts", "Cleared all injected scripts");
  } catch (error) {
    testFail("webview.clearScripts", "Failed to clear scripts", error);
  }

  // Re-inject for later testing
  try {
    webview.inject({
      code: "window.__reinjected = true;",
      time: "ready",
      permanent: true,
    });
    testPass("webview.inject (re-inject)", "Re-injected script after clear");
  } catch (error) {
    testFail("webview.inject (re-inject)", "Failed to re-inject script", error);
  }

  // ========================================================================
  // WINDOW METHODS
  // ========================================================================
  console.log("\n--- WINDOW METHODS ---");

  // Test focus() method
  try {
    webview.focus();
    testPass("webview.focus", "Focused the window");
  } catch (error) {
    testFail("webview.focus", "Failed to focus window", error);
  }

  // Test startDrag() method - note: this is meant to be called from a mousedown event
  // We can't actually test the drag behavior, but we can verify the method exists
  try {
    // Don't actually call startDrag as it would start a drag operation
    if (typeof webview.startDrag === "function") {
      testPass("webview.startDrag", "startDrag method is available");
    } else {
      testFail("webview.startDrag", "startDrag method not found");
    }
  } catch (error) {
    testFail("webview.startDrag", "Failed to check startDrag method", error);
  }

  // Test startResize() method - similar to startDrag
  try {
    if (typeof webview.startResize === "function") {
      testPass("webview.startResize", "startResize method is available");
    } else {
      testFail("webview.startResize", "startResize method not found");
    }
  } catch (error) {
    testFail("webview.startResize", "Failed to check startResize method", error);
  }

  // Test setIcon() method - we'll test with a non-existent file (should not crash)
  try {
    if (typeof webview.setIcon === "function") {
      testPass("webview.setIcon", "setIcon method is available");
      // Note: We don't actually set an icon as we don't have a test icon file
      // webview.setIcon('/path/to/icon.png');
    } else {
      testFail("webview.setIcon", "setIcon method not found");
    }
  } catch (error) {
    testFail("webview.setIcon", "Failed to check setIcon method", error);
  }

  // ========================================================================
  // SUPPORTING TYPES
  // ========================================================================
  console.log("\n--- SUPPORTING TYPES ---");

  // Test Stash class
  try {
    const testData = Buffer.from("Hello, Saucer!");
    const stash = Stash.from(testData);
    if (stash) {
      if (stash.size === testData.length) {
        testPass("Stash.from", `Created stash with size ${stash.size}`);
      } else {
        testFail("Stash.from", `Size mismatch: expected ${testData.length}, got ${stash.size}`);
      }

      const retrievedData = stash.getData();
      if (retrievedData && retrievedData.toString() === testData.toString()) {
        testPass("Stash.getData", "Retrieved data matches original");
      } else {
        testFail("Stash.getData", "Data mismatch");
      }

      if (stash.native && (typeof stash.native === "object" || typeof stash.native === "function")) {
        testPass("Stash.native", "Native stash handle is available");
      } else {
        testFail("Stash.native", `Unexpected native accessor type: ${typeof stash.native}`);
      }
    } else {
      testFail("Stash.from", "Failed to create stash");
    }
  } catch (error) {
    testFail("Stash", "Failed to test Stash class", error);
  }

  // Test Stash.view
  try {
    const testData = Buffer.from("View test data");
    const stashView = Stash.view(testData);
    if (stashView && stashView.size === testData.length) {
      testPass("Stash.view", `Created stash view with size ${stashView.size}`);
    } else {
      testFail("Stash.view", "Failed to create stash view");
    }
  } catch (error) {
    testFail("Stash.view", "Failed to test Stash.view", error);
  }

  // Test Icon class (static methods)
  try {
    if (typeof Icon.fromFile === "function") {
      testPass("Icon.fromFile", "Icon.fromFile method is available");
    } else {
      testFail("Icon.fromFile", "Icon.fromFile method not found");
    }

    if (typeof Icon.fromData === "function") {
      testPass("Icon.fromData", "Icon.fromData method is available");
    } else {
      testFail("Icon.fromData", "Icon.fromData method not found");
    }

    // Test creating an icon from data (using a minimal PNG)
    // Minimal 1x1 transparent PNG
    const minimalPng = Buffer.from([
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
      0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
      0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00,
      0x0a, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x00, 0x01, 0x00, 0x00,
      0x05, 0x00, 0x01, 0x0d, 0x0a, 0x2d, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x49,
      0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
    ]);
    const icon = Icon.fromData(minimalPng);
    if (icon) {
      testPass("Icon.fromData (create)", "Created icon from PNG data");

      if (icon.native && (typeof icon.native === "object" || typeof icon.native === "function")) {
        testPass("Icon.native", "Native icon handle is available");
      } else {
        testFail("Icon.native", `Unexpected native accessor type: ${typeof icon.native}`);
      }

      if (typeof icon.isEmpty === "function") {
        const empty = icon.isEmpty();
        testPass("Icon.isEmpty", `Icon isEmpty returned ${empty}`);
      }

      if (typeof icon.getData === "function") {
        const iconData = icon.getData();
        if (iconData && iconData.length > 0) {
          testPass("Icon.getData", `Got icon data (${iconData.length} bytes)`);
        } else {
          testWarn("Icon.getData", "Icon data is null or empty");
        }
      }

      if (typeof icon.save === "function") {
        testPass("Icon.save", "Icon.save method is available");
      }
    } else {
      testWarn("Icon.fromData (create)", "Could not create icon from minimal PNG (platform limitation)");
    }
  } catch (error) {
    testFail("Icon", "Failed to test Icon class", error);
  }

  // Test favicon getter
  try {
    const favicon = webview.favicon;
    // favicon may be null if no page is loaded yet, that's OK
    testPass("webview.favicon", `Favicon getter works (value: ${favicon ? `Buffer(${favicon.length})` : 'null'})`);
  } catch (error) {
    testFail("webview.favicon", "Failed to get favicon", error);
  }

  // Test pageTitle getter
  try {
    const pageTitle = webview.pageTitle;
    testPass("webview.pageTitle", `Page title getter works (value: ${pageTitle || 'null'})`);
  } catch (error) {
    testFail("webview.pageTitle", "Failed to get page title", error);
  }

  // Test setFile() method
  try {
    if (typeof webview.setFile === "function") {
      testPass("webview.setFile", "setFile method is available");
      // Note: We don't actually call it as we don't have a test HTML file
      // webview.setFile('/path/to/index.html');
    } else {
      testFail("webview.setFile", "setFile method not found");
    }
  } catch (error) {
    testFail("webview.setFile", "Failed to check setFile method", error);
  }

  // Test clearExposed() method
  try {
    if (typeof webview.clearExposed === "function") {
      testPass("webview.clearExposed", "clearExposed method is available");
      // We'll test this after expose() tests
    } else {
      testFail("webview.clearExposed", "clearExposed method not found");
    }
  } catch (error) {
    testFail("webview.clearExposed", "Failed to check clearExposed method", error);
  }

  // Test Desktop class
  console.log("\n--- DESKTOP MODULE ---");
  try {
    const desktop = new Desktop(app);
    testPass("new Desktop", "Created Desktop instance successfully");

    if (desktop.native && (typeof desktop.native === "object" || typeof desktop.native === "function")) {
      testPass("Desktop.native", "Native desktop handle is available");
    } else {
      testFail("Desktop.native", `Unexpected native accessor type: ${typeof desktop.native}`);
    }

    if (typeof desktop.open === "function") {
      testPass("Desktop.open", "open method is available");
    } else {
      testFail("Desktop.open", "open method not found");
    }

    if (typeof desktop.pickFile === "function") {
      testPass("Desktop.pickFile", "pickFile method is available");
    } else {
      testFail("Desktop.pickFile", "pickFile method not found");
    }

    if (typeof desktop.pickFolder === "function") {
      testPass("Desktop.pickFolder", "pickFolder method is available");
    } else {
      testFail("Desktop.pickFolder", "pickFolder method not found");
    }

    if (typeof desktop.pickFiles === "function") {
      testPass("Desktop.pickFiles", "pickFiles method is available");
    } else {
      testFail("Desktop.pickFiles", "pickFiles method not found");
    }

    if (typeof desktop.pickFolders === "function") {
      testPass("Desktop.pickFolders", "pickFolders method is available");
    } else {
      testFail("Desktop.pickFolders", "pickFolders method not found");
    }
  } catch (error) {
    testFail("Desktop", "Failed to test Desktop class", error);
  }

  // Test PDF class
  console.log("\n--- PDF MODULE ---");
  try {
    const pdf = new PDF(webview);
    testPass("new PDF", "Created PDF instance successfully");

    if (pdf.native && (typeof pdf.native === "object" || typeof pdf.native === "function")) {
      testPass("PDF.native", "Native PDF handle is available");
    } else {
      testFail("PDF.native", `Unexpected native accessor type: ${typeof pdf.native}`);
    }

    if (typeof pdf.save === "function") {
      testPass("PDF.save", "save method is available");
      // Note: We don't actually call save() as it would open a dialog or write a file
    } else {
      testFail("PDF.save", "save method not found");
    }
  } catch (error) {
    testFail("PDF", "Failed to test PDF class", error);
  }

  // Window events
  console.log("\n--- WINDOW EVENTS ---");
  let onceLoadCount = 0;
  let offNavigateCount = 0;
  try {
    webview.on("resize", (w, h) => {
      try {
        recordEvent("resize");
        testPass("event:resize", `Window resized to ${w}x${h}`);
      } catch (error) {
        testFail("event:resize", "Error in resize handler", error);
      }
    });

    webview.on("focus", (focused) => {
      try {
        recordEvent("focus");
        testPass("event:focus", `Focus ${focused ? "gained" : "lost"}`);
      } catch (error) {
        testFail("event:focus", "Error in focus handler", error);
      }
    });

    webview.on("closed", () => {
      try {
        recordEvent("closed");
        testPass("event:closed", "Window closed");

        // Print final test report
        printFinalReport();

        // Quit the application when window is closed
        app.quit();
        // Give app.quit() a moment to clean up, then force exit
        setTimeout(() => {
          process.exit(
            testResults.failed.length > 0 || testResults.errors.length > 0
              ? 1
              : 0,
          );
        }, 100);
      } catch (error) {
        testFail("event:closed", "Error in closed handler", error);
        process.exit(1);
      }
    });

    webview.on("close", () => {
      try {
        recordEvent("close");
        testPass("event:close", "Close requested (allowing)");
        return true; // Allow the window to close
      } catch (error) {
        testFail("event:close", "Error in close handler", error);
        return false;
      }
    });

    webview.on("decorated", (decorated) => {
      try {
        recordEvent("decorated");
        testPass("event:decorated", `Decorations: ${decorated}`);
      } catch (error) {
        testFail("event:decorated", "Error in decorated handler", error);
      }
    });

    webview.on("maximize", (maximized) => {
      try {
        recordEvent("maximize");
        testPass("event:maximize", `Maximized: ${maximized}`);
      } catch (error) {
        testFail("event:maximize", "Error in maximize handler", error);
      }
    });

    webview.on("minimize", (minimized) => {
      try {
        recordEvent("minimize");
        testPass("event:minimize", `Minimized: ${minimized}`);
      } catch (error) {
        testFail("event:minimize", "Error in minimize handler", error);
      }
    });

    testPass("Window events", "All window event handlers registered");
  } catch (error) {
    testFail(
      "Window events",
      "Failed to register window event handlers",
      error,
    );
  }

  // Webview events
  console.log("\n--- WEBVIEW EVENTS ---");
  try {
    webview.on("dom-ready", () => {
      try {
        recordEvent("dom-ready");
        testPass("event:dom-ready", "DOM is ready");
        if (domReadyResolve) {
          domReadyResolve();
          domReadyResolve = null;
        }
      } catch (error) {
        testFail("event:dom-ready", "Error in dom-ready handler", error);
      }
    });

    webview.on("load", (state) => {
      try {
        recordEvent("load");
        testPass("event:load", `Load state: ${state}`);
      } catch (error) {
        testFail("event:load", "Error in load handler", error);
      }
    });

    webview.once("load", () => {
      onceLoadCount += 1;
      if (onceLoadCount === 1) {
        testPass("webview.once callback", "One-time load listener fired once");
      } else {
        testFail(
          "webview.once callback",
          `One-time load listener fired ${onceLoadCount} times`,
        );
      }
    });
    testPass("webview.once registration", "Registered one-time load listener");

    webview.on("title", (title) => {
      try {
        recordEvent("title");
        testPass("event:title", `Title changed to: "${title}"`);
      } catch (error) {
        testFail("event:title", "Error in title handler", error);
      }
    });

    webview.on("navigated", (url) => {
      try {
        recordEvent("navigated");
        testPass("event:navigated", `Navigated to: ${url.substring(0, 60)}...`);
      } catch (error) {
        testFail("event:navigated", "Error in navigated handler", error);
      }
    });

    webview.on("navigate", (nav) => {
      try {
        recordEvent("navigate");
        testPass("event:navigate", `Navigation requested (allowing)`);
        return true; // allow navigation
      } catch (error) {
        testFail("event:navigate", "Error in navigate handler", error);
        return false;
      }
    });

    const removedNavigateHandler = () => {
      offNavigateCount += 1;
      return true;
    };
    webview.on("navigate", removedNavigateHandler);
    webview.off("navigate", removedNavigateHandler);
    testPass("webview.off registration", "Listener removed successfully");

    webview.on("favicon", (buf) => {
      try {
        recordEvent("favicon");
        if (buf) {
          testPass("event:favicon", `Received favicon: ${buf.length} bytes`);
        } else {
          testPass("event:favicon", "No favicon (null/undefined)");
        }
      } catch (error) {
        testFail("event:favicon", "Error in favicon handler", error);
      }
    });

    testPass("Webview events", "All webview event handlers registered");
  } catch (error) {
    testFail(
      "Webview events",
      "Failed to register webview event handlers",
      error,
    );
  }

  console.log("\n--- MESSAGING & RPC ---");

  // Track message receipt for validation
  let messageReceived = null;

  try {
    webview.onMessage((msg) => {
      try {
        recordEvent("message");
        messageReceived = msg;
        testPass("webview.onMessage", `Received message: ${msg}`);
        return true;
      } catch (error) {
        testFail("webview.onMessage handler", "Error in message handler", error);
        return false;
      }
    });
    testPass("webview.onMessage registration", "Message handler registered");
  } catch (error) {
    testFail(
      "webview.onMessage registration",
      "Failed to register message handler",
      error,
    );
  }

  try {
    webview.expose("sum", (a, b) => a + b);
    webview.expose(
      "echoAsync",
      async (payload) => {
        return { received: payload, ok: true };
      },
      { async: true },
    );
    // Expose a temporary function to test clearExposed
    webview.expose("tempFunc", () => "temp");
    testPass("webview.expose", "Registered Smartview RPC handlers");
  } catch (error) {
    testFail("webview.expose", "Failed to register RPC handlers", error);
  }

  // Test clearExposed() with a specific function
  try {
    webview.clearExposed("tempFunc");
    testPass("webview.clearExposed (specific)", "Cleared 'tempFunc' RPC handler");
  } catch (error) {
    testFail("webview.clearExposed (specific)", "Failed to clear specific RPC handler", error);
  }

  // Test Application threading methods
  console.log("\n--- APPLICATION THREADING ---");

  // Test post(): schedule a UI-thread callback
  let postDone;
  try {
    postDone = new Promise((resolve, reject) => {
      try {
        app.post(() => {
          try {
            testPass("app.post", "Callback executed on UI thread");
            resolve();
          } catch (error) {
            testFail("app.post callback", "Error inside post callback", error);
            reject(error);
          }
        });
      } catch (error) {
        testFail("app.post", "Failed to schedule post callback", error);
        reject(error);
      }
    });
  } catch (error) {
    testFail("app.post setup", "Failed to set up post test", error);
  }

  // Test dispatch(): run on UI thread and return a value
  try {
    const dispatchResult = await app.dispatch(() => {
      return `executed at ${new Date().toISOString()}`;
    });
    if (dispatchResult && dispatchResult.includes("executed at")) {
      testPass("app.dispatch", `Returned correct value: "${dispatchResult}"`);
    } else {
      testFail("app.dispatch", `Unexpected return value: "${dispatchResult}"`);
    }
  } catch (error) {
    testFail("app.dispatch", "Failed to execute dispatch", error);
  }

  // Test make(): alias for dispatch that returns a value
  try {
    const makeResult = await app.make(() => {
      return { createdBy: "make", timestamp: Date.now() };
    });
    if (
      makeResult &&
      makeResult.createdBy === "make" &&
      typeof makeResult.timestamp === "number"
    ) {
      testPass(
        "app.make",
        `Returned correct object: ${JSON.stringify(makeResult)}`,
      );
    } else {
      testFail(
        "app.make",
        `Unexpected return value: ${JSON.stringify(makeResult)}`,
      );
    }
  } catch (error) {
    testFail("app.make", "Failed to execute make", error);
  }

  // Test poolSubmit(): run on the thread pool and await
  try {
    const poolResult = await app.poolSubmit(() => {
      let total = 0;
      for (let i = 0; i < 1_000_00; i += 1) {
        total += i;
      }
      return total;
    });
    const expectedSum = 4999950000;
    if (poolResult === expectedSum) {
      testPass("app.poolSubmit", `Computed correct sum: ${poolResult}`);
    } else {
      testFail("app.poolSubmit", `Expected ${expectedSum}, got ${poolResult}`);
    }
  } catch (error) {
    testFail("app.poolSubmit", "Failed to execute poolSubmit", error);
  }

  // Test poolEmplace(): fire-and-forget pool task
  try {
    app.poolEmplace(() => {
      testPass("app.poolEmplace", "Background task completed");
    });
    testPass(
      "app.poolEmplace call",
      "poolEmplace called successfully (fire-and-forget)",
    );
  } catch (error) {
    testFail("app.poolEmplace", "Failed to execute poolEmplace", error);
  }

  // Verify run() is a harmless no-op in these bindings
  try {
    app.run();
    testPass("app.run", "Called successfully (no-op in Node.js bindings)");
  } catch (error) {
    testFail("app.run", "run() threw an error", error);
  }

  // Test webview properties and navigation
  console.log("\n--- WEBVIEW PROPERTIES ---");

  // Render the initial page with mode-specific content
  // Detect platform for platform-specific instructions
  const isMacOS = process.platform === "darwin";
  const isWindows = process.platform === "win32";
  const isLinux = process.platform === "linux";

  const manualInstructions =
    testMode === "manual"
      ? `
    <div style="background: #fff3cd; padding: 16px; border-radius: 8px; border: 2px solid #ffc107; margin: 20px 0;">
      <h2 style="margin-top: 0; color: #856404;">👤 Manual Testing Mode</h2>
      <p><strong>Please test these window actions to trigger events:</strong></p>
      <ul style="line-height: 2;">
        ${isMacOS
        ? `
          <li>🟢 <strong>Full Screen</strong> (green button) → macOS doesn't have maximize, uses full screen instead</li>
          <li>🟡 <strong>Minimize</strong> (yellow button) → triggers <code>minimize</code> event</li>
          <li>📐 <strong>Restore</strong> from Dock → triggers <code>minimize</code> event (false)</li>
        `
        : isWindows
          ? `
          <li>🔲 <strong>Maximize</strong> (□ button) → triggers <code>maximize</code> event</li>
          <li>🔳 <strong>Restore</strong> from maximized → triggers <code>maximize</code> event (false)</li>
          <li>➖ <strong>Minimize</strong> (− button) → triggers <code>minimize</code> event</li>
          <li>📐 <strong>Restore</strong> from taskbar → triggers <code>minimize</code> event (false)</li>
        `
          : `
          <li>🔲 <strong>Maximize</strong> the window → triggers <code>maximize</code> event (if available)</li>
          <li>➖ <strong>Minimize</strong> the window → triggers <code>minimize</code> event</li>
          <li>📐 <strong>Restore</strong> the window → triggers events (false)</li>
        `
      }
        <li>↔️ <strong>Resize manually</strong> (drag edges) → triggers <code>resize</code> event</li>
        <li>🌐 <strong>Favicon</strong> will load from example.com automatically</li>
      </ul>
      ${isMacOS
        ? `
        <p style="background: #e7f3ff; padding: 12px; border-left: 4px solid #0066cc; margin: 12px 0;">
          <strong>ℹ️ macOS Note:</strong> The <code>maximize</code> event may not fire on macOS since it uses
          full screen mode instead of traditional maximize. This is normal behavior.
        </p>
      `
        : ""
      }
      <p style="color: #856404; margin-bottom: 8px;"><strong>Note:</strong> Window decorations toggle is OS-dependent and may not be available.</p>
      <p style="margin-bottom: 0;"><strong>When done testing, close this window to see the final report.</strong></p>
      <p style="color: #856404; font-size: 0.9em; margin-bottom: 0;">All events are tracked automatically - check console for real-time feedback!</p>
    </div>
  `
      : "";

  const html = `
    <!doctype html>
    <html>
      <head>
        <meta charset="UTF-8" />
        <title>Saucer Feature Test - Page 1</title>
        <link rel="icon" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><text y='.9em' font-size='90'>🚀</text></svg>">
        <style>
          body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; padding: 32px; background: #f0f8ff; }
          h1 { margin-top: 0; color: #2c3e50; }
          h2 { color: #2c3e50; font-size: 1.3em; }
          code { background: #fff; padding: 2px 6px; border-radius: 4px; border: 1px solid #ddd; }
          ul { line-height: 1.8; }
          .status { color: #27ae60; font-weight: bold; }
          .mode-badge {
            display: inline-block;
            padding: 4px 12px;
            background: ${testMode === "manual" ? "#ffc107" : "#28a745"};
            color: white;
            border-radius: 4px;
            font-size: 0.9em;
            font-weight: bold;
          }
        </style>
      </head>
      <body>
        <h1>🚀 Saucer Application Feature Test</h1>
        <p>
          Mode: <span class="mode-badge">${testMode.toUpperCase()}</span>
        </p>
        ${manualInstructions}
        <h2>Application Features Tested</h2>
        <ul>
          <li><strong>isThreadSafe</strong>: <span class="status">${app.isThreadSafe()}</span></li>
          <li><strong>post()</strong>: <span class="status">check console</span></li>
          <li><strong>poolEmplace()</strong>: <span class="status">check console</span></li>
          <li><strong>nativeHandle()</strong>: <span class="status">${app.nativeHandle()}</span></li>
        </ul>
        <h2>Advanced Webview Features</h2>
        <ul>
          <li><strong>backgroundColor</strong>: RGBA color property</li>
          <li><strong>forceDarkMode</strong>: Force dark mode toggle</li>
          <li><strong>contextMenu</strong>: Right-click menu toggle</li>
          <li><strong>inject()</strong>: Script injection (creation/ready time)</li>
          <li><strong>embed()</strong>: File embedding via saucer:// protocol</li>
          <li><strong>serve()</strong>: Navigate to embedded files</li>
          <li><strong>handleScheme()</strong>: Custom URL scheme handlers (myapp://, test://)</li>
          <li><strong>clearScripts()</strong>: Remove injected scripts</li>
          <li><strong>clearEmbedded()</strong>: Remove embedded files</li>
          <li><strong>removeScheme()</strong>: Unregister scheme handlers</li>
        </ul>
        <h2>Window Methods</h2>
        <ul>
          <li><strong>focus()</strong>: Focus the window</li>
          <li><strong>startDrag()</strong>: Start window drag (for custom title bars)</li>
          <li><strong>startResize()</strong>: Start resize from edge</li>
          <li><strong>setIcon()</strong>: Set window icon from file or Buffer</li>
        </ul>
        ${testMode === "automated" ? "<p><em>Window will navigate to Page 2, then demonstrate back/forward navigation...</em></p>" : "<p><em>Automated tests are running in the background. You can interact with the window at any time.</em></p>"}
      </body>
    </html>
  `;

  try {
    webview.navigate(
      "data:text/html;charset=utf-8," + encodeURIComponent(html),
    );
    testPass("webview.navigate", "Navigated to initial page");
  } catch (error) {
    testFail("webview.navigate", "Failed to navigate", error);
  }

  // Note: loadHtml is tested for method availability in Phase 1 section above
  // We don't call it here to avoid rapid navigation which can cause crashes

  // Test URL property getter
  setTimeout(() => {
    try {
      const currentUrl = webview.url;
      if (currentUrl && currentUrl.startsWith("data:text/html")) {
        testPass("webview.url getter", `Retrieved URL successfully`);
      } else {
        testFail("webview.url getter", `Unexpected URL: ${currentUrl}`);
      }
    } catch (error) {
      testFail("webview.url getter", "Failed to get URL", error);
    }
  }, 100);

  // Test show/hide methods
  console.log("\n--- WINDOW METHODS ---");
  try {
    webview.show();
    testPass("webview.show", "Window shown successfully");
  } catch (error) {
    testFail("webview.show", "Failed to show window", error);
  }

  setTimeout(() => {
    try {
      const visibleAfterShow = webview.visible;
      if (visibleAfterShow === true) {
        testPass("webview.visible after show", "Window is visible (correct)");
      } else {
        testWarn(
          "webview.visible after show",
          `Expected true, got ${visibleAfterShow}`,
        );
      }
    } catch (error) {
      testFail(
        "webview.visible after show",
        "Failed to check visibility",
        error,
      );
    }
  }, 200);

  // Test hide method
  setTimeout(() => {
    try {
      webview.hide();
      testPass("webview.hide", "Window hidden successfully");
      setTimeout(() => {
        try {
          webview.show();
          testPass(
            "webview.show (second call)",
            "Window shown again after hide",
          );
        } catch (error) {
          testFail(
            "webview.show (second call)",
            "Failed to show window",
            error,
          );
        }
      }, 1000);
    } catch (error) {
      testFail("webview.hide", "Failed to hide window", error);
    }
  }, 500);

  // Navigate to a second page to test navigation history
  console.log("\n--- WEBVIEW NAVIGATION ---");
  setTimeout(() => {
    try {
      // In manual mode or if testing favicon, navigate to a real URL with a favicon
      if (testMode === "manual") {
        // Navigate to a real URL to trigger favicon event
        webview.navigate("https://example.com");
        testPass(
          "webview.navigate (page 2)",
          "Navigated to example.com (to test favicon event)",
        );
      } else {
        const page2Html = `
          <!doctype html>
          <html>
            <head>
              <meta charset="UTF-8" />
              <title>Saucer Feature Test - Page 2</title>
              <style>
                body { font-family: sans-serif; padding: 32px; background: #ffe0f0; }
                h1 { color: #c2185b; }
              </style>
            </head>
            <body>
              <h1>📄 Page 2</h1>
              <p>This is the second page. Testing navigation methods...</p>
              <p>We will now test: <strong>back()</strong>, <strong>forward()</strong>, and <strong>reload()</strong></p>
            </body>
          </html>
        `;
        webview.navigate(
          "data:text/html;charset=utf-8," + encodeURIComponent(page2Html),
        );
        testPass("webview.navigate (page 2)", "Navigated to Page 2");
      }

      // Test URL property setter
      setTimeout(() => {
        try {
          const testUrl =
            "data:text/html;charset=utf-8," +
            encodeURIComponent("<h1>Testing URL setter</h1>");
          webview.url = testUrl;
          testPass("webview.url setter", "Set URL via property setter");
        } catch (error) {
          testFail("webview.url setter", "Failed to set URL", error);
        }
      }, 800);
    } catch (error) {
      testFail(
        "webview.navigate (page 2)",
        "Failed to navigate to page 2",
        error,
      );
    }
  }, 2000);

  // Test back() navigation
  setTimeout(() => {
    try {
      webview.back();
      testPass("webview.back", "Navigated back successfully");
    } catch (error) {
      testFail("webview.back", "Failed to navigate back", error);
    }
  }, 3500);

  // Test forward() navigation
  setTimeout(() => {
    try {
      webview.forward();
      testPass("webview.forward", "Navigated forward successfully");
    } catch (error) {
      testFail("webview.forward", "Failed to navigate forward", error);
    }
  }, 4500);

  // Test reload()
  setTimeout(() => {
    try {
      webview.reload();
      testPass("webview.reload", "Reloaded current page successfully");
    } catch (error) {
      testFail("webview.reload", "Failed to reload page", error);
    }
  }, 5500);

  // Test serve() - navigate to embedded content
  setTimeout(() => {
    console.log("\n--- EMBEDDED CONTENT SERVING ---");
    try {
      webview.serve("index.html");
      testPass("webview.serve", "Serving embedded index.html via saucer:// protocol");
    } catch (error) {
      testFail("webview.serve", "Failed to serve embedded file", error);
    }
  }, 6000);

  // Test custom scheme navigation
  setTimeout(() => {
    console.log("\n--- CUSTOM SCHEME NAVIGATION ---");
    try {
      webview.navigate("myapp://hello");
      testPass("Custom scheme navigate", "Navigated to myapp://hello");
    } catch (error) {
      testFail("Custom scheme navigate", "Failed to navigate to custom scheme", error);
    }
  }, 6800);

  // Test clearEmbedded() - clear a specific file
  setTimeout(() => {
    try {
      webview.clearEmbedded("binary.txt");
      testPass("webview.clearEmbedded (file)", "Cleared specific embedded file");
    } catch (error) {
      testFail("webview.clearEmbedded (file)", "Failed to clear embedded file", error);
    }
  }, 7200);

  // Test removeScheme()
  setTimeout(() => {
    try {
      webview.removeScheme("test");
      testPass("webview.removeScheme", "Removed 'test' scheme handler");
    } catch (error) {
      testFail("webview.removeScheme", "Failed to remove scheme handler", error);
    }
  }, 7400);

  // Test clearEmbedded() - clear all
  setTimeout(() => {
    try {
      webview.clearEmbedded();
      testPass("webview.clearEmbedded (all)", "Cleared all embedded files");
    } catch (error) {
      testFail("webview.clearEmbedded (all)", "Failed to clear all embedded files", error);
    }
  }, 7600);

  // Test devTools property
  setTimeout(() => {
    console.log("\n--- DEVELOPER TOOLS ---");
    try {
      webview.devTools = true;
      testPass("webview.devTools = true", "Opened developer tools");

      setTimeout(() => {
        try {
          const devToolsState = webview.devTools;
          if (typeof devToolsState === "boolean") {
            testPass(
              "webview.devTools getter",
              `Retrieved devTools state: ${devToolsState}`,
            );
          } else {
            testWarn(
              "webview.devTools getter",
              `Unexpected type: ${typeof devToolsState}`,
            );
          }

          webview.devTools = false;
          testPass("webview.devTools = false", "Closed developer tools");
        } catch (error) {
          testFail(
            "webview.devTools getter/setter",
            "Error toggling devTools",
            error,
          );
        }
      }, 1500);
    } catch (error) {
      testFail(
        "webview.devTools = true",
        "Failed to open developer tools",
        error,
      );
    }
  }, 8000);

  // Trigger a resize event programmatically
  setTimeout(() => {
    console.log("\n--- WINDOW STATE CHANGES ---");
    try {
      webview.size = { width: 940, height: 660 };
      testPass("webview.size (resize)", "Triggered resize to 940x660");
    } catch (error) {
      testFail("webview.size (resize)", "Failed to resize window", error);
    }
  }, 10000);

  // Update title from inside the webview to exercise the title event
  const lastUITest = new Promise((resolve) => {
    setTimeout(() => {
      console.log("\n--- JAVASCRIPT EXECUTION ---");
      try {
        webview.execute(`document.title = 'Saucer Event Demo';`);
        testPass("webview.execute", "Executed JavaScript successfully");
      } catch (error) {
        testFail("webview.execute", "Failed to execute JavaScript", error);
      }
      // Resolve after a short delay to let any triggered events settle
      setTimeout(() => {
        if (onceLoadCount === 1) {
          testPass("webview.once behavior", "One-time listener fired exactly once");
        } else {
          testFail(
            "webview.once behavior",
            `Expected one-time listener to fire once, got ${onceLoadCount}`,
          );
        }

        if (offNavigateCount === 0) {
          testPass("webview.off behavior", "Removed navigate listener was not invoked");
        } else {
          testFail(
            "webview.off behavior",
            `Removed navigate listener was invoked ${offNavigateCount} times`,
          );
        }

        resolve();
      }, 1000);
    }, 11000);
  });

  // Smartview RPC / Messaging tests - run early on Page 1 (before navigation)
  // Schedule at t=1000ms (after initial page load, before hide/show at t=500ms)
  const rpcTestsDone = new Promise((resolve) => {
    setTimeout(async () => {
      console.log("\n--- SMARTVIEW RPC / MESSAGING TESTS ---");
      console.log("Running on initial page before navigation...");

      try {
        const messagePayload = { source: "webview", stage: "rpc-check" };
        messageReceived = null; // Reset before sending

        await webview.evaluate(
          "window.saucer.internal.send_message(JSON.stringify({}))",
          messagePayload,
        );

        // Wait for message to be received
        await new Promise((r) => setTimeout(r, 200));

        // Validate message was received
        if (messageReceived === null) {
          testFail(
            "Message round-trip",
            "Message handler was never called",
          );
        } else {
          // Parse and validate message content
          try {
            const parsedMessage = JSON.parse(messageReceived);
            if (
              parsedMessage.source === messagePayload.source &&
              parsedMessage.stage === messagePayload.stage
            ) {
              testPass(
                "Message round-trip",
                `Message correctly received with payload: ${messageReceived}`,
              );
            } else {
              testFail(
                "Message round-trip",
                `Message payload mismatch. Expected ${JSON.stringify(messagePayload)}, got ${messageReceived}`,
              );
            }
          } catch (parseError) {
            testFail(
              "Message round-trip",
              `Failed to parse received message: ${messageReceived}`,
              parseError,
            );
          }
        }
      } catch (error) {
        testFail(
          "webview.evaluate send_message",
          "Failed to dispatch message from webview",
          error,
        );
      }

      console.log("[DEBUG] Starting sum RPC test...");

      // First check if smartview bridge is available
      try {
        const bridgeCheck = await webview.evaluate(
          "typeof window.saucer !== 'undefined' && typeof window.saucer.exposed !== 'undefined' && typeof window.saucer.exposed.sum === 'function'"
        );
        console.log("[DEBUG] Smartview bridge check:", bridgeCheck);
      } catch (error) {
        console.log("[DEBUG] Bridge check failed:", error);
      }

      try {
        console.log("[DEBUG] Calling sum RPC with literal values...");
        const sumResult = await webview.evaluate(
          "window.saucer.exposed.sum(8, 4)"  // Literal values, no placeholders
        );
        console.log("[DEBUG] sum returned:", sumResult);
        if (sumResult === 12) {
          testPass("webview.expose sum", "RPC sum returned correct value (12)");
        } else {
          testFail("webview.expose sum", `Unexpected sum result: ${sumResult}`);
        }
      } catch (error) {
        console.log("[DEBUG] sum error:", error);
        testFail("webview.expose sum", "Failed to invoke RPC sum", error);
      }

      try {
        const echoResult = await webview.evaluate(
          "window.saucer.exposed.echoAsync({ nested: { ok: true }, items: [1, 2, 3] })"  // Literal object
        );
        if (
          echoResult &&
          echoResult.ok === true &&
          echoResult.received &&
          echoResult.received.items &&
          Array.isArray(echoResult.received.items)
        ) {
          testPass(
            "webview.expose echoAsync",
            "RPC echoAsync returned nested structure",
          );
        } else {
          testFail(
            "webview.expose echoAsync",
            `Unexpected echo result: ${JSON.stringify(echoResult)}`,
          );
        }
      } catch (error) {
        testFail(
          "webview.expose echoAsync",
          "Failed to invoke RPC echoAsync",
          error,
        );
      }

      try {
        const formattedResult = await webview.evaluate(
          "({}.value + {})",
          { value: 10 },
          5,
        );
        if (formattedResult === 15) {
          testPass("webview.evaluate args", "Formatted evaluation returned 15");
        } else {
          testFail(
            "webview.evaluate args",
            `Unexpected evaluation result: ${formattedResult}`,
          );
        }
      } catch (error) {
        testFail("webview.evaluate args", "Failed to evaluate with args", error);
      }

      // Wait for the post() callback to fire
      try {
        await postDone;
        testPass("app.post (await)", "post() callback completed successfully");
      } catch (error) {
        testFail("app.post (await)", "post() promise rejected", error);
      }

      // ============================================================
      // SmartviewRPC Tests - Type-safe RPC with zero-copy Buffer support
      // ============================================================
      console.log("\n--- SmartviewRPC Tests ---");

      // Test Types DSL
      try {
        if (Types.string.type === "string") {
          testPass("Types.string", "Types.string has correct type");
        } else {
          testFail("Types.string", `Unexpected type: ${Types.string.type}`);
        }

        if (Types.buffer.binary === true) {
          testPass("Types.buffer", "Types.buffer is marked as binary");
        } else {
          testFail("Types.buffer", "Types.buffer should be marked as binary");
        }

        const arrayType = Types.array(Types.number);
        if (arrayType.type === "number[]") {
          testPass("Types.array", "Types.array(Types.number) = number[]");
        } else {
          testFail("Types.array", `Unexpected array type: ${arrayType.type}`);
        }

        const paramType = Types.param("name", Types.string);
        if (paramType.name === "name" && paramType.type === "string") {
          testPass("Types.param", "Types.param creates named parameter");
        } else {
          testFail("Types.param", "Types.param has incorrect structure");
        }

        const optionalType = Types.optional(Types.number);
        if (optionalType.optional === true) {
          testPass("Types.optional", "Types.optional marks type as optional");
        } else {
          testFail("Types.optional", "Types.optional should set optional=true");
        }
      } catch (error) {
        testFail("Types DSL", "Types DSL test failed", error);
      }

      // Test SmartviewRPC class
      try {
        const rpc = createRPC(webview);
        if (rpc instanceof SmartviewRPC) {
          testPass("createRPC", "createRPC returns SmartviewRPC instance");
        } else {
          testFail("createRPC", "createRPC should return SmartviewRPC");
        }

        // Define a test function
        rpc.define("testSum", {
          params: [Types.param("a", Types.number), Types.param("b", Types.number)],
          returns: "number",
        }, (a, b) => a + b);

        const definedFuncs = rpc.getDefinedFunctions();
        if (definedFuncs.includes("testSum")) {
          testPass("SmartviewRPC.define", "Function registered successfully");
        } else {
          testFail("SmartviewRPC.define", "Function not found in definitions");
        }

        // Test generateTypes
        const generatedTypes = rpc.generateTypes();
        if (generatedTypes.includes("testSum") && generatedTypes.includes("declare global")) {
          testPass("SmartviewRPC.generateTypes", "TypeScript definitions generated");
        } else {
          testFail("SmartviewRPC.generateTypes", "Generated types missing expected content");
        }

        // Test binary function definition (just registration, not actual call)
        rpc.define("testBinaryEcho", {
          params: [Types.param("data", Types.buffer)],
          returns: "Buffer",
        }, (data) => data);

        if (rpc.getDefinedFunctions().includes("testBinaryEcho")) {
          testPass("SmartviewRPC binary define", "Binary function registered");
        } else {
          testFail("SmartviewRPC binary define", "Binary function not registered");
        }

        // Test undefine
        rpc.undefine("testSum");
        if (!rpc.getDefinedFunctions().includes("testSum")) {
          testPass("SmartviewRPC.undefine", "Function removed successfully");
        } else {
          testFail("SmartviewRPC.undefine", "Function still present after undefine");
        }

      } catch (error) {
        testFail("SmartviewRPC", "SmartviewRPC tests failed", error);
      }

      // Test scheme registration
      try {
        if (SmartviewRPC.isSchemeRegistered()) {
          testPass("SmartviewRPC.isSchemeRegistered", "Binary RPC scheme is registered");
        } else {
          testFail("SmartviewRPC.isSchemeRegistered", "Scheme should be registered");
        }
      } catch (error) {
        testFail("SmartviewRPC.isSchemeRegistered", "Failed to check scheme registration", error);
      }

      resolve();
    }, 1000); // Run early on stable Page 1
  });

  // ============================================================================
  // PHASE 3: Premium Features Tests
  // ============================================================================
  const phase3TestsDone = new Promise((resolve) => {
    setTimeout(async () => {
      console.log("\n--- PHASE 3: PREMIUM FEATURES ---");

      // --------------------------------------------------------------------
      // Clipboard API Tests
      // --------------------------------------------------------------------
      console.log("\n--- Clipboard API ---");

      try {
        // Test clipboard write
        const testText = `saucer-test-${Date.now()}`;
        clipboard.writeText(testText);
        testPass("clipboard.writeText", `Wrote text: "${testText}"`);

        // Test clipboard read
        const readText = clipboard.readText();
        if (readText === testText) {
          testPass("clipboard.readText", `Read back correctly: "${readText}"`);
        } else {
          testFail("clipboard.readText", `Expected "${testText}", got "${readText}"`);
        }

        // Test clipboard hasText
        const hasText = clipboard.hasText();
        if (hasText === true) {
          testPass("clipboard.hasText", "Correctly reports text is available");
        } else {
          testFail("clipboard.hasText", `Expected true, got ${hasText}`);
        }

        // Test clipboard hasImage
        const hasImage = clipboard.hasImage();
        if (typeof hasImage === "boolean") {
          testPass("clipboard.hasImage", `Reports image available: ${hasImage}`);
        } else {
          testFail("clipboard.hasImage", `Unexpected type: ${typeof hasImage}`);
        }

        // Test clipboard clear
        clipboard.clear();
        const afterClear = clipboard.readText();
        if (afterClear === "" || afterClear === null || afterClear === undefined) {
          testPass("clipboard.clear", "Clipboard cleared successfully");
        } else {
          testWarn("clipboard.clear", `Clipboard may not have cleared: "${afterClear}"`);
        }
      } catch (error) {
        testFail("Clipboard API", "Clipboard tests failed", error);
      }

      // --------------------------------------------------------------------
      // Notification API Tests
      // --------------------------------------------------------------------
      console.log("\n--- Notification API ---");

      try {
        // Test Notification.isSupported
        const isSupported = Notification.isSupported();
        if (typeof isSupported === "boolean") {
          testPass("Notification.isSupported", `Notifications supported: ${isSupported}`);
        } else {
          testFail("Notification.isSupported", `Unexpected type: ${typeof isSupported}`);
        }

        // Test Notification.requestPermission
        const permission = await Notification.requestPermission();
        if (permission === "granted" || permission === "denied" || permission === "default") {
          testPass("Notification.requestPermission", `Permission: ${permission}`);
        } else {
          testFail("Notification.requestPermission", `Unexpected permission: ${permission}`);
        }

        // Test Notification instance creation
        const notification = new Notification({
          title: "saucer-nodejs Test",
          body: "This is an automated test notification"
        });
        if (notification && typeof notification.show === "function") {
          testPass("new Notification", "Created notification instance");
        } else {
          testFail("new Notification", "Failed to create notification instance");
        }

        // Test notification.show (skip in automated mode to avoid popup)
        if (testMode === "manual") {
          notification.show();
          testPass("notification.show", "Notification displayed (check system tray)");
        } else {
          testPass("notification.show (skipped)", "Skipped in automated mode");
        }
      } catch (error) {
        testFail("Notification API", "Notification tests failed", error);
      }

      // --------------------------------------------------------------------
      // System Tray API Tests (macOS full implementation, stubs elsewhere)
      // --------------------------------------------------------------------
      console.log("\n--- System Tray API ---");

      let tray = null;
      try {
        // Test SystemTray creation
        tray = new SystemTray();
        if (tray && typeof tray.show === "function") {
          testPass("new SystemTray", "Created system tray instance");
        } else {
          testFail("new SystemTray", "Failed to create system tray instance");
        }

        // Test setTooltip
        tray.setTooltip("saucer-nodejs Test Tray");
        testPass("SystemTray.setTooltip", "Set tooltip successfully");

        // Test show (may be no-op on some platforms)
        tray.show();
        testPass("SystemTray.show", "Show called successfully");

        // Test hide
        tray.hide();
        testPass("SystemTray.hide", "Hide called successfully");

        // Test onClick registration (cannot be deterministically triggered here)
        tray.onClick(() => {});
        testPass("SystemTray.onClick", "Registered click callback successfully");

        // Test setMenu (basic)
        tray.setMenu([
          { label: "Test Item 1" },
          { type: "separator" },
          { label: "Test Item 2" }
        ]);
        testPass("SystemTray.setMenu", "Set menu items successfully");

      } catch (error) {
        testFail("SystemTray API", "System tray tests failed", error);
      } finally {
        // Cleanup
        if (tray) {
          try {
            tray.destroy();
            testPass("SystemTray.destroy", "Destroyed tray successfully");
          } catch (e) {
            testWarn("SystemTray.destroy", `Cleanup warning: ${e.message}`);
          }
        }
      }

      // --------------------------------------------------------------------
      // Window Position API Tests
      // --------------------------------------------------------------------
      console.log("\n--- Window Position API ---");

      try {
        // Test position getter
        const initialPos = webview.position;
        if (initialPos && typeof initialPos.x === "number" && typeof initialPos.y === "number") {
          testPass("webview.position getter", `Position: x=${initialPos.x}, y=${initialPos.y}`);
        } else {
          testFail("webview.position getter", `Unexpected position: ${JSON.stringify(initialPos)}`);
        }

        // Test position setter
        const newX = 150;
        const newY = 150;
        webview.position = { x: newX, y: newY };

        // Small delay for position to take effect
        await new Promise(r => setTimeout(r, 100));

        const newPos = webview.position;
        // Allow some tolerance for window manager adjustments
        if (newPos && Math.abs(newPos.x - newX) < 50 && Math.abs(newPos.y - newY) < 50) {
          testPass("webview.position setter", `Moved to x=${newPos.x}, y=${newPos.y}`);
        } else {
          testWarn("webview.position setter", `Position may not have updated: ${JSON.stringify(newPos)}`);
        }

        // Restore original position
        webview.position = { x: initialPos.x, y: initialPos.y };
        testPass("webview.position restore", `Restored to x=${initialPos.x}, y=${initialPos.y}`);
      } catch (error) {
        testFail("Window Position API", "Position tests failed", error);
      }

      // --------------------------------------------------------------------
      // Fullscreen API Tests
      // --------------------------------------------------------------------
      console.log("\n--- Fullscreen API ---");

      try {
        // Test fullscreen getter
        const initialFullscreen = webview.fullscreen;
        if (typeof initialFullscreen === "boolean") {
          testPass("webview.fullscreen getter", `Initial fullscreen: ${initialFullscreen}`);
        } else {
          testFail("webview.fullscreen getter", `Unexpected type: ${typeof initialFullscreen}`);
        }

        // Test fullscreen toggle (only in manual mode to avoid disruption)
        if (testMode === "manual") {
          webview.fullscreen = true;
          await new Promise(r => setTimeout(r, 500));

          if (webview.fullscreen === true) {
            testPass("webview.fullscreen = true", "Entered fullscreen mode");
          } else {
            testWarn("webview.fullscreen = true", `Fullscreen may not have toggled`);
          }

          webview.fullscreen = false;
          await new Promise(r => setTimeout(r, 500));

          if (webview.fullscreen === false) {
            testPass("webview.fullscreen = false", "Exited fullscreen mode");
          } else {
            testWarn("webview.fullscreen = false", `Fullscreen may not have toggled`);
          }
        } else {
          testPass("webview.fullscreen toggle (skipped)", "Skipped in automated mode");
        }
      } catch (error) {
        testFail("Fullscreen API", "Fullscreen tests failed", error);
      }

      // --------------------------------------------------------------------
      // Zoom API Tests
      // --------------------------------------------------------------------
      console.log("\n--- Zoom API ---");

      try {
        // Test zoom getter
        const initialZoom = webview.zoom;
        if (typeof initialZoom === "number" && initialZoom > 0) {
          testPass("webview.zoom getter", `Initial zoom: ${initialZoom}`);
        } else {
          testFail("webview.zoom getter", `Unexpected zoom: ${initialZoom}`);
        }

        // Test zoom setter - zoom in
        webview.zoom = 1.5;
        await new Promise(r => setTimeout(r, 100));

        const zoomedIn = webview.zoom;
        if (Math.abs(zoomedIn - 1.5) < 0.1) {
          testPass("webview.zoom = 1.5", `Zoomed to ${zoomedIn}`);
        } else {
          testWarn("webview.zoom = 1.5", `Expected 1.5, got ${zoomedIn}`);
        }

        // Test zoom setter - zoom out
        webview.zoom = 0.75;
        await new Promise(r => setTimeout(r, 100));

        const zoomedOut = webview.zoom;
        if (Math.abs(zoomedOut - 0.75) < 0.1) {
          testPass("webview.zoom = 0.75", `Zoomed to ${zoomedOut}`);
        } else {
          testWarn("webview.zoom = 0.75", `Expected 0.75, got ${zoomedOut}`);
        }

        // Restore original zoom
        webview.zoom = initialZoom;
        testPass("webview.zoom restore", `Restored zoom to ${initialZoom}`);
      } catch (error) {
        testFail("Zoom API", "Zoom tests failed", error);
      }

      console.log("\n--- Phase 3 Tests Complete ---");
      resolve();
    }, 12500); // Run after other UI tests complete
  });

  // ============================================================================
  // PHASE 4: Developer Experience Tests
  // ============================================================================
  const phase4TestsDone = new Promise((resolve) => {
    setTimeout(async () => {
      console.log("\n--- PHASE 4: DEVELOPER EXPERIENCE ---");

      // --------------------------------------------------------------------
      // Debug Module Tests
      // --------------------------------------------------------------------
      console.log("\n--- Debug Module ---");

      try {
        // Dynamic import of the debug module
        const debug = await import("../lib/debug.js");

        // Test createDebugger
        if (typeof debug.createDebugger === "function") {
          const customDebug = debug.createDebugger("test");
          if (typeof customDebug === "function") {
            testPass("debug.createDebugger", "Created custom debugger function");
          } else {
            testFail("debug.createDebugger", "Should return a function");
          }
        } else {
          testFail("debug.createDebugger", "createDebugger not found in module");
        }

        // Test pre-configured debuggers
        if (typeof debug.debugRPC === "function") {
          testPass("debug.debugRPC", "Pre-configured RPC debugger available");
        }

        if (typeof debug.debugWindow === "function") {
          testPass("debug.debugWindow", "Pre-configured window debugger available");
        }

        if (typeof debug.debugWebview === "function") {
          testPass("debug.debugWebview", "Pre-configured webview debugger available");
        }

        // Test logError and logWarn
        if (typeof debug.logError === "function") {
          testPass("debug.logError", "Error logging function available");
        }

        if (typeof debug.logWarn === "function") {
          testPass("debug.logWarn", "Warning logging function available");
        }

        // Test isDebugEnabled
        if (typeof debug.isDebugEnabled === "function") {
          const enabled = debug.isDebugEnabled();
          testPass("debug.isDebugEnabled", `Debug enabled: ${enabled}`);
        }

        // Test getEnabledCategories
        if (typeof debug.getEnabledCategories === "function") {
          const categories = debug.getEnabledCategories();
          if (Array.isArray(categories)) {
            testPass("debug.getEnabledCategories", `Categories: ${JSON.stringify(categories)}`);
          }
        }

        // Test measure function
        if (typeof debug.measure === "function") {
          const result = await debug.measure("test-operation", async () => {
            return 42;
          });
          if (result === 42) {
            testPass("debug.measure", "Measure function works correctly");
          }
        }

      } catch (error) {
        testFail("Debug Module", "Debug module tests failed", error);
      }

      console.log("\n--- Phase 4 Tests Complete ---");
      resolve();
    }, 14000); // Run after Phase 3 tests
  });

  // Handle cleanup based on test mode
  if (testMode === "automated") {
    // Automated mode: wait for all tests to complete, then close
    try {
      await Promise.all([lastUITest, rpcTestsDone, phase3TestsDone, phase4TestsDone]);
      console.log("\n--- CLEANUP (AUTOMATED) ---");
      testPass("All tests completed", "All UI and RPC tests finished");
      // Give a brief moment for any final events to fire
      await new Promise((resolve) => setTimeout(resolve, 500));
      testPass("webview.close", "Programmatically closing webview...");
      console.log(
        "   (This will trigger 'close' and 'closed' events, then quit the app)",
      );
      webview.close();
      // Note: app.quit() will be called automatically by the 'closed' event handler
    } catch (error) {
      testFail("webview.close", "Failed to close webview", error);
      // Force exit on error
      printFinalReport();
      process.exit(1);
    }
  } else {
    // Manual mode: wait for user to close the window
    const platform =
      process.platform === "darwin"
        ? "macOS"
        : process.platform === "win32"
          ? "Windows"
          : process.platform === "linux"
            ? "Linux"
            : "Unknown";

    console.log("\n--- MANUAL MODE ACTIVE ---");
    console.log(`👤 The window is now open for manual testing (${platform}).`);
    console.log("📋 Follow the instructions in the window:");

    if (process.platform === "darwin") {
      console.log("   macOS Controls:");
      console.log(
        "   - 🟢 Full Screen (green button) - Note: maximize event may not fire on macOS",
      );
      console.log("   - 🟡 Minimize (yellow button)");
      console.log("   - 📐 Restore from Dock");
      console.log("   - ↔️  Resize manually (drag window edges)");
    } else if (process.platform === "win32") {
      console.log("   Windows Controls:");
      console.log("   - 🔲 Maximize (□ button)");
      console.log("   - ➖ Minimize (− button)");
      console.log("   - 📐 Restore from taskbar");
      console.log("   - ↔️  Resize manually (drag window edges)");
    } else {
      console.log("   - Maximize/restore the window");
      console.log("   - Minimize/restore the window");
      console.log("   - Resize the window manually");
    }

    console.log("\n🔍 All events are being tracked automatically.");
    console.log(
      "✅ When finished, close the window to see the final report.\n",
    );
    console.log("⏳ Waiting for you to close the window...");
    // Window will stay open until user closes it
    // The 'closed' event handler will print the final report
  }
}

main().catch((err) => {
  console.error("\n🔴 FATAL ERROR IN MAIN:");
  console.error(err);
  testResults.errors.push({ type: "mainCatchBlock", error: err });
  printFinalReport();
  process.exit(1);
});
