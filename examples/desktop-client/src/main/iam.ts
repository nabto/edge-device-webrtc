// Edge IAM pairing over CoAP (nm_iam module in the Nabto Embedded SDK).
//
// Mode selection mirrors the iOS demo (edge-ios-webrtc-decentral-auth,
// PairingService.swift): LocalInitial, then LocalOpen, then PasswordOpen, then
// PasswordInvite, taking the first one the device offers that we have the
// credentials for.

import os from 'os';
import { decode, encode } from 'cbor-x';
import type { Connection } from './nabto/client';
import { NabtoError } from './nabto/client';
import { sanitizeUsername } from '../shared/pairing-string';
import type { PairingInput, PairingMode } from '../shared/types';

const CF_CBOR = 60;

export interface PairingInfo {
  modes: PairingMode[];
  appName?: string;
  appVersion?: string;
  friendlyName?: string;
  nabtoVersion?: string;
}

export interface IamUser {
  username: string;
  displayName?: string;
  role?: string;
  sct?: string;
  fingerprints: string[];
}

export class PairingError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'PairingError';
  }
}

async function cborRequest(connection: Connection, method: 'GET' | 'POST', path: string, body?: unknown) {
  const res = await connection.coap(method, path,
    body === undefined ? undefined : { contentFormat: CF_CBOR, payload: encode(body) });
  const value = res.contentFormat === CF_CBOR && res.payload.length > 0 ? decode(res.payload) : undefined;
  return { status: res.status, value, text: res.contentFormat !== CF_CBOR ? res.payload.toString('utf8') : '' };
}

/// GET /iam/pairing. Returns null when the device has no IAM module (404).
export async function getPairingInfo(connection: Connection): Promise<PairingInfo | null> {
  const res = await cborRequest(connection, 'GET', '/iam/pairing');
  if (res.status === 404) {
    return null;
  }
  if (res.status !== 205 || typeof res.value !== 'object') {
    throw new PairingError(`GET /iam/pairing failed with status ${res.status} ${res.text}`);
  }
  const v = res.value as Record<string, unknown>;
  return {
    modes: (Array.isArray(v.Modes) ? v.Modes : []) as PairingMode[],
    appName: v.AppName as string | undefined,
    appVersion: v.AppVersion as string | undefined,
    friendlyName: v.FriendlyName as string | undefined,
    nabtoVersion: v.NabtoVersion as string | undefined,
  };
}

/// GET /iam/me. Returns null when this client is not paired.
export async function getMe(connection: Connection): Promise<IamUser | null> {
  const res = await cborRequest(connection, 'GET', '/iam/me');
  if (res.status === 404) {
    return null;
  }
  if (res.status !== 205 || typeof res.value !== 'object') {
    throw new PairingError(`GET /iam/me failed with status ${res.status} ${res.text}`);
  }
  const v = res.value as Record<string, unknown>;
  const fps = Array.isArray(v.Fingerprints)
    ? (v.Fingerprints as Array<Record<string, string>>).map((f) => f.Fingerprint)
    : (typeof v.Fingerprint === 'string' ? [v.Fingerprint] : []);
  return {
    username: v.Username as string,
    displayName: v.DisplayName as string | undefined,
    role: v.Role as string | undefined,
    sct: v.Sct as string | undefined,
    fingerprints: fps,
  };
}

function checkPairingStatus(status: number, text: string, mode: PairingMode): void {
  if (status === 201) {
    return;
  }
  switch (status) {
    case 400: throw new PairingError(`${mode} pairing rejected the request (400): ${text}`);
    case 401:
    case 403: throw new PairingError(mode === 'LocalInitial' || mode === 'LocalOpen'
      ? `${mode} pairing was denied. It requires a local (same LAN) connection.`
      : `${mode} pairing was denied by the device IAM configuration.`);
    case 404: throw new PairingError(`${mode} pairing is not enabled on the device.`);
    case 409: throw new PairingError(mode === 'LocalOpen' || mode === 'PasswordOpen'
      ? 'That username is already in use on the device. Choose another one.'
      : 'The device user is already paired with another client.');
    default: throw new PairingError(`${mode} pairing failed with status ${status}: ${text}`);
  }
}

function fingerprintName(): string {
  return `${os.hostname()} (desktop client)`;
}

/// Pair the client on an already connected `connection`. Returns the mode used,
/// or null if this client was already paired.
export async function pair(connection: Connection, input: PairingInput, info: PairingInfo): Promise<PairingMode | null> {
  const existing = await getMe(connection);
  const clientFp = connection.clientFingerprint();
  if (existing && existing.fingerprints.includes(clientFp)) {
    return null;
  }

  const modes = info.modes;
  if (modes.length === 0) {
    throw new PairingError('The device is not open for pairing. Ask the owner for an invite, or reset the device IAM state.');
  }
  const username = sanitizeUsername(input.desiredUsername || input.username || os.userInfo().username);

  if (modes.includes('LocalInitial')) {
    const res = await cborRequest(connection, 'POST', '/iam/pairing/local-initial', { FingerprintName: fingerprintName() });
    checkPairingStatus(res.status, res.text, 'LocalInitial');
    return 'LocalInitial';
  }
  if (modes.includes('LocalOpen')) {
    const res = await cborRequest(connection, 'POST', '/iam/pairing/local-open', { Username: username, FingerprintName: fingerprintName() });
    checkPairingStatus(res.status, res.text, 'LocalOpen');
    return 'LocalOpen';
  }
  // The device lists PasswordInvite whenever invite pairing is enabled, even if
  // the pairing string is an open one. An open pairing string has no username.
  const canInvite = modes.includes('PasswordInvite') && !!input.username && !!input.password;
  if (modes.includes('PasswordOpen') && input.password && !canInvite) {
    await passwordAuthenticate(connection, '', input.password);
    const res = await cborRequest(connection, 'POST', '/iam/pairing/password-open', { Username: username, FingerprintName: fingerprintName() });
    checkPairingStatus(res.status, res.text, 'PasswordOpen');
    return 'PasswordOpen';
  }
  if (canInvite) {
    await passwordAuthenticate(connection, input.username!, input.password!);
    const res = await cborRequest(connection, 'POST', '/iam/pairing/password-invite', { FingerprintName: fingerprintName() });
    checkPairingStatus(res.status, res.text, 'PasswordInvite');
    return 'PasswordInvite';
  }
  if (modes.includes('PasswordOpen') || modes.includes('PasswordInvite')) {
    throw new PairingError(`The device requires password pairing (${modes.join(', ')}), but the pairing details have no ${modes.includes('PasswordInvite') && !modes.includes('PasswordOpen') ? 'username and password' : 'password'}.`);
  }
  throw new PairingError(`None of the device's pairing modes are supported: ${modes.join(', ')}`);
}

async function passwordAuthenticate(connection: Connection, username: string, password: string): Promise<void> {
  try {
    await connection.passwordAuthenticate(username, password);
  } catch (err) {
    if (err instanceof NabtoError && err.code === 'UNAUTHORIZED') {
      throw new PairingError('The pairing password was not accepted by the device.');
    }
    throw err;
  }
}
