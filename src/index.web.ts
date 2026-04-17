export interface RangeQueryResult {
  key: string;
  value: any;
}

const IS_SERVER = typeof window === 'undefined';

/**
 * Web Implementation of TurboDB.
 * Features:
 * - SSR Friendly (No-op on server)
 * - IndexedDB Backend for persistence
 * - In-memory cache for synchronous reads (SEO/SSR optimization)
 * - Debounced writes to prevent blocking interaction (Core Web Vitals)
 */
export class TurboDB {
  static install(): boolean {
    return true; // No-op on web
  }

  static getDocumentsDirectory(): string {
    return '/';
  }

  private isInitialized = false;
  private storage: Map<string, any> = new Map();
  private db: IDBDatabase | null = null;
  private saveTimeout: any = null;
  private initPromise: Promise<void> | null = null;

  constructor(
    private path: string,
    private size: number = 10 * 1024 * 1024
  ) {
    if (IS_SERVER) {
      console.log('TurboDB (Web): SSR Mode');
    } else {
      console.log(`TurboDB (Web): Created with size limit ${this.size}`);
      // Lazy-trigger initialization
      this.initPromise = this.ensureInitialized();
    }
  }

  private async ensureInitialized(): Promise<void> {
    if (IS_SERVER) return;
    if (this.isInitialized) return;
    if (this.initPromise) return this.initPromise;

    this.initPromise = new Promise((resolve) => {
      const dbName = `turbodb_${this.path.replace(/\//g, '_')}`;
      const request = indexedDB.open(dbName, 1);

      request.onerror = (event) => {
        console.error('TurboDB (Web): IndexedDB error', event);
        this.isInitialized = true; // Mark as initialized even on failure to avoid infinite loops
        resolve();
      };

      request.onupgradeneeded = (event: any) => {
        const db = event.target.result;
        if (!db.objectStoreNames.contains('kv')) {
          db.createObjectStore('kv');
        }
      };

      request.onsuccess = async (event: any) => {
        this.db = event.target.result;
        await this.loadFromIndexedDB();
        this.isInitialized = true;
        resolve();
      };
    });

    return this.initPromise;
  }

  private async loadFromIndexedDB(): Promise<void> {
    if (!this.db) return;

    return new Promise((resolve) => {
      const transaction = this.db!.transaction(['kv'], 'readonly');
      const store = transaction.objectStore('kv');
      const request = store.openCursor();

      request.onsuccess = (event: any) => {
        const cursor = event.target.result;
        if (cursor) {
          this.storage.set(cursor.key, cursor.value);
          cursor.continue();
        } else {
          resolve();
        }
      };

      request.onerror = () => {
        console.warn('TurboDB (Web): Failed to load from IndexedDB');
        resolve();
      };
    });
  }

  private scheduleSave() {
    if (IS_SERVER) return;
    if (this.saveTimeout) clearTimeout(this.saveTimeout);
    this.saveTimeout = setTimeout(() => this.persistToIndexedDB(), 100);
  }

  private async persistToIndexedDB(): Promise<void> {
    if (!this.db) return;

    return new Promise((resolve) => {
      const transaction = this.db!.transaction(['kv'], 'readwrite');
      const store = transaction.objectStore('kv');

      store.clear();
      for (const [key, value] of this.storage.entries()) {
        store.put(value, key);
      }

      transaction.oncomplete = () => resolve();
      transaction.onerror = () => {
        console.warn('TurboDB (Web): Persistence failed');
        resolve();
      };
    });
  }

  // --- Synchronous API (SSR/Hydration Optimization) ---

  /**
   * Synchronously check if a key exists (uses in-memory cache)
   */
  has(key: string): boolean {
    return this.storage.has(key);
  }

  /**
   * Synchronously get a value (uses in-memory cache)
   */
  get<T = any>(key: string): T | undefined {
    return this.storage.get(key);
  }

  /**
   * Synchronously set a value (updates cache immediately, persists in background)
   */
  set(key: string, value: any): boolean {
    if (IS_SERVER) return false;
    this.storage.set(key, value);
    this.scheduleSave();
    return true;
  }

  /**
   * Synchronously remove a value
   */
  remove(key: string): boolean {
    if (IS_SERVER) return false;
    const res = this.storage.delete(key);
    this.scheduleSave();
    return res;
  }

  /**
   * Synchronously clear all values
   */
  clear(): boolean {
    if (IS_SERVER) return false;
    this.storage.clear();
    this.scheduleSave();
    return true;
  }

  getAllKeys(): string[] {
    return Array.from(this.storage.keys());
  }

  getAllKeysPaged(limit: number, offset: number): string[] {
    const keys = Array.from(this.storage.keys());
    return keys.slice(offset, offset + limit);
  }

  // --- Asynchronous API ---

  async setAsync(key: string, value: any): Promise<boolean> {
    await this.ensureInitialized();
    return this.set(key, value);
  }

  async getAsync<T = any>(key: string): Promise<T | undefined> {
    await this.ensureInitialized();
    return this.get<T>(key);
  }

  async setMulti(entries: Record<string, any>): Promise<boolean> {
    if (IS_SERVER) return false;
    await this.ensureInitialized();
    for (const [key, value] of Object.entries(entries)) {
      this.storage.set(key, value);
    }
    this.scheduleSave();
    return true;
  }

  async getMultiple(keys: string[]): Promise<Record<string, any>> {
    await this.ensureInitialized();
    const result: Record<string, any> = {};
    for (const key of keys) {
      result[key] = this.storage.get(key);
    }
    return result;
  }

  async deleteAll(): Promise<boolean> {
    return this.clear();
  }

  async rangeQuery(
    startKey: string,
    endKey: string
  ): Promise<RangeQueryResult[]> {
    await this.ensureInitialized();
    const results: RangeQueryResult[] = [];
    const keys = Array.from(this.storage.keys()).sort();
    for (const key of keys) {
      if (key >= startKey && key <= endKey) {
        results.push({ key, value: this.storage.get(key) });
      }
    }
    return results;
  }

  async getAllKeysAsync(): Promise<string[]> {
    await this.ensureInitialized();
    return this.getAllKeys();
  }

  async flush(): Promise<void> {
    if (IS_SERVER) return;
    await this.ensureInitialized();
    if (this.saveTimeout) {
      clearTimeout(this.saveTimeout);
      this.saveTimeout = null;
    }
    await this.persistToIndexedDB();
  }

  // Legacy method aliases
  async del(key: string): Promise<boolean> {
    return this.remove(key);
  }

  benchmark(): number {
    return 0;
  }
}

export default TurboDB;
