// Actual mobile HTML/JS; local HTTP fixture only, no device or real API Key.
const {chromium}=require('playwright');
const fs=require('node:fs'),http=require('node:http'),assert=require('node:assert/strict'),path=require('node:path');
const root=path.resolve(__dirname,'..'),out=path.join(root,'build-sim','setup-evidence');fs.mkdirSync(out,{recursive:true});
let submitted,mode='success',posts=0,tests=0,lastPath='',scanFail=false;
const server=http.createServer((req,res)=>{
 res.setHeader('Cache-Control','no-store');
 if(req.url==='/'){res.setHeader('Content-Type','text/html; charset=utf-8');res.end(fs.readFileSync(path.join(root,'main','setup.html')));return;}
 res.setHeader('Content-Type','application/json');
 if(req.url==='/api/session'){res.end(JSON.stringify({nonce:'fixture-session'}));return;}
 if(req.url==='/api/wifi'){res.statusCode=scanFail?503:200;res.end(JSON.stringify(scanFail?{error:'扫描失败，可手动输入'}:{networks:[{ssid:'家庭网络',rssi:-45,secure:true},{ssid:'<开放网络>',rssi:-61,secure:false}]}));return;}
 if(req.url==='/api/status'){res.end(JSON.stringify({busy:false,online:true,message:mode==='success'?(lastPath==='/api/test'?'Wi-Fi 和 Token 测试通过，尚未保存配置':'配置和模型测试成功，设备即将重新启动'):'密钥无效，请重新配置'}));return;}
 if(req.url==='/api/config'||req.url==='/api/test'){let body='';req.on('data',c=>body+=c);req.on('end',()=>{submitted=JSON.parse(body);lastPath=req.url;if(req.url==='/api/test')tests++;else posts++;assert.equal(req.headers['x-setup-nonce'],'fixture-session');res.statusCode=mode==='save-fail'?503:202;res.end(JSON.stringify(mode==='save-fail'?{error:'设备忙，请重试'}:{accepted:true}));});return;}
 res.statusCode=404;res.end('{}');
});
(async()=>{
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 const browser=await chromium.launch({executablePath:process.env.LEARNING_BROWSER||'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true});
 try{
  const page=await browser.newPage({viewport:{width:390,height:844},locale:'zh-CN'}),errors=[];
  page.on('pageerror',e=>errors.push(e.message));const url=`http://127.0.0.1:${server.address().port}`;
  await page.goto(url);assert.equal(await page.title(),'连接你的学习助手');
  assert.equal(await page.locator('#key').getAttribute('type'),'password');
  await page.locator('.eye[data-for="key"]').click();assert.equal(await page.locator('#key').getAttribute('type'),'text');
  await page.locator('.eye[data-for="key"]').click();
  await page.locator('#scan').click();await page.waitForFunction(()=>document.querySelector('#networks').options.length===3);
  await page.locator('#networks').selectOption('<开放网络>');assert.equal(await page.locator('#ssid').inputValue(),'<开放网络>');
  assert.equal(await page.locator('#networks option').nth(2).textContent(),'<开放网络> · 免密 · -61 dBm');
  scanFail=true;await page.locator('#scan').click();await page.waitForFunction(()=>document.querySelector('#scan-status').textContent.includes('扫描失败'));assert.equal(await page.locator('#ssid').inputValue(),'<开放网络>');scanFail=false;
  await page.locator('#scan').click();await page.waitForFunction(()=>document.querySelector('#scan-status').textContent.includes('已找到'));
  await page.locator('#networks').selectOption('家庭网络');assert.equal(await page.locator('#ssid').inputValue(),'家庭网络');
  await page.locator('#ssid').fill('家庭网络');await page.locator('#password').fill('123');await page.locator('#key').fill('sk-test-placeholder');
  await page.locator('#save').click();await page.waitForFunction(()=>document.querySelector('#status').textContent.includes('8–63'));assert.equal(posts,0);
  await page.locator('#password').fill('12345678');await page.evaluate(()=>document.querySelector('#status').textContent='请保持手机连接设备热点。提示“无法上网”时选择继续连接。');await page.screenshot({path:path.join(out,'phone-390.png'),fullPage:true});
  assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);
  await page.locator('#test').click();await page.waitForFunction(()=>document.querySelector('#status').textContent.includes('尚未保存配置'));
  assert.equal(tests,1);assert.equal(posts,0);assert.equal(await page.locator('#key').inputValue(),'sk-test-placeholder');assert.equal(await page.locator('#password').inputValue(),'12345678');
  await page.locator('#key').fill('\u200Bsk-test-placeholder \uFEFF');
  await page.locator('#save').click();await page.waitForFunction(()=>document.querySelector('#status').textContent.includes('配置和模型测试成功'));
  assert.equal(submitted.ssid,'家庭网络');assert.equal(submitted.region,0);assert.equal(submitted.key,'sk-test-placeholder');
  assert.equal(await page.locator('#key').inputValue(),'');assert.equal(await page.locator('#password').inputValue(),'');
  mode='save-fail';await page.goto(url);await page.locator('#ssid').fill('home');await page.locator('#key').fill('sk-test-placeholder');
  await page.locator('#save').click();await page.waitForFunction(()=>document.querySelector('#status').textContent==='设备忙，请重试');
  assert.equal(await page.locator('#save').isEnabled(),true);
  mode='bad-key';await page.locator('#region').selectOption('1');await page.locator('#save').click();
  await page.waitForFunction(()=>document.querySelector('#status').textContent==='密钥无效，请重新配置');assert.equal(await page.locator('#save').isEnabled(),true);
  assert.equal(submitted.region,1);assert.equal(errors.length,0);
  await page.setViewportSize({width:320,height:720});assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);
  fs.writeFileSync(path.join(out,'result.json'),JSON.stringify({ok:true,checks:['390px/320px layout','Wi-Fi scan and selection','scan failure preserves manual entry','SSID text escaping','test without save retains inputs','secret visibility','password validation','session nonce','submit payload','secret clearing','save failure retry','bad key retry','region'],scope:'Real mobile HTML with mocked HTTP responses; no physical phone/hotspot/NVS/provider verification'},null,2));
  console.log('PASS: mobile setup layout, validation, hidden secrets, request/session, success/failure and retry');
 }finally{await browser.close();server.close();}
})().catch(error=>{console.error(error);server.close();process.exitCode=1;});
