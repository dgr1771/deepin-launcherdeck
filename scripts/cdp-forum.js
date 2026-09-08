// deepin 论坛 CDP 驱动 v2：导航 + 轮询等待 + 状态读取
const http = require('http');
function getJson(url) {
  return new Promise((resolve, reject) => {
    http.get(url, (res) => {
      let d = '';
      res.on('data', (c) => d += c);
      res.on('end', () => { try { resolve(JSON.parse(d)); } catch (e) { reject(new Error('bad json')); } });
    }).on('error', reject);
  });
}
async function main() {
  const pages = await getJson('http://127.0.0.1:9224/json');
  const page = pages.find(p => p.type === 'page');
  const ws = new WebSocket(page.webSocketDebuggerUrl);
  let id = 0;
  const pending = new Map();
  ws.onmessage = (ev) => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) { pending.get(m.id)(m.result); pending.delete(m.id); }
  };
  await new Promise((r) => ws.onopen = r);
  function send(method, params) {
    return new Promise((resolve) => {
      const mid = ++id;
      pending.set(mid, resolve);
      ws.send(JSON.stringify({ id: mid, method, params }));
    });
  }
  const cmd = process.argv[2] || 'check';
  if (cmd === 'check') {
    await send('Page.enable');
    await send('Page.navigate', { url: 'https://bbs.deepin.org.cn/' });
    // 轮询至多 90s：document.body 有文字才算活
    let state = null;
    for (let i = 0; i < 30; i++) {
      await new Promise(r => setTimeout(r, 3000));
      const r = await send('Runtime.evaluate', { returnByValue: true, expression: `(() => ({
        url: location.href, title: document.title,
        text: document.body ? document.body.innerText.slice(0, 80) : ''
      }))()` });
      state = r.result.value;
      process.stderr.write(`poll${i} ${state.url.slice(0, 50)} ${state.text.slice(0, 30)}\n`);
      if (/bbs\.deepin/.test(state.url) && state.text.trim()) break;
    }
    const r2 = await send('Runtime.evaluate', { returnByValue: true, expression: `(() => {
      const text = document.body.innerText;
      return {
        url: location.href, title: document.title,
        loggedIn: (() => {
          // flarum 头部：登录后右侧是用户头像+用户名，未登录是"登录/注册"按钮
          const session = document.querySelector('#header .SessionScroller .item-session, #header-secondary .item-session');
          const html = (session && session.innerHTML) || '';
          if (/avatar/i.test(html)) return 'yes';
          if (/登录|log ?in|注册/i.test(html)) return 'no';
          if (/退出|控制台|settings/i.test(text)) return 'yes';
          return 'unknown';
        })(),
        hasComposer: !!document.querySelector('.IndexPage, .DiscussionList'),
        textHead: text.slice(0, 400),
      };
    })()` });
    console.log(JSON.stringify(r2.result.value, null, 1));
  }
  ws.close();
  process.exit(0);
}
main().catch(e => { console.error('ERR', e.message); process.exit(1); });
