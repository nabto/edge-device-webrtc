// Persistent client state in Electron's userData directory:
//   client.json   the client private key (encrypted with safeStorage when the OS supports it)
//   devices.json  bookmarks of paired devices

import fs from 'fs';
import path from 'path';
import { safeStorage } from 'electron';
import type { Bookmark } from '../shared/types';

interface ClientFile {
  privateKey: string;
  encrypted: boolean;
}

export class Store {
  private readonly clientFile: string;
  private readonly devicesFile: string;

  constructor(dir: string) {
    fs.mkdirSync(dir, { recursive: true });
    this.clientFile = path.join(dir, 'client.json');
    this.devicesFile = path.join(dir, 'devices.json');
  }

  /// The client's private key. Created with `create` on first use. The key's
  /// fingerprint is the client's identity on every device it pairs with.
  async privateKey(create: () => string): Promise<string> {
    const stored = this.readJson<ClientFile>(this.clientFile);
    if (stored) {
      if (!stored.encrypted) {
        return stored.privateKey;
      }
      const { result, shouldReEncrypt } = await safeStorage.decryptStringAsync(Buffer.from(stored.privateKey, 'base64'));
      if (shouldReEncrypt) {
        await this.savePrivateKey(result);
      }
      return result;
    }
    const key = create();
    await this.savePrivateKey(key);
    return key;
  }

  private async savePrivateKey(key: string): Promise<void> {
    const encrypted = await safeStorage.isAsyncEncryptionAvailable();
    const file: ClientFile = {
      privateKey: encrypted ? (await safeStorage.encryptStringAsync(key)).toString('base64') : key,
      encrypted,
    };
    this.writeJson(this.clientFile, file);
  }

  bookmarks(): Bookmark[] {
    return this.readJson<Bookmark[]>(this.devicesFile) ?? [];
  }

  bookmark(id: string): Bookmark | undefined {
    return this.bookmarks().find((b) => b.id === id);
  }

  /// Insert or replace the bookmark with the same id.
  saveBookmark(bookmark: Bookmark): void {
    const others = this.bookmarks().filter((b) => b.id !== bookmark.id);
    this.writeJson(this.devicesFile, [...others, bookmark]);
  }

  removeBookmark(id: string): void {
    this.writeJson(this.devicesFile, this.bookmarks().filter((b) => b.id !== id));
  }

  private readJson<T>(file: string): T | undefined {
    try {
      return JSON.parse(fs.readFileSync(file, 'utf8')) as T;
    } catch {
      return undefined;
    }
  }

  private writeJson(file: string, value: unknown): void {
    const tmp = `${file}.tmp`;
    fs.writeFileSync(tmp, JSON.stringify(value, null, 2), { mode: 0o600 });
    fs.renameSync(tmp, file);
  }
}
