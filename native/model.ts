// input: provider events; output: common task state; pos: semantics shared by adapters and screen.
export type Provider = 'Codex' | 'Claude';
export type Status = 'running' | 'done' | 'waiting' | 'stopped' | 'error' | 'unknown';
export type Task = { id: string; provider: Provider; title: string; cwd: string; status: Status;
  progress: string; updatedAt: number; startedAt?: number; turnId?: string; source: string; owned?: boolean };
export const labels: Record<Status, string> = { running: '运行中', done: '已完成', waiting: '待处理', stopped: '已停止', error: '出错', unknown: '空闲' };
// Keeps paragraph breaks so the reading view can show the whole latest reply, not one squashed line.
export function clean(text: string, limit = 6000) {
  return text.replace(/<[^>]*>/g, '').replace(/!\[[^\]]*\]\([^)]*\)/g, '').replace(/\[([^\]]+)\]\([^)]*\)/g, '$1')
    .replace(/^\s*\|?[-:| ]+\|[-:| ]*$/gm, '').replace(/^#+\s*/gm, '').replace(/[*`]/g, '')
    .replace(/[ \t]+/g, ' ').replace(/ *\n */g, '\n').replace(/\n{2,}/g, '\n').trim().slice(0, limit);
}
export function title(text: string) {
  const user = text.split('## My request:').at(-1) || text;
  return clean(user, 160).replace(/\n/g, ' ');
}
export function codexEvent(task: Task, event: any) {
  const p = event.payload || {}, at = Date.parse(event.timestamp) || task.updatedAt;
  if (event.type === 'event_msg') {
    if (p.type === 'task_started') { task.status = 'running'; task.startedAt = at; task.turnId = p.turn_id; }
    if (p.type === 'task_complete' && (!task.turnId || p.turn_id === task.turnId)) {
      task.status = 'done'; task.progress = clean(p.last_agent_message || task.progress);
    }
    if (p.type === 'turn_aborted' && (!task.turnId || p.turn_id === task.turnId)) task.status = 'stopped';
    if (['task_started', 'task_complete', 'turn_aborted'].includes(p.type)) task.updatedAt = at;
  }
  if (event.type !== 'response_item') return;
  if (p.type === 'message' && p.role === 'assistant' && ['commentary', 'final_answer'].includes(p.phase)) {
    const text = (p.content || []).filter((c: any) => c.type === 'output_text').map((c: any) => c.text).join(' ');
    if (text) { task.progress = clean(text); task.updatedAt = at; }
  }
  if (p.type === 'function_call' && p.name?.endsWith('request_user_input')) { task.status = 'waiting'; task.updatedAt = at; }
  if (p.type === 'function_call_output' && task.status === 'waiting') { task.status = 'running'; task.updatedAt = at; }
}
export function claudeEvent(task: Task, p: any) {
  const at = Date.parse(p.timestamp) || task.updatedAt;
  if (p.cwd) task.cwd = p.cwd;
  if (p.type === 'custom-title' && p.customTitle) task.title = title(p.customTitle);
  if (p.type === 'last-prompt' && p.lastPrompt) task.title = title(p.lastPrompt);
  const blocks = Array.isArray(p.message?.content) ? p.message.content : [];
  if (p.type === 'user' && !p.isMeta && !p.isSidechain && !blocks.some((c: any) => c.type === 'tool_result')) {
    const text = typeof p.message?.content === 'string' ? p.message.content : blocks.filter((c: any) => c.type === 'text').map((c: any) => c.text).join(' ');
    if (text && !text.startsWith('<')) { task.title = title(text); task.status = 'running'; task.startedAt = at; task.updatedAt = at; }
  }
  if (p.type === 'assistant' && !p.isSidechain) {
    const text = blocks.filter((c: any) => c.type === 'text').map((c: any) => c.text).join(' ');
    if (text) task.progress = clean(text);
    if (p.message?.stop_reason === 'end_turn') task.status = 'done';
    else task.status = 'running';
    task.updatedAt = at;
  }
  if (p.type === 'system' && p.subtype === 'api_error') { task.status = 'error'; task.progress = 'API 请求失败'; task.updatedAt = at; }
  if (p.type === 'system' && p.subtype === 'turn_duration') { task.status = 'done'; task.updatedAt = at; }
}
