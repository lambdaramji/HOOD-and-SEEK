"""Pi5의 실제 상주 음성노드를 수동 시작/종료하고 localhost에서 처리 결과를 본다."""
import argparse
from collections import deque
import json
import math
from pathlib import Path
import secrets
import shlex
import signal
import subprocess
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

EXECUTABLE = {'FAN_SET','FAN_OFF','IND_ON','IND_OFF','IND_UP','IND_DOWN'}
REASONS = {
    'activation_similarity_below_threshold':'하이엘지 활성어 미확인',
    'activation_only':'활성어만 확인 · 명령 없음',
    'question_or_permission':'질문·허가 표현',
    'negation_quote_compound_or_relative':'부정·인용·복합·상대·상태 표현',
    'target_too_different':'대상이 불명확하거나 유사도 미달',
    'target_required':'대상명 필요 · 끌 기기를 추정하지 않음',
    'no_explicit_action_or_level':'동작 또는 목표 풍량 없음',
    'ambiguous_similar_action':'서로 다른 동작 후보가 비슷함',
    'different_target_commands_too_close':'서로 다른 대상 후보가 비슷함',
    'unsupported_whole_sentence':'지원 문법으로 확정하지 못함',
    'existing_whole_command':'기존 전체 문법 일치',
    'composed_whole_command':'단어·문법 조합 일치',
    'token_composition60':'대상 토큰 조합으로 후보 선정',
    'similar_action_form':'동작 형태 유사도로 후보 선정',
    'similar_level_form':'풍량 단어 유사도로 후보 선정',
    'invalid_text':'문자/길이 경계 검사 탈락',
}


def fields(line):
    if not line.startswith('event='):
        return {}  # journald/PulseAudio 경고는 원문에 남기되 노드 이벤트로 해석하지 않는다.
    try:
        return dict(x.split('=',1) for x in shlex.split(line) if '=' in x)
    except ValueError:
        return {'event':'parse_error','message':line}


def number(value):
    try:
        n=float(value)
        return n if math.isfinite(n) else None
    except (TypeError,ValueError):
        return None


def outcome(event):
    if event.get('event') == 'vad_drop':
        return 'VAD_TOO_LONG'
    if event.get('event') == 'error':
        return 'ERROR'
    if event.get('expired') == '1':
        return 'EXPIRED'
    if event.get('intent') == 'DROP' or event.get('disposition') == 'drop_text':
        return 'DROP'
    if event.get('intent') == 'NONE':
        return 'NONE'
    if event.get('intent') in EXECUTABLE:
        return 'COMMAND_CANDIDATE'
    return 'OTHER'


class LiveNode:
    def __init__(self,args):
        self.args=args
        self.lock=threading.RLock()
        self.process=None
        self.stopping=False
        self.generation=0
        self.state={'phase':'stopped','mode':'observation_only','transmitted':0,
                    'model_loads':0,'asr_calls':0,'rows':deque(maxlen=200),'raw':deque(maxlen=120),
                    'level':{},'config':{},'error':None}
        args.output.mkdir(parents=True,exist_ok=False)
        (args.output/'connection.json').write_text(json.dumps({k:str(v) for k,v in vars(args).items()},ensure_ascii=False,indent=2))

    def snapshot(self):
        with self.lock:
            start=self.state.get('inference_started_local')
            return dict(self.state,rows=list(self.state['rows']),raw=list(self.state['raw']),
                        inference_elapsed_ms=(time.monotonic()-start)*1000 if start else None)

    def consume(self,line):
        event=fields(line)
        kind=event.get('event')
        with self.lock:
            self.state['raw'].append(line)
            if kind=='model_loaded':
                self.state['model_loads']+=1
                self.state['phase']='model_ready'
            elif kind=='recognition_config':
                self.state['config']=event
            elif kind=='capture_ready':
                self.state['phase']='listening'
            elif kind=='audio_level':
                self.state['level']={k:number(event.get(k)) for k in ['dbfs','speech_probability','noise_dbfs','relative_db']}
            elif kind=='asr_started':
                self.state['phase']='recognizing'
                self.state['asr_calls']+=1
                self.state['inference_started_local']=time.monotonic()
            elif kind in ('asr','vad_drop','error','parse_error'):
                row=dict(event,received_at=time.strftime('%H:%M:%S'),status=outcome(event))
                row['reason_text']=REASONS.get(event.get('parser_reason'),event.get('reason',event.get('message','')))
                self.state['rows'].appendleft(row)
                self.state['phase']='error' if kind in ('error','parse_error') else 'listening'
                if kind in ('error','parse_error'):self.state['error']=row['reason_text']
                self.state.pop('inference_started_local',None)
            elif kind=='stopped':
                self.state['phase']='stopped'
                self.state['transmitted']=int(event.get('transmitted','0'))

    def ssh(self):
        return ['ssh','-o','BatchMode=yes','-o','ConnectTimeout=8','-o','ServerAliveInterval=5',
                '-o','ServerAliveCountMax=2',*(['-J',self.args.jump] if self.args.jump else []),self.args.ssh]

    def start(self):
        with self.lock:
            if self.state['phase'] not in ('stopped','error','disconnected'):
                raise ValueError('노드가 이미 시작 중이거나 상주 중입니다.')
            if self.process and self.process.poll() is None:
                raise ValueError('기존 노드 종료를 기다리세요.')
            self.generation+=1
            generation=self.generation
            self.stopping=False
            self.state.update(phase='connecting',error=None)
        threading.Thread(target=self.run,args=(generation,),daemon=True).start()

    def run(self,generation):
        with self.lock:
            if self.stopping:
                self.state['phase']='stopped';return
        a=self.args
        remote=a.remote_root+'/'+str(generation)
        self.remote=remote
        q=shlex.quote
        cmd=[a.observer,'--sensevoice-dir',a.model_dir,'--vad-model',a.vad_model,
             '--pulse-source',a.mic,'--seconds',str(a.seconds)]
        if a.capture_raw:cmd+=['--capture-raw',remote+'/capture.raw']
        script=f'''set -eu
if systemctl is-active --quiet hoodseek-voice-node.service; then printf 'event=error message="voice service already active"\\n'; exit 1; fi
for exe in /proc/[0-9]*/exe; do
 target=$(readlink "$exe" 2>/dev/null || true)
 case "$target" in */hoodseek-voice-observer*) printf 'event=error message="another voice observer is active"\\n'; exit 1;; esac
done
mkdir -p {q(remote)}
cd {q(remote)}
printf '%s  %s\\n' {q(a.expected_sha)} {q(a.observer)} | sha256sum -c - > binary-check.txt
export LD_LIBRARY_PATH={q(a.runtime_lib)}
export LLVM_PROFILE_FILE=/dev/null
pid=''
cleanup() {{ if test -n "$pid"; then kill -INT "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; fi; }}
trap cleanup EXIT HUP INT TERM
{shlex.join(cmd)} 2> node.stderr &
pid=$!
printf '%s\\n' "$pid" > node.pid
set +e
wait "$pid"
rc=$?
pid=''
cat node.stderr
exit "$rc"
'''
        (a.output/f'run-{generation}.sh').write_text(script)
        try:
            with (a.output/f'run-{generation}.stderr').open('x') as errors:
                with self.lock:
                    if self.stopping:
                        self.state['phase']='stopped';return
                    process=subprocess.Popen(self.ssh()+['sh'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,
                        stderr=errors,text=True,bufsize=1)
                    self.process=process
                    process.stdin.write(script);process.stdin.close()
                with (a.output/f'run-{generation}.log').open('x') as log:
                    for line in process.stdout:
                        log.write(line);log.flush();self.consume(line.rstrip('\n'))
                rc=process.wait()
            with self.lock:
                if self.state['phase'] not in ('stopped','error'):
                    self.state['phase']='stopped' if self.stopping or rc==0 else 'disconnected'
                if rc and not self.stopping and not self.state['error']:
                    self.state['error']='SSH/노드 종료 코드 '+str(rc)+' · 로컬 stderr와 Pi5 node.stderr 확인'
        except Exception as error:
            with self.lock:self.state.update(phase='error',error=str(error))

    def stop(self):
        with self.lock:
            self.stopping=True
            if not self.process or self.process.poll() is not None:
                self.state['phase']='stopped';return
            self.state['phase']='stopping'
            remote=self.remote
        q=shlex.quote
        script=f'''set -eu
i=0
while test ! -f {q(remote+'/node.pid')} && test "$i" -lt 100; do sleep .1; i=$((i+1)); done
cd {q(remote)}
pid=$(cat node.pid)
case "$pid" in ''|*[!0-9]*) exit 1;; esac
if test -e "/proc/$pid/exe"; then
 test "$(readlink "/proc/$pid/exe")" = {q(self.args.observer)}
 test "$(readlink "/proc/$pid/cwd")" = "$(pwd -P)"
 kill -INT "$pid"
fi
'''
        completed=subprocess.run(self.ssh()+['sh'],input=script,text=True,capture_output=True,timeout=15)
        if completed.returncode:
            raise ValueError('해당 노드 종료를 확인하지 못했습니다. 다른 프로세스는 종료하지 않았습니다.')


HTML=r'''<!doctype html><html lang="ko"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>하이엘지 · 실시간 음성노드</title>
<style>body{font:16px/1.6 system-ui;margin:24px auto;padding:0 20px;max-width:1250px;color:#203047}button{font:inherit;padding:9px 16px;cursor:pointer}button:disabled{opacity:.45}.cards{display:flex;gap:12px;flex-wrap:wrap}.card{background:#eef4fb;padding:18px;border-radius:12px;flex:1;min-width:170px}.value{font-size:24px;font-weight:bold}.notice{padding:12px;background:#fff1cc}table{border-collapse:collapse;width:100%;margin:20px 0}td,th{border:1px solid #ccd4df;padding:9px;text-align:left;vertical-align:top}th{background:#eef4fb}.COMMAND_CANDIDATE{background:#e6f5ea}.EXPIRED,.VAD_TOO_LONG,.ERROR{background:#ffe5e9}.DROP{background:#f1f1f1}pre{white-space:pre-wrap;max-height:300px;overflow:auto}#error{color:#b12232}meter{width:100%}</style>
<h1>하이엘지 · 실시간 음성노드</h1><p>Pi5의 ReSpeaker를 실제 상주 SenseVoice 노드가 처리합니다. 녹음 파일 업로드·문장별 재시작을 하지 않습니다.</p>
<p class="notice"><b>관측 전용 · 실제 TCP 송신 차단.</b> ‘실행 후보’는 명령 판정이며 하드웨어 실행 완료가 아닙니다. 현재 호출어는 VAD→전사 후 확인합니다. <b>“하이엘지 + 명령”을 한 발화로</b> 말하세요. 호출어만 따로 말하면 다음 발화를 자동 허용하는 대기 세션은 없습니다.</p>
<button id="start">음성노드 수동 시작</button> <button id="stop">음성노드 종료</button> <span id="error" role="alert"></span>
<div class="cards"><div class="card">노드 상태<div class="value" id="phase">연결 확인 중</div></div><div class="card">모델 로딩 / ASR 호출<div class="value" id="counts">0 / 0</div></div><div class="card">마이크 입력<div id="level">—</div><meter id="meter" min="0" max="1" value="0"></meter></div><div class="card">실제 voice 송신<div class="value" id="sent">0</div></div></div>
<p id="configuration"></p><p>발화가 없으면 대기 상태가 정상입니다. 초록은 실행 후보, NONE은 비실행, DROP은 활성어/문자 경계 탈락입니다. 종료→판정 시간은 현재 VAD 기준 추정값이며 공식 원음 끝 기준 성적과 구분합니다.</p>
<table><thead><tr><th>시각 / 판정</th><th>최초 전사</th><th>활성어 · 명령문</th><th>intent / 사유</th><th>처리 시간</th></tr></thead><tbody id="rows"></tbody></table>
<details><summary>실제 노드 원문 로그</summary><pre id="raw"></pre></details>
<script>
const $=x=>document.getElementById(x), token=new URLSearchParams(location.search).get('token');
const phases={stopped:'종료',connecting:'SSH 연결·모델 준비',model_ready:'마이크 준비',listening:'상주 · 입력 대기',recognizing:'전사·명령 판단 중',stopping:'종료 중',error:'오류',disconnected:'연결 끊김'};
const statuses={COMMAND_CANDIDATE:'실행 후보 · 미송신',NONE:'비실행 NONE',DROP:'DROP',EXPIRED:'5초 기한 만료',VAD_TOO_LONG:'VAD 발화 4초 초과',ERROR:'오류',OTHER:'기타'};
async function api(path,body){const r=await fetch(path+'?'+new URLSearchParams({token}),body?{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}:{});const d=await r.json();if(!r.ok)throw Error(d.error||'요청 실패');return d}
function td(row,text){const e=document.createElement('td');e.textContent=text;row.append(e)}
async function poll(){try{const s=await api('/state');$('phase').textContent=(phases[s.phase]||s.phase)+(s.phase==='recognizing'&&s.inference_elapsed_ms>3000?' · 처리 3초 경과':'');$('counts').textContent=s.model_loads+' / '+s.asr_calls;$('sent').textContent=s.transmitted;$('error').textContent=s.error||'';$('start').disabled=!['stopped','error','disconnected'].includes(s.phase);$('stop').disabled=['stopped','stopping'].includes(s.phase);const l=['stopped','disconnected','error'].includes(s.phase)?{}:s.level;$('level').textContent=l.dbfs==null?'입력 대기':l.dbfs.toFixed(1)+' dBFS · 음성확률 '+(l.speech_probability==null?'—':l.speech_probability.toFixed(2));$('meter').value=l.speech_probability||0;$('configuration').textContent='SenseVoiceSmall INT8 · ko/ITN/greedy/ASR CPU4 · VAD CPU1/시작224ms/종료544ms · 팬 켜기 기본1단 · 하이엘지0.60/대상0.60/동작0.70/후보차0.10';$('rows').replaceChildren();for(const x of s.rows){const tr=document.createElement('tr');tr.className=x.status;td(tr,x.received_at+' · '+(statuses[x.status]||x.status));td(tr,x.initial_text||x.text||'—');const a=Number(x.activation_similarity);td(tr,(Number.isFinite(a)&&a>=.6?'하이엘지 확인':'활성어 미확인/해당 없음')+'\n'+(x.normalized_text?'명령문: '+x.normalized_text.replace(/^하이엘지,\s*/, ''):'—'));td(tr,(x.intent||'—')+(x.value?'('+x.value+')':'')+'\n'+(x.reason_text||x.parser_reason||''));td(tr,'ASR '+(x.asr_ms||'—')+' ms\n종료→판정 '+(x.end_to_intent_ms||'—')+' ms'+(Number(x.end_to_intent_ms)>3000?' · 3초 기준 초과':''));$('rows').append(tr)}$('raw').textContent=s.raw.join('\n')}catch(e){$('error').textContent=e.message}finally{setTimeout(poll,400)}}
$('start').onclick=()=>api('/start',{}).catch(e=>$('error').textContent=e.message);$('stop').onclick=()=>api('/stop',{}).catch(e=>$('error').textContent=e.message);poll();
</script></html>'''


def serve(node,port):
    token=secrets.token_urlsafe(24)
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def authorized(self):
            hosts=(f'127.0.0.1:{self.server.server_port}',f'localhost:{self.server.server_port}')
            return (self.headers.get('Host') in hosts and parse_qs(urlparse(self.path).query).get('token')==[token]
                    and (not self.headers.get('Origin') or self.headers['Origin'] in ['http://'+x for x in hosts]))
        def reply(self,data,status=200,kind='application/json; charset=utf-8'):
            payload=(data if isinstance(data,str) else json.dumps(data,ensure_ascii=False,allow_nan=False)).encode()
            self.send_response(status);self.send_header('Content-Type',kind);self.send_header('Cache-Control','no-store');self.send_header('Content-Length',str(len(payload)));self.end_headers();self.wfile.write(payload)
        def do_GET(self):
            if not self.authorized():return self.reply({'error':'접근 거부'},403)
            path=urlparse(self.path).path
            if path=='/':return self.reply(HTML,kind='text/html; charset=utf-8')
            if path=='/state':return self.reply(node.snapshot())
            self.reply({'error':'없는 경로'},404)
        def do_POST(self):
            if not self.authorized():return self.reply({'error':'접근 거부'},403)
            try:
                size=int(self.headers.get('Content-Length','0'))
                if not 1<=size<=1024:raise ValueError('요청 크기 오류')
                if not isinstance(json.loads(self.rfile.read(size)),dict):raise ValueError('객체 필요')
                path=urlparse(self.path).path
                if path=='/start':node.start()
                elif path=='/stop':node.stop()
                else:return self.reply({'error':'없는 경로'},404)
                self.reply(node.snapshot())
            except (ValueError,subprocess.SubprocessError) as error:self.reply({'error':str(error)},400)
    server=ThreadingHTTPServer(('127.0.0.1',port),Handler)
    url=f'http://127.0.0.1:{server.server_port}/?token={token}'
    (node.args.output/'page.json').write_text(json.dumps({'url':url}))
    print('LIVE_PAGE '+url,flush=True)
    return server


def self_test():
    assert outcome({'event':'asr','intent':'FAN_SET'})=='COMMAND_CANDIDATE'
    assert outcome({'event':'asr','intent':'NONE'})=='NONE'
    assert outcome({'event':'asr','intent':'DROP'})=='DROP'
    assert outcome({'event':'asr','intent':'IND_OFF','expired':'1'})=='EXPIRED'
    assert outcome({'event':'vad_drop','reason':'max4s'})=='VAD_TOO_LONG'
    assert fields('event=asr text="하이엘지 팬 꺼줘" intent=FAN_OFF')['text']=='하이엘지 팬 꺼줘'
    assert fields('event=asr text="cut')['event']=='parse_error'
    warning="XDG_RUNTIME_DIR (/run/xdg) is not owned by us (uid 0), but by uid 1004! Don't do that."
    assert fields(warning)=={}
    assert number('nan') is None and number('inf') is None
    with tempfile.TemporaryDirectory() as root:
        node=LiveNode(argparse.Namespace(output=Path(root)/'new-run'))
        for line in ['event=model_loaded', warning, 'event=capture_ready',
                     'event=asr_started model=sensevoice',
                     'event=asr intent=FAN_OFF text="하이엘지 팬 꺼줘" expired=0']:
            node.consume(line)
        state=node.snapshot()
        assert (state['phase'],state['model_loads'],state['asr_calls'],state['transmitted'],
                state['rows'][0]['status'])==('listening',1,1,0,'COMMAND_CANDIDATE')
        assert state['error'] is None and len(state['rows'])==1 and warning in state['raw']
    print('live_observer_checks=11 failures=0')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test',action='store_true')
    parser.add_argument('--ssh');parser.add_argument('--jump')
    parser.add_argument('--observer');parser.add_argument('--expected-sha')
    parser.add_argument('--model-dir');parser.add_argument('--vad-model');parser.add_argument('--runtime-lib')
    parser.add_argument('--mic',default='alsa_input.usb-Seeed_Studio_ReSpeaker_Lite_0000000001-00.analog-stereo')
    parser.add_argument('--output',type=Path)
    parser.add_argument('--remote-root')
    parser.add_argument('--port',type=int,default=0)
    parser.add_argument('--seconds',type=int,default=0)
    parser.add_argument('--capture-raw',action='store_true')
    parser.add_argument('--start',action='store_true',help='페이지 준비 후 실제 노드를 수동 시작')
    args=parser.parse_args()
    if args.self_test:return self_test()
    for key in ['ssh','observer','expected_sha','model_dir','vad_model','runtime_lib','output','remote_root']:
        if not getattr(args,key):parser.error('--'+key.replace('_','-')+' 필요')
    if args.ssh.startswith('-') or any(c.isspace() for c in args.ssh):parser.error('잘못된 SSH 대상')
    if len(args.expected_sha)!=64 or any(c not in '0123456789abcdef' for c in args.expected_sha):parser.error('sha256 필요')
    if not 0<=args.seconds<=600 or args.capture_raw and args.seconds==0:parser.error('원음 보존 시 seconds 1..600 필요')
    node=LiveNode(args);server=serve(node,args.port)
    if args.start:node.start()
    try:server.serve_forever()
    except KeyboardInterrupt:
        node.stop()
        if node.process:
            try:node.process.wait(timeout=15)
            except subprocess.TimeoutExpired:print('노드 종료 미확인 · 다른 프로세스는 종료하지 않음',flush=True)
    finally:server.server_close()


if __name__=='__main__':main()
