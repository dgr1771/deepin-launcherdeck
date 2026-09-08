// 点开论坛登录框（等用户登录）
const http = require('http');
function getJson(url) {
  return new Promise((resolve, reject) => {
    http.get(url, (res) => {
      let d = '';
      res.on('data', (c) => d += c);
      res.on('end', () => { try { resolve(JSON.parse(d)); } catch (e) { reject(e); } });
    }).on('error', reject);
  });
}
async function main() {
  const pages = await getJson('http://127.0.0.1:9224/json');
  const page = pages.find(p => p.type === 'page' && /bbs\.deepin/.test(p.url || ''));
  if (!page) { console.error('no bbs page'); process.exit(1); }
  const ws = new WebSocket(page.webSocketDebuggerUrl);
  let id = 0;
  const pending = new Map();
  ws.onmessage = (ev) => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) { pending.get(m.id)(m.result); pending.delete(m.id); }
  };
  await new Promise((r) => ws.onopen = r);
  const send = (method, params) => new Promise((resolve) => {
    const mid = ++id;
    pending.set(mid, resolve);
    ws.send(JSON.stringify({ id: mid, method, params }));
  });
  const r = await send('Runtime.evaluate', { returnByValue: true, expression: `(() => {
    // flarum：头部「登录」按钮 .item-logIn button 或文本匹配
    const btns = [...document.querySelectorAll('button, a')];
    const login = btns.find(b => b.innerText.trim() === '登录' && b.offsetParent);
    if (login) { login.click(); return { clicked: true, text: login.innerText }; }
    return { clicked: false };
  })()` });
  console.log(JSON.stringify(r.result.value));
  await new Promise(r2 => setTimeout(r2, 3000));
  const r2 = await send('Runtime.evaluate', { returnByValue: true, expression: `(() => {
    const modal = document.querySelector('.ModalManager .Modal, .modal-dialog, [class*="LogInModal"]');
    return { modalOpen: !!modal, modalText: modal ? modal.innerText.slice(0, 200) : '' };
  })()` });
  console.log(JSON.stringify(r2.result.value));
  ws.close();
  process.exit(0);
}
main().catch(e => { console.error('ERR', e.message); process.exit(1); });
