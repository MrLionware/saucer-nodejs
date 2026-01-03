import { Application, Webview } from '../index.js';

// Create application
const app = new Application({
  threads: 4
});

console.log('Application created');
console.log('Thread safe:', app.isThreadSafe());

// Create webview
const webview = new Webview(app, {
  hardwareAcceleration: true,
  persistentCookies: false
});

console.log('Webview created');

// Set up the window
webview.title = 'Saucer Node.js Example';
webview.size = { width: 1024, height: 768 };

// Navigate to a website
webview.navigate('https://browserbench.org/Speedometer3.1/');

// Show the window
webview.show();

console.log('Window shown');
console.log('Current URL:', webview.url);

// Execute some JavaScript after a delay
setTimeout(() => {
  webview.execute(`
    console.log('Hello from Node.js!');
  `);
  console.log('Executed JavaScript in webview');
}, 2000);

// Handle close event
webview.on('closed', () => {
  console.log('Window closed, quitting application');
  app.quit();
});

// Keep Node.js process running
// The event loop integration allows Node.js to continue processing
// while the webview is running
console.log('Event loop integration active - Node.js and webview running concurrently');

// Example: You can still use Node.js timers and async operations
setInterval(() => {
  console.log('Node.js event loop is still running!');
}, 5000);
