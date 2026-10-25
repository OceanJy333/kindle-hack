// input: canonical Codex database/rollouts and Claude transcripts; output: incremental task updates; pos: local adapters.
import { DatabaseSync } from 'node:sqlite';
import { openSync, closeSync, readSync, statSync, watch, readdirSync, existsSync } from 'node:fs';
import { StringDecoder } from 'node:string_decoder';
import { homedir } from 'node:os';
import { join } from 'node:path';
import { codexEvent, claudeEvent, title } from './model.ts';
import type { Task } from './model.ts';

class Tail {
  offset = 0; rest = ''; decoder = new StringDecoder('utf8'); watcher: ReturnType<typeof watch>;
  path: string; consume: (event: any) => void; changed: () => void;
  constructor(path: string, consume: (event: any) => void, changed: () => void) {
    this.path=path;this.consume=consume;this.changed=changed;
    this.read();
    this.watcher = watch(path, () => { this.read(); this.changed(); });
    this.watcher.on('error', () => {});
  }
  read() {
    const size = statSync(this.path).size;
    if (size < this.offset) { this.offset = 0; this.rest = ''; this.decoder = new StringDecoder('utf8'); }
    if (size === this.offset) return;
    const fd = openSync(this.path, 'r'), buffer = Buffer.alloc(256 * 1024);
    try {
      while (this.offset < size) {
        const count = readSync(fd, buffer, 0, Math.min(buffer.length, size-this.offset), this.offset);
        if (!count) break;
        this.offset += count; this.rest += this.decoder.write(buffer.subarray(0, count));
        let end: number;
        while ((end = this.rest.indexOf('\n')) >= 0) {
          const line = this.rest.slice(0, end); this.rest = this.rest.slice(end+1);
          try { this.consume(JSON.parse(line)); } catch { /* Incomplete/unknown records have no state authority. */ }
        }
      }
    } finally { closeSync(fd); }
  }
}

export class Sources {
  tasks = new Map<string, Task>(); tails = new Map<string, Tail>();
  db: DatabaseSync;
  current: string; changed: () => void;
  constructor(current: string, changed: () => void) {
    this.current=current;this.changed=changed;
    this.db = new DatabaseSync(join(homedir(), '.codex/state_5.sqlite'), { readOnly: true });
    this.discover();
    setInterval(() => this.discover(), 30000).unref();
    // fs.watch is the immediate path; this catches dropped watch events and file replacement.
    setInterval(() => { for (const t of this.tails.values()) { try { t.read(); } catch {} } this.changed(); }, 15000).unref();
  }
  add(task: Task, path: string, consume: (task: Task, event: any) => void) {
    if (this.tails.has(path)) { const existing=this.tasks.get(task.id); if(existing && task.provider==='Codex')existing.title=task.title;return; }
    if (!existsSync(path)) return;
    const old=this.tasks.get(task.id);
    if(old && old.source!==path) { this.tails.get(old.source)?.watcher.close();this.tails.delete(old.source); }
    this.tasks.set(task.id, task);
    this.tails.set(path, new Tail(path, e => consume(task, e), this.changed));
  }
  discover() {
    const rows: any[] = this.db.prepare("SELECT id,COALESCE(NULLIF(name,''),NULLIF(title,''),NULLIF(first_user_message,''),'Codex 对话') AS title,cwd,rollout_path,updated_at FROM threads WHERE archived=0 ORDER BY updated_at DESC LIMIT 12").all();
    if (!rows.some(t => t.id === this.current)) {
      const current = this.db.prepare("SELECT id,COALESCE(NULLIF(name,''),NULLIF(title,''),NULLIF(first_user_message,''),'Codex 对话') AS title,cwd,rollout_path,updated_at FROM threads WHERE id=?").get(this.current);
      if (current) rows.unshift(current);
    }
    for (const row of rows) this.add({ id: row.id, provider: 'Codex', title: title(row.title), cwd: row.cwd,
      status: 'unknown', progress: '', updatedAt: row.updated_at*1000, source: row.rollout_path }, row.rollout_path, codexEvent);
    const root = join(homedir(), '.claude/projects');
    if (existsSync(root)) {
      const files: {path: string; time: number}[] = [];
      for (const dir of readdirSync(root, { withFileTypes: true }).filter(d => d.isDirectory()))
        for (const f of readdirSync(join(root, dir.name)).filter(n => n.endsWith('.jsonl'))) {
          const path = join(root, dir.name, f); files.push({ path, time: statSync(path).mtimeMs });
        }
      for (const f of files.sort((a,b) => b.time-a.time).slice(0, 6)) {
        const id = f.path.split('/').at(-1)!.replace('.jsonl', '');
        this.add({id, provider: 'Claude', title: 'Claude 对话', cwd: '', status: 'unknown', progress: '', updatedAt: f.time, source: f.path}, f.path, claudeEvent);
      }
    }
    this.changed();
  }
  list() {
    return [...this.tasks.values()].map(t => {
      // An unfinished old transcript cannot establish a live process.
      if (['running', 'waiting'].includes(t.status) && Date.now()-t.updatedAt > 30*60*1000 && t.id !== this.current)
        return { ...t, status: 'unknown' as const };
      return {...t};
    });
  }
}
