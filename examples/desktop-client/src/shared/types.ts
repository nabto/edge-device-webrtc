// Types shared between the Electron main process and the renderer.

// ---------------------------------------------------------------------------
// Signaling protocol spoken with the device over a Nabto stream. See
// src/library/signaling-stream/signaling_stream.cpp in edge-device-webrtc.

export enum SignalMessageType {
  Offer = 0,
  Answer = 1,
  IceCandidate = 2,
  TurnRequest = 3,
  TurnResponse = 4,
}

export interface SignalTrackMetadata {
  mid: string;
  trackId: string;
  /** Set by the device: ACCESS_DENIED | UNKNOWN_TRACK_ID | INVALID_CODECS | UNKNOWN_ERROR */
  error?: string;
}

export interface SignalMetadata {
  tracks?: SignalTrackMetadata[];
  noTrickle?: boolean;
  /** Set by the device: OK | FAILED */
  status?: string;
}

/** Deprecated TURN server format, still sent by the device next to iceServers. */
export interface LegacyTurnServer {
  hostname: string;
  port: number;
  username: string;
  password: string;
}

export interface SignalIceServer {
  urls: string[];
  username?: string;
  credential?: string;
}

export interface SignalMessage {
  type: SignalMessageType;
  /** JSON encoded string: {type, sdp} for offers/answers, {candidate, sdpMid} for ICE candidates. */
  data?: string;
  metadata?: SignalMetadata;
  servers?: LegacyTurnServer[];
  iceServers?: SignalIceServer[];
}

// ---------------------------------------------------------------------------
// Devices and pairing

export interface PairingDetails {
  productId: string;
  deviceId: string;
  username?: string;
  password?: string;
  sct?: string;
  deviceFingerprint?: string;
}

export interface PairingInput extends PairingDetails {
  /** Username to request on the device for open pairing modes. */
  desiredUsername?: string;
}

export type PairingMode = 'LocalInitial' | 'LocalOpen' | 'PasswordOpen' | 'PasswordInvite';

/** A device this client has paired with (or which needs no pairing). */
export interface Bookmark {
  id: string;
  productId: string;
  deviceId: string;
  sct?: string;
  deviceFingerprint?: string;
  name?: string;
  username?: string;
  role?: string;
  /** True when the device has no IAM module (e.g. the simple-webrtc example). */
  noIam?: boolean;
  pairedAt: string;
}

export interface PairResult {
  bookmark: Bookmark;
  /** Pairing mode used, or null when already paired or the device has no IAM. */
  mode: PairingMode | null;
}

export interface DiscoveredDevice {
  productId: string;
  deviceId: string;
  serviceInstanceName: string;
  txtItems: Record<string, string>;
}

// ---------------------------------------------------------------------------
// WebRTC session (Nabto side lives in main, RTCPeerConnection in the renderer)

export interface SessionInfo {
  sessionId: string;
  connectionType: 'relay' | 'direct';
  clientFingerprint: string;
  deviceFingerprint: string;
  sdkVersion: string;
}

export interface CoapResult {
  status: number;
  contentFormat: number | null;
  /** Response payload decoded as UTF-8 for JSON/text, otherwise base64. */
  text: string;
}

export interface Result<T> {
  ok: boolean;
  value?: T;
  error?: string;
}

/** API exposed to the renderer by the preload script as window.edge. */
export interface DesktopApi {
  sdkVersion(): Promise<string>;

  devices: {
    list(): Promise<Bookmark[]>;
    remove(id: string): Promise<void>;
  };

  pairing: {
    pair(input: PairingInput): Promise<Result<PairResult>>;
  };

  mdns: {
    start(): Promise<void>;
    stop(): Promise<void>;
    onResult(cb: (action: 'add' | 'update' | 'remove', device: DiscoveredDevice) => void): () => void;
  };

  session: {
    start(bookmarkId: string): Promise<Result<SessionInfo>>;
    send(sessionId: string, msg: SignalMessage): Promise<void>;
    coap(sessionId: string, method: 'GET' | 'POST', path: string, json?: unknown): Promise<Result<CoapResult>>;
    connectionType(sessionId: string): Promise<'relay' | 'direct' | null>;
    stop(sessionId: string): Promise<void>;
    onSignal(cb: (sessionId: string, msg: SignalMessage) => void): () => void;
    onClosed(cb: (sessionId: string, reason: string) => void): () => void;
  };
}
