// Electron main process: window setup and IPC between the renderer UI and the
// Nabto Edge Client SDK.

import path from 'path';
import { BrowserWindow, app, ipcMain } from 'electron';
import { MdnsScanner, NabtoClient } from './nabto/client';
import { DeviceService } from './devices';
import { Store } from './store';
import type { PairingInput, Result, SignalMessage } from '../shared/types';

let window: BrowserWindow | null = null;
let client: NabtoClient | null = null;
let devices: DeviceService | null = null;
let scanner: MdnsScanner | null = null;

function send(channel: string, ...args: unknown[]): void {
  if (window && !window.isDestroyed()) {
    window.webContents.send(channel, ...args);
  }
}

/// Run `fn` and report errors as a Result instead of a rejected IPC call, so the
/// renderer gets the plain error message.
async function result<T>(fn: () => Promise<T>): Promise<Result<T>> {
  try {
    return { ok: true, value: await fn() };
  } catch (err) {
    return { ok: false, error: err instanceof Error ? err.message : String(err) };
  }
}

function services(): { client: NabtoClient; devices: DeviceService } {
  if (!client || !devices) {
    throw new Error('Nabto client is not initialized');
  }
  return { client, devices };
}

function registerIpc(store: Store): void {
  ipcMain.handle('sdk:version', () => NabtoClient.version());

  ipcMain.handle('devices:list', () => store.bookmarks());
  ipcMain.handle('devices:remove', (_e, id: string) => store.removeBookmark(id));

  ipcMain.handle('pairing:pair', (_e, input: PairingInput) => result(() => services().devices.pair(input)));

  ipcMain.handle('mdns:start', () => {
    if (scanner) {
      return;
    }
    scanner = services().client.createMdnsScanner();
    scanner.start(
      (r) => send('mdns:result', r.action, {
        productId: r.productId,
        deviceId: r.deviceId,
        serviceInstanceName: r.serviceInstanceName,
        txtItems: r.txtItems,
      }),
      (err) => console.error(`mDNS scan failed: ${err.message}`),
    );
  });
  ipcMain.handle('mdns:stop', () => {
    scanner?.stop();
    scanner = null;
  });

  ipcMain.handle('session:start', (_e, bookmarkId: string) => result(() => services().devices.startSession(bookmarkId, {
    onSignal: (sessionId, msg) => send('session:signal', sessionId, msg),
    onClosed: (sessionId, reason) => send('session:closed', sessionId, reason),
  })));
  ipcMain.handle('session:send', (_e, sessionId: string, msg: SignalMessage) => services().devices.send(sessionId, msg));
  ipcMain.handle('session:coap', (_e, sessionId: string, method: 'GET' | 'POST', path: string, json?: unknown) =>
    result(() => services().devices.coap(sessionId, method, path, json)));
  ipcMain.handle('session:connectionType', (_e, sessionId: string) => services().devices.connectionType(sessionId));
  ipcMain.handle('session:stop', (_e, sessionId: string) => services().devices.stopSession(sessionId));
}

function createWindow(): void {
  window = new BrowserWindow({
    width: 1280,
    height: 820,
    minWidth: 900,
    minHeight: 600,
    title: 'Nabto Edge WebRTC Client',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      sandbox: true,
      nodeIntegration: false,
      // Remote audio should start playing without a click on the page first.
      autoplayPolicy: 'no-user-gesture-required',
    },
  });
  window.removeMenu();
  void window.loadFile(path.join(__dirname, '..', '..', 'static', 'index.html'));
  // Navigation away from the app page is never expected.
  window.webContents.on('will-navigate', (e) => e.preventDefault());
  window.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));
  window.on('closed', () => {
    window = null;
  });
}

let shuttingDown = false;

async function shutdown(): Promise<void> {
  scanner?.stop();
  scanner = null;
  await devices?.stopAll();
  await client?.close();
  client = null;
  devices = null;
}

app.whenReady().then(async () => {
  const store = new Store(app.getPath('userData'));
  const nabto = new NabtoClient();
  client = nabto;
  if (process.env.NABTO_LOG_LEVEL) {
    nabto.setLogLevel(process.env.NABTO_LOG_LEVEL as 'error' | 'warn' | 'info' | 'trace');
  }
  const key = await store.privateKey(() => nabto.createPrivateKey());
  devices = new DeviceService(nabto, store, key);
  registerIpc(store);
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
}).catch((err) => {
  console.error(err);
  app.exit(1);
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

// Close Nabto connections gracefully (so devices see the peer go away) before
// the process exits.
app.on('before-quit', (e) => {
  if (shuttingDown || !client) {
    return;
  }
  e.preventDefault();
  shuttingDown = true;
  shutdown().finally(() => app.quit());
});
