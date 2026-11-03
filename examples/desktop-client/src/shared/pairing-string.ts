// Parser for Nabto pairing strings as printed by edge_device_webrtc, e.g.
//   p=pr-abcdefgh,d=de-ijklmnop,u=admin,pwd=secret,sct=token,fp=<64 hex chars>
//
// Same format as the iOS demo app (edge-ios-webrtc-decentral-auth,
// PairingString.swift): elements separated by ',' ';' or ':', p and d required,
// u, pwd, sct and fp optional.

import type { PairingDetails } from './types';

export function parsePairingString(raw: string): PairingDetails {
  const elements = raw
    .split(/[;:,]/)
    .map((e) => e.trim())
    .filter((e) => e.length > 0);
  if (elements.length === 0) {
    throw new Error('Pairing string is empty.');
  }

  const details: Partial<PairingDetails> = {};
  for (const element of elements) {
    const eq = element.indexOf('=');
    if (eq < 0) {
      throw new Error(`Pairing string element is missing '=': ${element}`);
    }
    const key = element.slice(0, eq);
    const value = element.slice(eq + 1);
    switch (key) {
      case 'p': details.productId = value; break;
      case 'd': details.deviceId = value; break;
      case 'u': details.username = value; break;
      case 'pwd': details.password = value; break;
      case 'sct': details.sct = value; break;
      case 'fp': details.deviceFingerprint = value.toLowerCase(); break;
      default: throw new Error(`Unexpected element in pairing string: ${key}`);
    }
  }
  if (!details.productId) {
    throw new Error('Pairing string is missing required field: p');
  }
  if (!details.deviceId) {
    throw new Error('Pairing string is missing required field: d');
  }
  return details as PairingDetails;
}

/// Usernames must be lower case [a-z0-9._-] (same rule as the iOS app).
export function sanitizeUsername(raw: string): string {
  const filtered = raw.toLowerCase().replace(/[^a-z0-9._-]/g, '-').replace(/^[-._]+|[-._]+$/g, '');
  return filtered.length === 0 ? 'user' : filtered.slice(0, 64);
}
