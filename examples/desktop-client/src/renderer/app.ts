// Renderer UI: device list, pairing form and device view.

import { parsePairingString } from '../shared/pairing-string.js';
import type { Bookmark, DesktopApi, DiscoveredDevice, PairingInput } from '../shared/types.js';
import { SessionState, SessionStats, WebRtcSession } from './session.js';

declare global {
  interface Window {
    edge: DesktopApi;
  }
}

const api = window.edge;

function $(id: string): HTMLElement {
  const el = document.getElementById(id);
  if (!el) {
    throw new Error(`#${id} missing`);
  }
  return el;
}

function role<T extends HTMLElement = HTMLElement>(root: ParentNode, name: string): T {
  const el = root.querySelector<T>(`[data-role="${name}"]`);
  if (!el) {
    throw new Error(`[data-role=${name}] missing`);
  }
  return el;
}

function fromTemplate(id: string): DocumentFragment {
  return ($(id) as HTMLTemplateElement).content.cloneNode(true) as DocumentFragment;
}

function el<K extends keyof HTMLElementTagNameMap>(tag: K, className?: string, text?: string): HTMLElementTagNameMap[K] {
  const e = document.createElement(tag);
  if (className) {
    e.className = className;
  }
  if (text !== undefined) {
    e.textContent = text;
  }
  return e;
}

function shortFp(fp?: string): string {
  return fp ? `${fp.slice(0, 8)}…${fp.slice(-8)}` : '—';
}

// ---------------------------------------------------------------------------
// State

let bookmarks: Bookmark[] = [];
const discovered = new Map<string, DiscoveredDevice>();
let selected: { kind: 'welcome' } | { kind: 'add'; prefill?: Partial<PairingInput> } | { kind: 'device'; id: string } = { kind: 'welcome' };
let activeView: DeviceView | null = null;

function deviceKey(d: { productId: string; deviceId: string }): string {
  return `${d.productId}.${d.deviceId}`;
}

// ---------------------------------------------------------------------------
// Sidebar

function renderSidebar(): void {
  const list = $('bookmarks');
  list.replaceChildren(...bookmarks
    .slice()
    .sort((a, b) => (a.name ?? a.deviceId).localeCompare(b.name ?? b.deviceId))
    .map((b) => {
      const li = el('li', selected.kind === 'device' && selected.id === b.id ? 'selected' : '');
      const online = discovered.has(b.id);
      li.append(
        el('span', `dot ${online ? 'online' : ''}`),
        el('span', 'title', b.name || b.deviceId),
        el('span', 'sub', `${b.productId}.${b.deviceId}${b.role ? ` · ${b.role}` : ''}`),
      );
      li.title = online ? 'Found on the local network' : '';
      li.onclick = () => select({ kind: 'device', id: b.id });
      return li;
    }));
  $('bookmarks-empty').hidden = bookmarks.length > 0;

  const unpaired = [...discovered.values()].filter((d) => !bookmarks.some((b) => b.id === deviceKey(d)));
  $('discovered').replaceChildren(...unpaired.map((d) => {
    const li = el('li');
    li.append(
      el('span', 'dot online'),
      el('span', 'title', d.txtItems.fn || d.deviceId),
      el('span', 'sub', `${d.productId}.${d.deviceId} · tap to pair`),
    );
    li.onclick = () => select({ kind: 'add', prefill: { productId: d.productId, deviceId: d.deviceId } });
    return li;
  }));
  $('discovered-empty').hidden = unpaired.length > 0;
}

async function reloadBookmarks(): Promise<void> {
  bookmarks = await api.devices.list();
  renderSidebar();
}

function select(next: typeof selected): void {
  if (activeView && !(next.kind === 'device' && selected.kind === 'device' && next.id === selected.id)) {
    void activeView.dispose();
    activeView = null;
  }
  selected = next;
  renderSidebar();
  const content = $('content');
  if (next.kind === 'welcome') {
    content.replaceChildren(fromTemplate('tpl-welcome'));
  } else if (next.kind === 'add') {
    content.replaceChildren(buildAddForm(next.prefill));
  } else if (!activeView) {
    const bookmark = bookmarks.find((b) => b.id === next.id);
    if (!bookmark) {
      select({ kind: 'welcome' });
      return;
    }
    activeView = new DeviceView(bookmark);
    content.replaceChildren(activeView.root);
  }
}

// ---------------------------------------------------------------------------
// Add / pair

function buildAddForm(prefill?: Partial<PairingInput>): DocumentFragment {
  const frag = fromTemplate('tpl-add');
  const form = frag.querySelector('form')!;
  const input = (name: string) => form.elements.namedItem(name) as HTMLInputElement | HTMLTextAreaElement;
  const fields = ['productId', 'deviceId', 'username', 'password', 'sct', 'deviceFingerprint'] as const;
  const parseError = role(form, 'parse-error');
  const status = role(form, 'status');
  const details = role<HTMLDetailsElement>(form, 'details');
  const pairButton = role<HTMLButtonElement>(form, 'pair');

  for (const f of fields) {
    input(f).value = (prefill?.[f] as string | undefined) ?? '';
  }
  if (prefill?.productId) {
    details.open = true;
  }

  input('pairingString').addEventListener('input', () => {
    const raw = input('pairingString').value.trim();
    parseError.hidden = true;
    if (!raw) {
      return;
    }
    try {
      const parsed = parsePairingString(raw);
      for (const f of fields) {
        input(f).value = parsed[f] ?? '';
      }
    } catch (err) {
      parseError.textContent = (err as Error).message;
      parseError.hidden = false;
    }
  });

  form.addEventListener('submit', async (e) => {
    e.preventDefault();
    const pairing: PairingInput = {
      productId: input('productId').value.trim(),
      deviceId: input('deviceId').value.trim(),
    };
    for (const f of ['username', 'password', 'sct', 'deviceFingerprint'] as const) {
      const v = input(f).value.trim();
      if (v) {
        pairing[f] = v;
      }
    }
    const desired = input('desiredUsername').value.trim();
    if (desired) {
      pairing.desiredUsername = desired;
    }
    if (!pairing.productId || !pairing.deviceId) {
      details.open = true;
      status.className = 'status small error';
      status.textContent = 'Product ID and Device ID are required.';
      return;
    }

    pairButton.disabled = true;
    status.className = 'status small';
    status.textContent = `Connecting to ${pairing.productId}.${pairing.deviceId}…`;
    const res = await api.pairing.pair(pairing);
    pairButton.disabled = false;
    if (!res.ok || !res.value) {
      status.className = 'status small error';
      status.textContent = res.error ?? 'Pairing failed';
      return;
    }
    const { bookmark, mode } = res.value;
    await reloadBookmarks();
    select({ kind: 'device', id: bookmark.id });
    activeView?.log(mode
      ? `Paired as ${bookmark.username} (${bookmark.role}) using ${mode} pairing`
      : bookmark.noIam ? 'Device has no IAM, no pairing needed' : `Already paired as ${bookmark.username} (${bookmark.role})`);
  });
  return frag;
}

// ---------------------------------------------------------------------------
// Device view

class DeviceView {
  readonly root: HTMLElement;
  private session: WebRtcSession | null = null;
  private statsTimer: number | undefined;
  private readonly video: HTMLVideoElement;
  private readonly audio: HTMLAudioElement;
  private readonly audioToggle: HTMLButtonElement;
  private readonly overlay: HTMLElement;
  private readonly connectButton: HTMLButtonElement;
  private readonly tracksInput: HTMLInputElement;
  private readonly stateBadge: HTMLElement;
  private readonly diag: HTMLElement;
  private readonly logList: HTMLElement;
  private readonly trackErrors = new Map<string, string>();
  private receivedTracks: string[] = [];
  private lastStats: SessionStats | null = null;

  constructor(private readonly bookmark: Bookmark) {
    const frag = fromTemplate('tpl-device');
    this.root = frag.firstElementChild as HTMLElement;
    this.video = role(this.root, 'video');
    this.audio = role(this.root, 'audio');
    this.audioToggle = role(this.root, 'audio-toggle');
    this.overlay = role(this.root, 'overlay');
    this.connectButton = role(this.root, 'connect');
    this.tracksInput = role(this.root, 'tracks');
    this.stateBadge = role(this.root, 'state');
    this.diag = role(this.root, 'diag');
    this.logList = role(this.root, 'log');

    role(this.root, 'name').textContent = bookmark.name || bookmark.deviceId;
    role(this.root, 'ids').textContent =
      `${bookmark.productId}.${bookmark.deviceId}${bookmark.username ? ` · ${bookmark.username} (${bookmark.role ?? 'no role'})` : ''}`;

    this.connectButton.onclick = () => (this.session ? void this.disconnect() : void this.connect());
    this.audioToggle.onclick = () => {
      this.audio.muted = !this.audio.muted;
      this.updateAudioToggle();
    };
    role(this.root, 'remove').onclick = async () => {
      if (confirm(`Remove ${bookmark.name || bookmark.deviceId}? The device keeps its user; pair again to re-add it.`)) {
        await this.dispose();
        await api.devices.remove(bookmark.id);
        activeView = null;
        await reloadBookmarks();
        select({ kind: 'welcome' });
      }
    };
    this.renderDiagnostics();
  }

  log(message: string): void {
    const li = el('li');
    li.append(el('span', 'muted', new Date().toLocaleTimeString()), document.createTextNode(` ${message}`));
    this.logList.append(li);
    while (this.logList.children.length > 200) {
      this.logList.firstElementChild?.remove();
    }
    this.logList.scrollTop = this.logList.scrollHeight;
  }

  private setState(state: SessionState | 'idle', detail?: string): void {
    this.stateBadge.textContent = state;
    this.stateBadge.className = `badge ${state}`;
    this.connectButton.textContent = state === 'connecting' || state === 'negotiating' || state === 'connected' ? 'Disconnect' : 'Connect';
    this.tracksInput.disabled = this.session !== null;
    if (state === 'connected') {
      this.overlay.hidden = this.video.srcObject !== null;
      this.overlay.textContent = 'Waiting for video…';
    } else if (state === 'failed') {
      this.overlay.hidden = false;
      this.overlay.textContent = detail ?? 'Connection failed';
    } else if (state === 'closed' || state === 'idle') {
      this.overlay.hidden = false;
      this.overlay.textContent = 'Not connected';
    } else {
      this.overlay.hidden = false;
      this.overlay.textContent = state === 'connecting' ? 'Connecting…' : 'Negotiating WebRTC…';
    }
  }

  private async connect(): Promise<void> {
    this.trackErrors.clear();
    this.receivedTracks = [];
    this.lastStats = null;
    const raw = this.tracksInput.value.trim();
    const tracks = raw ? raw.split(/[\s,]+/).filter(Boolean) : 'default';
    const session = new WebRtcSession(api, this.bookmark.id, tracks, {
      onState: (state, detail) => {
        this.setState(state, detail);
        if (state === 'closed' || state === 'failed') {
          this.cleanupSession();
        }
      },
      onTrack: (trackId, track) => this.attachTrack(trackId, track),
      onTrackError: (trackId, error) => {
        this.trackErrors.set(trackId, error);
        this.log(`Track ${trackId}: ${error}`);
        this.renderDiagnostics();
      },
      onLog: (m) => this.log(m),
    });
    this.session = session;
    this.setState('connecting');
    this.statsTimer = window.setInterval(() => void this.pollStats(), 1000);
    await session.start();
  }

  private async disconnect(): Promise<void> {
    await this.session?.stop();
  }

  private cleanupSession(): void {
    window.clearInterval(this.statsTimer);
    this.statsTimer = undefined;
    this.session = null;
    this.video.srcObject = null;
    this.audio.srcObject = null;
    this.audioToggle.hidden = true;
    this.tracksInput.disabled = false;
    this.renderDiagnostics();
  }

  private attachTrack(trackId: string, track: MediaStreamTrack): void {
    this.receivedTracks.push(`${trackId} (${track.kind})`);
    if (track.kind === 'video') {
      this.video.srcObject = new MediaStream([track]);
      this.video.onloadeddata = () => {
        this.overlay.hidden = true;
      };
    } else {
      this.audio.srcObject = new MediaStream([track]);
      this.audioToggle.hidden = false;
      this.updateAudioToggle();
    }
    this.renderDiagnostics();
  }

  private updateAudioToggle(): void {
    this.audioToggle.textContent = this.audio.muted ? 'Unmute audio' : 'Mute audio';
  }

  private async pollStats(): Promise<void> {
    if (!this.session) {
      return;
    }
    try {
      this.lastStats = await this.session.stats();
    } catch {
      // the session may have closed between the timer firing and the call
    }
    this.renderDiagnostics();
  }

  private renderDiagnostics(): void {
    const info = this.session?.info;
    const s = this.lastStats;
    const rows: Array<[string, string]> = [
      ['Nabto connection', s?.connectionType ?? info?.connectionType ?? '—'],
      ['ICE path', s?.iceCandidatePair ?? '—'],
      ['Device fingerprint', shortFp(info?.deviceFingerprint ?? this.bookmark.deviceFingerprint)],
      ['Client fingerprint', shortFp(info?.clientFingerprint)],
      ['User / role', this.bookmark.username ? `${this.bookmark.username} / ${this.bookmark.role ?? '—'}` : this.bookmark.noIam ? 'no IAM on device' : '—'],
      ['Tracks received', this.receivedTracks.join(', ') || '—'],
    ];
    for (const [trackId, error] of this.trackErrors) {
      rows.push([`⚠ ${trackId}`, error]);
    }
    if (s?.video) {
      const v = s.video;
      rows.push(
        ['Video codec', v.codec],
        ['Resolution / fps', `${v.resolution} @ ${v.fps.toFixed(0)} fps`],
        ['Video bitrate', `${v.bitrateKbps} kbit/s`],
        ['Frames decoded / dropped', `${v.framesDecoded} / ${v.framesDropped} (key ${v.keyFramesDecoded})`],
        ['Video packets / lost', `${v.packetsReceived} / ${v.packetsLost}`],
        ['Video jitter / PLIs', `${v.jitterMs} ms / ${v.pliCount}`],
      );
    }
    if (s?.audio) {
      rows.push(
        ['Audio codec', s.audio.codec],
        ['Audio packets / lost', `${s.audio.packetsReceived} / ${s.audio.packetsLost}`],
      );
    }
    if (info) {
      rows.push(['Client SDK', info.sdkVersion]);
    }
    this.diag.replaceChildren(...rows.flatMap(([k, v]) => [el('dt', '', k), el('dd', 'mono', v)]));
  }

  async dispose(): Promise<void> {
    await this.session?.stop();
    this.cleanupSession();
  }
}

// ---------------------------------------------------------------------------
// Startup

async function main(): Promise<void> {
  $('sdk-version').textContent = `Nabto Client SDK ${await api.sdkVersion()}`;
  $('add-device').onclick = () => select({ kind: 'add' });

  api.mdns.onResult((action, device) => {
    const key = deviceKey(device);
    if (action === 'remove') {
      discovered.delete(key);
    } else {
      discovered.set(key, device);
    }
    renderSidebar();
  });
  await api.mdns.start();
  await reloadBookmarks();
  select(bookmarks.length > 0 ? { kind: 'device', id: bookmarks[0].id } : { kind: 'welcome' });
}

void main();
