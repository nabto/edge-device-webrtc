import { test } from 'node:test';
import assert from 'node:assert/strict';
import { parsePairingString, sanitizeUsername } from '../shared/pairing-string';

test('parses the initial pairing string printed by edge_device_webrtc', () => {
  const fp = '99E725BE3FC35F0CABA982A2D9D095B50DDA2F905076A1B928D6D856E61FBF75';
  assert.deepEqual(parsePairingString(`p=pr-abc,d=de-xyz,u=admin,pwd=secret,sct=token,fp=${fp}`), {
    productId: 'pr-abc',
    deviceId: 'de-xyz',
    username: 'admin',
    password: 'secret',
    sct: 'token',
    deviceFingerprint: fp.toLowerCase(),
  });
});

test('accepts ; and : separators and surrounding whitespace', () => {
  assert.deepEqual(parsePairingString(' p=pr-abc; d=de-xyz : pwd=x \n'), {
    productId: 'pr-abc',
    deviceId: 'de-xyz',
    password: 'x',
  });
});

test('keeps = characters inside values', () => {
  assert.equal(parsePairingString('p=a,d=b,pwd=x=y').password, 'x=y');
});

test('rejects malformed strings', () => {
  assert.throws(() => parsePairingString(''), /empty/);
  assert.throws(() => parsePairingString('p=a'), /required field: d/);
  assert.throws(() => parsePairingString('d=b'), /required field: p/);
  assert.throws(() => parsePairingString('p=a,d=b,foo=1'), /Unexpected element/);
  assert.throws(() => parsePairingString('p=a,d=b,oops'), /missing '='/);
});

test('sanitizes usernames to the IAM username alphabet', () => {
  assert.equal(sanitizeUsername('John Doe'), 'john-doe');
  assert.equal(sanitizeUsername('..Ålborg_PC.'), 'lborg_pc');
  assert.equal(sanitizeUsername('!!!'), 'user');
  assert.equal(sanitizeUsername('a'.repeat(100)).length, 64);
});
