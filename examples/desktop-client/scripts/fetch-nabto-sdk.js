#!/usr/bin/env node
// Downloads the prebuilt Nabto Edge Client SDK shared library from
// https://github.com/nabto/nabto-client-sdk-releases into vendor/<platform>-<arch>/.
//
// Runs as an npm postinstall step, so it is plain JavaScript (TypeScript is not
// compiled yet at that point).
//
//   node scripts/fetch-nabto-sdk.js          # current platform only
//   node scripts/fetch-nabto-sdk.js --all    # every supported platform
//
// The <platform>-<arch> directory names match electron-builder's ${platform}-${arch}
// macros so packaging can pick the right library up as an extraResource.

'use strict';

const fs = require('fs');
const path = require('path');

const SDK_VERSION = process.env.NABTO_CLIENT_SDK_VERSION || 'v5.15.5';
const BASE_URL = `https://raw.githubusercontent.com/nabto/nabto-client-sdk-releases/${SDK_VERSION}/lib`;

// vendor dir name -> file in the releases repo
const TARGETS = {
  'linux-x64': 'linux-x86_64/libnabto_client.so',
  'linux-arm64': 'linux-aarch64/libnabto_client.so',
  'mac-x64': 'macos-universal/libnabto_client.dylib',
  'mac-arm64': 'macos-universal/libnabto_client.dylib',
  'mac-universal': 'macos-universal/libnabto_client.dylib',
  'win-x64': 'windows-x86_64/nabto_client.dll',
};

function currentTarget() {
  const platform = { darwin: 'mac', win32: 'win', linux: 'linux' }[process.platform];
  return `${platform}-${process.arch}`;
}

async function download(target) {
  const remote = TARGETS[target];
  if (!remote) {
    console.warn(`[fetch-nabto-sdk] No Nabto Client SDK build for ${target}, skipping.`);
    return;
  }
  const destDir = path.join(__dirname, '..', 'vendor', target);
  const dest = path.join(destDir, path.basename(remote));
  const versionFile = path.join(destDir, 'VERSION');
  if (fs.existsSync(dest) && fs.existsSync(versionFile) &&
      fs.readFileSync(versionFile, 'utf8').trim() === SDK_VERSION) {
    console.log(`[fetch-nabto-sdk] ${target}: ${SDK_VERSION} already present`);
    return;
  }
  const url = `${BASE_URL}/${remote}`;
  console.log(`[fetch-nabto-sdk] ${target}: downloading ${url}`);
  const res = await fetch(url);
  if (!res.ok) {
    throw new Error(`Download of ${url} failed: HTTP ${res.status}`);
  }
  const data = Buffer.from(await res.arrayBuffer());
  fs.mkdirSync(destDir, { recursive: true });
  fs.writeFileSync(dest, data);
  fs.writeFileSync(versionFile, SDK_VERSION + '\n');
  console.log(`[fetch-nabto-sdk] ${target}: wrote ${dest} (${data.length} bytes)`);
}

async function main() {
  const targets = process.argv.includes('--all') ? Object.keys(TARGETS) : [currentTarget()];
  for (const t of targets) {
    await download(t);
  }
}

main().catch((err) => {
  console.error(`[fetch-nabto-sdk] ${err.message}`);
  process.exit(1);
});
