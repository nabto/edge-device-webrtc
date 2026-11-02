// Connecting to devices, pairing them and running WebRTC signaling sessions.
// The RTCPeerConnection itself lives in the renderer (Chromium's WebRTC stack);
// this side owns the Nabto connection and relays signaling messages.

import { randomUUID } from 'crypto';
import type { Connection, NabtoClient } from './nabto/client';
import { NabtoClient as Client } from './nabto/client';
import { SignalingChannel } from './signaling';
import { getMe, getPairingInfo, pair } from './iam';
import type { Store } from './store';
import type { Bookmark, CoapResult, PairResult, PairingInput, SessionInfo, SignalMessage } from '../shared/types';

const CF_JSON = 50;

interface Target {
  productId: string;
  deviceId: string;
  sct?: string;
  deviceFingerprint?: string;
}

interface Session {
  connection: Connection;
  signaling: SignalingChannel;
}

export interface SessionEvents {
  onSignal(sessionId: string, msg: SignalMessage): void;
  onClosed(sessionId: string, reason: string): void;
}

export class DeviceService {
  private readonly sessions = new Map<string, Session>();

  constructor(private readonly client: NabtoClient, private readonly store: Store,
              private readonly privateKey: string) {}

  /// Connect and verify the device fingerprint when one is known.
  private async connect(target: Target): Promise<Connection> {
    const connection = this.client.createConnection();
    try {
      connection.setOptions({
        ProductId: target.productId,
        DeviceId: target.deviceId,
        PrivateKey: this.privateKey,
        ServerConnectToken: target.sct,
      });
      await connection.connect();
      const fp = connection.deviceFingerprint();
      if (target.deviceFingerprint && target.deviceFingerprint.toLowerCase() !== fp) {
        throw new Error(`Device identity mismatch. Expected fingerprint ${target.deviceFingerprint.slice(0, 16)}..., ` +
          `the device presented ${fp.slice(0, 16)}...`);
      }
      return connection;
    } catch (err) {
      await connection.close();
      throw err;
    }
  }

  async pair(input: PairingInput): Promise<PairResult> {
    const connection = await this.connect(input);
    try {
      const deviceFingerprint = connection.deviceFingerprint();
      const info = await getPairingInfo(connection);
      const bookmark: Bookmark = {
        id: `${input.productId}.${input.deviceId}`,
        productId: input.productId,
        deviceId: input.deviceId,
        sct: input.sct,
        deviceFingerprint,
        pairedAt: new Date().toISOString(),
      };
      let mode = null;
      if (info) {
        mode = await pair(connection, input, info);
        const me = await getMe(connection);
        bookmark.username = me?.username;
        bookmark.role = me?.role;
        // The device may have a per user SCT which replaces the one from the pairing string.
        bookmark.sct = me?.sct ?? input.sct;
        bookmark.name = info.friendlyName || info.appName;
      } else {
        bookmark.noIam = true;
      }
      this.store.saveBookmark(bookmark);
      return { bookmark, mode };
    } finally {
      await connection.close();
    }
  }

  async startSession(bookmarkId: string, events: SessionEvents): Promise<SessionInfo> {
    const bookmark = this.store.bookmark(bookmarkId);
    if (!bookmark) {
      throw new Error(`Unknown device ${bookmarkId}`);
    }
    const connection = await this.connect(bookmark);
    let signaling: SignalingChannel;
    try {
      signaling = await SignalingChannel.open(connection);
    } catch (err) {
      await connection.close();
      throw err;
    }
    const sessionId = randomUUID();
    this.sessions.set(sessionId, { connection, signaling });
    void signaling.readLoop((msg) => events.onSignal(sessionId, msg)).then(async (reason) => {
      if (this.sessions.has(sessionId)) {
        await this.stopSession(sessionId);
        events.onClosed(sessionId, reason);
      }
    });
    return {
      sessionId,
      connectionType: connection.type(),
      clientFingerprint: connection.clientFingerprint(),
      deviceFingerprint: connection.deviceFingerprint(),
      sdkVersion: Client.version(),
    };
  }

  async send(sessionId: string, msg: SignalMessage): Promise<void> {
    await this.session(sessionId).signaling.send(msg);
  }

  /// CoAP request on the session's Nabto connection. JSON in, JSON/text out.
  async coap(sessionId: string, method: 'GET' | 'POST', path: string, json?: unknown): Promise<CoapResult> {
    const body = json === undefined ? undefined
      : { contentFormat: CF_JSON, payload: Buffer.from(JSON.stringify(json), 'utf8') };
    const res = await this.session(sessionId).connection.coap(method, path, body);
    const isText = res.contentFormat === null || res.contentFormat === 0 || res.contentFormat === CF_JSON;
    return {
      status: res.status,
      contentFormat: res.contentFormat,
      text: isText ? res.payload.toString('utf8') : res.payload.toString('base64'),
    };
  }

  connectionType(sessionId: string): 'relay' | 'direct' | null {
    const session = this.sessions.get(sessionId);
    return session ? session.connection.type() : null;
  }

  async stopSession(sessionId: string): Promise<void> {
    const session = this.sessions.get(sessionId);
    if (!session) {
      return;
    }
    this.sessions.delete(sessionId);
    await session.signaling.close();
    await session.connection.close();
  }

  async stopAll(): Promise<void> {
    await Promise.all([...this.sessions.keys()].map((id) => this.stopSession(id)));
  }

  private session(sessionId: string): Session {
    const session = this.sessions.get(sessionId);
    if (!session) {
      throw new Error('The session is closed');
    }
    return session;
  }
}
