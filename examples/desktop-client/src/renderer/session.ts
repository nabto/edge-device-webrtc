// One WebRTC session with a device. The Nabto connection and signaling stream
// live in the main process; this class runs the RTCPeerConnection.
//
// Flow (see src/library/webrtc-connection/webrtc_connection.cpp in edge-device-webrtc):
//   1. main opens the Nabto connection and the signaling stream
//   2. we send a TURN request, the device answers with its ICE servers
//   3. we create the RTCPeerConnection and POST /webrtc/tracks
//   4. the device sends an offer with metadata mapping mid -> trackId
//   5. we answer, echoing the track metadata back (the device replaces its
//      metadata with whatever we send), and ICE candidates trickle both ways
//
// The device is the "impolite" peer of the perfect negotiation pattern, so
// this side always yields: an incoming offer is applied even if we were in
// the middle of something else.

import { SignalMessageType } from '../shared/types.js';
import type { DesktopApi, SessionInfo, SignalIceServer, SignalMessage, SignalTrackMetadata } from '../shared/types.js';

export type SessionState = 'connecting' | 'negotiating' | 'connected' | 'closed' | 'failed';

export interface SessionHandlers {
  onState(state: SessionState, detail?: string): void;
  onTrack(trackId: string, track: MediaStreamTrack): void;
  onTrackError(trackId: string, error: string): void;
  onLog(message: string): void;
}

export interface SessionStats {
  connectionType: string;
  iceCandidatePair: string;
  video?: {
    codec: string;
    resolution: string;
    fps: number;
    framesDecoded: number;
    framesDropped: number;
    keyFramesDecoded: number;
    packetsReceived: number;
    packetsLost: number;
    jitterMs: number;
    bitrateKbps: number;
    pliCount: number;
  };
  audio?: {
    codec: string;
    packetsReceived: number;
    packetsLost: number;
    jitterMs: number;
  };
}

const TRACK_ERRORS: Record<string, string> = {
  ACCESS_DENIED: 'access denied (the user role lacks Webrtc:VideoStream)',
  UNKNOWN_TRACK_ID: 'the device has no track with this id',
  INVALID_CODECS: 'no codec in common with the device',
  UNKNOWN_ERROR: 'unknown error on the device',
};

export class WebRtcSession {
  info: SessionInfo | null = null;
  private pc: RTCPeerConnection | null = null;
  private readonly midToTrack = new Map<string, string>();
  private deviceTracks: SignalTrackMetadata[] = [];
  private queue: Promise<void> = Promise.resolve();
  private unsubscribers: Array<() => void> = [];
  private closed = false;
  private lastVideoBytes = { bytes: 0, timestamp: 0 };

  constructor(private readonly api: DesktopApi, private readonly bookmarkId: string,
              private readonly tracks: string[] | 'default', private readonly handlers: SessionHandlers) {}

  async start(): Promise<void> {
    this.handlers.onState('connecting');
    const res = await this.api.session.start(this.bookmarkId);
    if (!res.ok || !res.value) {
      this.fail(res.error ?? 'Failed to start session');
      return;
    }
    this.info = res.value;
    const sessionId = this.info.sessionId;
    this.handlers.onLog(`Nabto connection established (${this.info.connectionType})`);

    this.unsubscribers.push(this.api.session.onSignal((id, msg) => {
      if (id === sessionId) {
        // Process messages strictly in order: an ICE candidate must not be
        // applied before the offer that precedes it has been.
        this.queue = this.queue.then(() => this.handleMessage(msg)).catch((err) => this.handlers.onLog(`Signaling error: ${err}`));
      }
    }));
    this.unsubscribers.push(this.api.session.onClosed((id, reason) => {
      if (id === sessionId && !this.closed) {
        this.handlers.onLog(`Signaling closed: ${reason}`);
        void this.stop('failed', reason);
      }
    }));

    this.handlers.onState('negotiating');
    this.handlers.onLog('Requesting ICE servers');
    await this.send({ type: SignalMessageType.TurnRequest });
  }

  async stop(state: SessionState = 'closed', detail?: string): Promise<void> {
    if (this.closed) {
      return;
    }
    this.closed = true;
    this.unsubscribers.forEach((u) => u());
    this.unsubscribers = [];
    this.pc?.close();
    this.pc = null;
    if (this.info) {
      await this.api.session.stop(this.info.sessionId);
    }
    this.handlers.onState(state, detail);
  }

  private fail(reason: string): void {
    this.handlers.onLog(reason);
    void this.stop('failed', reason);
  }

  private async send(msg: SignalMessage): Promise<void> {
    if (this.info && !this.closed) {
      await this.api.session.send(this.info.sessionId, msg);
    }
  }

  private async handleMessage(msg: SignalMessage): Promise<void> {
    if (this.closed) {
      return;
    }
    switch (msg.type) {
      case SignalMessageType.TurnResponse:
        if (!this.pc) {
          this.createPeerConnection(iceServersFrom(msg));
          await this.requestTracks();
        }
        break;
      case SignalMessageType.Offer:
      case SignalMessageType.Answer:
        await this.handleDescription(msg);
        break;
      case SignalMessageType.IceCandidate:
        if (this.pc && msg.data) {
          try {
            await this.pc.addIceCandidate(JSON.parse(msg.data) as RTCIceCandidateInit);
          } catch (err) {
            this.handlers.onLog(`Ignoring ICE candidate: ${err}`);
          }
        }
        break;
      default:
        this.handlers.onLog(`Ignoring signaling message of type ${msg.type}`);
    }
  }

  private createPeerConnection(iceServers: RTCIceServer[]): void {
    this.handlers.onLog(`Got ${iceServers.length} ICE server(s) from the device`);
    const pc = new RTCPeerConnection({ iceServers });
    this.pc = pc;

    pc.onicecandidate = (e) => {
      if (e.candidate && e.candidate.candidate) {
        void this.send({
          type: SignalMessageType.IceCandidate,
          data: JSON.stringify({ candidate: e.candidate.candidate, sdpMid: e.candidate.sdpMid }),
        });
      }
    };
    pc.ontrack = (e) => {
      const mid = e.transceiver.mid ?? '';
      const trackId = this.midToTrack.get(mid) ?? `mid ${mid}`;
      this.handlers.onLog(`Receiving ${e.track.kind} track ${trackId}`);
      this.handlers.onTrack(trackId, e.track);
    };
    pc.onconnectionstatechange = () => {
      this.handlers.onLog(`PeerConnection state: ${pc.connectionState}`);
      if (pc.connectionState === 'connected') {
        this.handlers.onState('connected');
      } else if (pc.connectionState === 'failed') {
        this.fail('The WebRTC connection failed (ICE could not find a working path to the device)');
      }
    };
  }

  private async requestTracks(): Promise<void> {
    const sessionId = this.info!.sessionId;
    let tracks = this.tracks;
    if (tracks === 'default') {
      const res = await this.api.session.coap(sessionId, 'GET', '/webrtc/tracks/default');
      if (res.ok && res.value?.status === 205) {
        const d = JSON.parse(res.value.text) as { videoTrackId?: string; audioTrackId?: string };
        tracks = [d.videoTrackId, d.audioTrackId].filter((t): t is string => !!t);
      } else {
        tracks = ['frontdoor-video', 'frontdoor-audio'];
      }
    }
    this.handlers.onLog(`POST /webrtc/tracks ${JSON.stringify(tracks)}`);
    const res = await this.api.session.coap(sessionId, 'POST', '/webrtc/tracks', { tracks });
    if (!res.ok) {
      this.fail(res.error ?? 'POST /webrtc/tracks failed');
      return;
    }
    const status = res.value!.status;
    if (status !== 201) {
      const reason = status === 401 || status === 403
        ? 'The device denied the tracks request. Is this client paired with a role that allows Webrtc:VideoStream?'
        : status === 404
          ? `The device does not know the requested tracks (${res.value!.text})`
          : `POST /webrtc/tracks failed with status ${status} ${res.value!.text}`;
      this.fail(reason);
    }
  }

  private async handleDescription(msg: SignalMessage): Promise<void> {
    const pc = this.pc;
    if (!pc || !msg.data) {
      return;
    }
    const description = JSON.parse(msg.data) as RTCSessionDescriptionInit;
    if (msg.metadata?.tracks) {
      this.deviceTracks = msg.metadata.tracks;
      for (const t of msg.metadata.tracks) {
        this.midToTrack.set(t.mid, t.trackId);
        if (t.error && t.error !== 'OK') {
          this.handlers.onTrackError(t.trackId, TRACK_ERRORS[t.error] ?? t.error);
        }
      }
    }
    if (msg.metadata?.status === 'FAILED') {
      this.handlers.onLog('The device reported that negotiation failed');
    }

    // Polite peer: setRemoteDescription performs an implicit rollback if we
    // have a pending local offer, so a colliding device offer always wins.
    await pc.setRemoteDescription(description);
    if (description.type === 'offer') {
      await pc.setLocalDescription();
      const local = pc.localDescription!;
      await this.send({
        type: SignalMessageType.Answer,
        data: JSON.stringify({ type: local.type, sdp: local.sdp }),
        metadata: { tracks: this.deviceTracks.map(({ mid, trackId }) => ({ mid, trackId })), noTrickle: false },
      });
    }
  }

  async stats(): Promise<SessionStats | null> {
    const pc = this.pc;
    if (!pc || !this.info) {
      return null;
    }
    const report = await pc.getStats();
    const connectionType = (await this.api.session.connectionType(this.info.sessionId)) ?? '—';
    const byId = new Map<string, Record<string, unknown>>();
    report.forEach((s) => byId.set(s.id, s as Record<string, unknown>));
    const codecName = (id: unknown) => {
      const c = typeof id === 'string' ? byId.get(id) : undefined;
      return c ? `${String(c.mimeType).replace(/^(video|audio)\//, '')}${c.payloadType !== undefined ? ` (pt ${c.payloadType})` : ''}` : '—';
    };

    const result: SessionStats = { connectionType, iceCandidatePair: '—' };
    report.forEach((s) => {
      const st = s as Record<string, number & string>;
      if (s.type === 'transport' && st.selectedCandidatePairId) {
        const pair = byId.get(st.selectedCandidatePairId);
        const local = pair && byId.get(pair.localCandidateId as string);
        const remote = pair && byId.get(pair.remoteCandidateId as string);
        if (local && remote) {
          result.iceCandidatePair = `${local.candidateType}/${local.protocol} → ${remote.candidateType} ${remote.address ?? ''}:${remote.port ?? ''}` +
            (pair?.currentRoundTripTime !== undefined ? `, rtt ${Math.round(Number(pair.currentRoundTripTime) * 1000)} ms` : '');
        }
      }
      if (s.type === 'inbound-rtp' && st.kind === 'video') {
        const bytes = Number(st.bytesReceived ?? 0);
        const ts = Number(s.timestamp);
        const prev = this.lastVideoBytes;
        const bitrateKbps = prev.timestamp && ts > prev.timestamp ? ((bytes - prev.bytes) * 8) / (ts - prev.timestamp) : 0;
        this.lastVideoBytes = { bytes, timestamp: ts };
        result.video = {
          codec: codecName(st.codecId),
          resolution: st.frameWidth ? `${st.frameWidth}×${st.frameHeight}` : '—',
          fps: Number(st.framesPerSecond ?? 0),
          framesDecoded: Number(st.framesDecoded ?? 0),
          framesDropped: Number(st.framesDropped ?? 0),
          keyFramesDecoded: Number(st.keyFramesDecoded ?? 0),
          packetsReceived: Number(st.packetsReceived ?? 0),
          packetsLost: Number(st.packetsLost ?? 0),
          jitterMs: Math.round(Number(st.jitter ?? 0) * 1000),
          bitrateKbps: Math.max(0, Math.round(bitrateKbps)),
          pliCount: Number(st.pliCount ?? 0),
        };
      }
      if (s.type === 'inbound-rtp' && st.kind === 'audio') {
        result.audio = {
          codec: codecName(st.codecId),
          packetsReceived: Number(st.packetsReceived ?? 0),
          packetsLost: Number(st.packetsLost ?? 0),
          jitterMs: Math.round(Number(st.jitter ?? 0) * 1000),
        };
      }
    });
    return result;
  }
}

/// Use the device's iceServers list, falling back to the deprecated servers field.
function iceServersFrom(msg: SignalMessage): RTCIceServer[] {
  if (msg.iceServers && msg.iceServers.length > 0) {
    return msg.iceServers.map((s: SignalIceServer) => ({ urls: s.urls, username: s.username, credential: s.credential }));
  }
  return (msg.servers ?? []).map((s) => ({
    urls: [s.hostname.includes(':') ? s.hostname : `turn:${s.hostname}:${s.port}`],
    username: s.username,
    credential: s.password,
  }));
}
