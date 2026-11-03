// Exposes the DesktopApi to the sandboxed renderer as window.edge.

import { contextBridge, ipcRenderer, IpcRendererEvent } from 'electron';
import type { DesktopApi, DiscoveredDevice, SignalMessage } from '../shared/types';

function subscribe<A extends unknown[]>(channel: string, cb: (...args: A) => void): () => void {
  const listener = (_e: IpcRendererEvent, ...args: unknown[]) => cb(...(args as A));
  ipcRenderer.on(channel, listener);
  return () => ipcRenderer.removeListener(channel, listener);
}

const api: DesktopApi = {
  sdkVersion: () => ipcRenderer.invoke('sdk:version'),

  devices: {
    list: () => ipcRenderer.invoke('devices:list'),
    remove: (id) => ipcRenderer.invoke('devices:remove', id),
  },

  pairing: {
    pair: (input) => ipcRenderer.invoke('pairing:pair', input),
  },

  mdns: {
    start: () => ipcRenderer.invoke('mdns:start'),
    stop: () => ipcRenderer.invoke('mdns:stop'),
    onResult: (cb) => subscribe<['add' | 'update' | 'remove', DiscoveredDevice]>('mdns:result', cb),
  },

  session: {
    start: (bookmarkId) => ipcRenderer.invoke('session:start', bookmarkId),
    send: (sessionId, msg) => ipcRenderer.invoke('session:send', sessionId, msg),
    coap: (sessionId, method, path, json) => ipcRenderer.invoke('session:coap', sessionId, method, path, json),
    connectionType: (sessionId) => ipcRenderer.invoke('session:connectionType', sessionId),
    stop: (sessionId) => ipcRenderer.invoke('session:stop', sessionId),
    onSignal: (cb) => subscribe<[string, SignalMessage]>('session:signal', cb),
    onClosed: (cb) => subscribe<[string, string]>('session:closed', cb),
  },
};

contextBridge.exposeInMainWorld('edge', api);
