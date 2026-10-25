// input: explicit screen submissions; output: local agent process events and receipts; pos: task dispatch adapters.
import { spawn } from 'node:child_process';
import { randomUUID } from 'node:crypto';
import { appendFileSync } from 'node:fs';
import { clean, title } from './model.ts';
import type { Task, Provider } from './model.ts';

export class Dispatcher {
  tasks = new Map<string, Task>();
  receipt: string; changed: () => void; renamed: (from: string, to: string) => void;
  constructor(receipt: string, changed: () => void, renamed: (from: string, to: string) => void = () => {}) {this.receipt=receipt;this.changed=changed;this.renamed=renamed;}
  // A reply resumes the same session, so the answer lands in the conversation the user is reading.
  submit(provider: Provider, prompt: string, cwd: string, resume?: Task) {
    if (!prompt.trim() || prompt.length > 2000) throw new Error('任务为空或过长');
    const requestId = randomUUID();
    const task: Task = resume ? {...resume, status: 'running', progress: '已发送回复，等待 Agent 响应', updatedAt: Date.now(), startedAt: Date.now(), source: 'dispatch', owned: true}
      : {id: requestId, provider, title: title(prompt), cwd, status: 'running', progress: '任务已派发，等待 Agent 响应',
      updatedAt: Date.now(), startedAt: Date.now(), source: 'dispatch', owned: true};
    this.tasks.set(task.id, task);
    const log = (type: string, data: object = {}) => appendFileSync(this.receipt, JSON.stringify({at: new Date().toISOString(), requestId, provider, type, ...data})+'\n');
    log(resume ? 'replied' : 'submitted', {prompt, cwd, session: resume?.id});
    const args = provider === 'Codex'
      ? resume ? ['exec', 'resume', '--json', '--skip-git-repo-check', '-c', 'sandbox_mode="workspace-write"', resume.id, '-']
        : ['exec', '--json', '--sandbox', 'workspace-write', '--skip-git-repo-check', '-C', cwd, '-']
      : ['-p', ...(resume ? ['--resume', resume.id] : []), '--output-format', 'stream-json', '--verbose', '--permission-mode', 'default'];
    const child = spawn(provider === 'Codex' ? 'codex' : 'claude', args, {cwd, env: {...process.env, CODEX_THREAD_ID: undefined}, stdio: ['pipe','pipe','pipe']});
    child.stdin.end(prompt);
    let buffer = '', stderr = '', terminal = false;
    const update = () => { task.updatedAt = Date.now(); this.changed(); };
    child.stdout.on('data', b => {
      buffer += b.toString(); let end: number;
      while ((end = buffer.indexOf('\n')) >= 0) {
        const line = buffer.slice(0,end); buffer = buffer.slice(end+1);
        try {
          const e = JSON.parse(line);
          if (!resume && (e.thread_id || e.session_id) && task.id !== (e.thread_id || e.session_id)) { const from = task.id; this.tasks.delete(from); task.id = e.thread_id || e.session_id; this.tasks.set(task.id, task); this.renamed(from, task.id); }
          if (e.type === 'item.completed' && e.item?.type === 'agent_message') task.progress = clean(e.item.text);
          if (e.type === 'assistant') {
            const text = (e.message?.content || []).filter((c: any) => c.type === 'text').map((c: any) => c.text).join(' ');
            if (text) task.progress = clean(text);
          }
          if (e.type === 'turn.completed' || e.type === 'result') {
            terminal = true; task.status = e.is_error ? 'error' : 'done';
            if (e.result) task.progress = clean(e.result);
            log(task.status, {id: task.id, result: task.progress, durationMs: Date.now()-task.startedAt!});
          }
          if (e.type === 'turn.failed' || e.type === 'error') {
            terminal = true; task.status = 'error'; task.progress = clean(e.error?.message || e.message || '执行失败');
            log('error', {detail: task.progress});
          }
          update();
        } catch { /* Ignore non-protocol stdout. */ }
      }
    });
    child.stderr.on('data', b => { stderr = (stderr+b.toString()).slice(-2000); });
    child.on('error', e => { terminal = true; task.status = 'error'; task.progress = e.message; log('error', {detail: e.message}); update(); });
    child.on('exit', code => {
      if (!terminal) { task.status = 'error'; task.progress = clean(stderr || `进程结束 (${code})，未收到完成事件`); log('error', {detail: task.progress}); }
      update();
    });
    this.changed(); return task;
  }
  list() { return [...this.tasks.values()].map(t => ({...t})); }
}
