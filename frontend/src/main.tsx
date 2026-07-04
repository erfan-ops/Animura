/**
 * @file main.tsx
 * @brief React application entry point.
 *
 * Mounts the App component into the DOM and loads design tokens.
 * Suppresses the default browser right-click context menu because
 * the app runs inside WebView2 as a desktop application, not a web page.
 */

import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';
import './theme/tokens.css';

// Suppress the browser-style right-click context menu.
// The app runs as a desktop application inside WebView2; a browser
// context menu is inappropriate. WebView2-level context menu suppression
// is also configured in WebView2Host::initWebView2().
document.addEventListener('contextmenu', (e) => e.preventDefault());

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);
