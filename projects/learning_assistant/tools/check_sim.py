"""Drive the actual native C backend through the pinned simulator's input API."""
from pathlib import Path
import argparse,json,os,socket,subprocess,time,hashlib

ROOT=Path(__file__).resolve().parents[1]
class Rpc:
    def __init__(self,url):
        host,port=url.removeprefix('tcp://').rsplit(':',1)
        self.sock=socket.create_connection((host,int(port)),timeout=10)
        self.stream=self.sock.makefile('rwb',buffering=0);self.counter=0
    def call(self,method,params=None):
        self.counter+=1
        body=json.dumps(dict(jsonrpc='2.0',id=self.counter,method=method,params=params or {})).encode()
        self.stream.write(f'Content-Length: {len(body)}\r\n\r\n'.encode()+body)
        while True:
            headers={}
            while (line:=self.stream.readline()) not in (b'\r\n',b'\n'):
                if not line:raise RuntimeError('Simulator disconnected')
                k,v=line.decode().split(':',1);headers[k.lower()]=v.strip()
            n=int(headers['content-length']);chunks=[]
            while n:
                part=self.stream.read(n)
                if not part:raise RuntimeError('Simulator disconnected')
                chunks.append(part);n-=len(part)
            result=json.loads(b''.join(chunks))
            if result.get('id')==self.counter:
                if 'error' in result:raise RuntimeError(result['error'])
                return result.get('result')
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--host',type=Path,required=True);parser.add_argument('--grade-only',action='store_true');args=parser.parse_args()
    out=ROOT/'build-sim/evidence';out.mkdir(parents=True,exist_ok=True)
    ready=out/'ready.json';ready.unlink(missing_ok=True)
    trace=out/'trace.json';trace.unlink(missing_ok=True)
    key=out/'key.txt';key.write_text('0')
    failure=out/'failure.txt';failure.write_text('0')
    connection=out/'connection.txt';connection.write_text('2')
    questions=out/'questions.txt';questions.write_text('0')
    env=os.environ.copy();env['LEARNING_SIM_KEY']=str(key);env['LEARNING_SIM_TRACE']=str(trace)
    env['LEARNING_SIM_QUESTIONS']=str(questions)
    env['LEARNING_SIM_FAILURE']=str(failure);env['LEARNING_SIM_CONNECTION']=str(connection)
    hostlog=(out/'host.log').open('wb');backendlog=(out/'backend.log').open('wb')
    host=subprocess.Popen([str(args.host),'--bundle',str(ROOT/'build-sim/learning_backend_assets/app.gspb'),
        '--headless','--api-listen','tcp://127.0.0.1:0','--backend-enable','--backend-required',
        '--dynamic-font',str(ROOT/'ui/fonts/LearningSans.ttf'),'--ready-file',str(ready)],stdout=hostlog,stderr=subprocess.STDOUT)
    backend=None;rpc=None;checks=[]
    try:
        deadline=time.monotonic()+30
        while not ready.exists():
            if host.poll() is not None:raise RuntimeError('Host exited; inspect host.log')
            if time.monotonic()>deadline:raise TimeoutError('Host startup')
            time.sleep(.05)
        addresses=json.loads(ready.read_text());print('Ready:',addresses)
        api=addresses.get('api_endpoint') or addresses.get('api')
        back=addresses.get('backend_endpoint') or addresses.get('backend')
        if not api or not back:raise RuntimeError('Missing ready endpoints')
        backend=subprocess.Popen([str(ROOT/'build-sim/learning_backend.exe'),'--endpoint',back],env=env,stdout=backendlog,stderr=subprocess.STDOUT)
        rpc=Rpc(api)
        def state(page,**wanted):
            deadline=time.monotonic()+8
            while time.monotonic()<deadline:
                try:s=json.loads(trace.read_text())
                except (FileNotFoundError,json.JSONDecodeError):s={}
                if s.get('page')==page and all(s.get(k)==v for k,v in wanted.items()):
                    time.sleep(.15);return s
                time.sleep(.05)
            raise AssertionError((page,wanted,s))
        def tap(x,y):
            rpc.call('feed_pointer',dict(x=x,y=y,pressed=True));time.sleep(.08)
            rpc.call('feed_pointer',dict(x=x,y=y,pressed=False));time.sleep(.25)
        def drag(x1,y1,x2,y2):
            for i in range(7):
                rpc.call('feed_pointer',dict(x=round(x1+(x2-x1)*i/6),y=round(y1+(y2-y1)*i/6),pressed=True));time.sleep(.06)
            rpc.call('feed_pointer',dict(x=x2,y=y2,pressed=False));time.sleep(.35)
        def capture(name):rpc.call('screenshot',dict(path=str(out/(name+'.png')),format='png'));checks.append(name)
        def check_grades():
            tap(240,216);capture('29-grade-open')
            tap(120,270);state(6,grade=4)
            tap(240,216);tap(120,270);state(6,grade=1)
            tap(240,216)
            for _ in range(5):drag(240,411,240,268)
            capture('30-grade-high-school');tap(120,414);state(6,grade=12)
            capture('31-grade-selected')
        if args.grade_only:
            state(0);drag(240,2,240,465);state(0,drawer=True);tap(348,384);state(6)
            check_grades()
            print('PASS: grade menu scroll and selection');return
        state(0,online=True,api_state=1);capture('01-home')
        connection.write_text('0');state(0,online=False,api_state=0);capture('18-home-offline')
        connection.write_text('1');state(0,online=True,api_state=0);capture('19-home-api-unverified')
        connection.write_text('3');state(0,online=True,api_state=2);capture('20-home-api-error')
        connection.write_text('2');state(0,online=True,api_state=1)
        drag(240,2,240,465);state(0,drawer=True);capture('14-quick-settings')
        drag(360,148,180,148);s=state(0,drawer=True);assert 10<=s['brightness']<70,s
        drag(240,242,60,242);s=state(0,drawer=True);assert s['volume']<20,s
        tap(412,300);capture('14a-toggle-click');state(0,drawer=True,muted=True)
        tap(412,300);state(0,drawer=True,muted=False)
        tap(130,384);state(0,drawer=True,automatic=False);capture('15-quick-fixed')
        tap(130,384);state(0,drawer=True,automatic=True)
        drag(60,242,240,242)
        drag(240,470,240,12);state(0,drawer=False);capture('16-quick-closed')
        tap(140,290);state(1);capture('02-camera')
        tap(86,400);state(1,light=True)
        tap(394,400);state(1,light=True,flipped=True);capture('28-camera-light-flipped')
        tap(240,399);state(2);capture('03-confirm')
        tap(90,405);state(1,light=False,flipped=True)
        tap(394,400);state(1,flipped=False)
        tap(240,399);state(2)
        answers=state(2)['answer_requests'];reads=state(2)['speech_requests']
        tap(330,405);s=state(9,question_count=6,question_page=0);capture('33-question-picker')
        time.sleep(.5);s=state(9);assert s['answer_requests']==answers and s['speech_requests']==reads
        tap(82,420);state(9,question_page=0)
        tap(390,420);state(9,question_page=1);capture('34-question-picker-next')
        tap(390,420);state(9,question_page=1)
        key.write_text('1');time.sleep(.3);key.write_text('0');state(9,recording=False)
        tap(82,420);state(9,question_page=0)
        tap(240,266);s=state(3,selected_question=2,answer_requests=answers+1);assert s['page_count']>=2;capture('04-reading')
        tap(303,39);state(9,question_count=6)
        assert state(9)['answer_requests']==answers+1
        tap(240,189);s=state(3,selected_question=1,answer_requests=answers+2)

        generation=s['generation'];index=s['page_index']
        tap(76,347);s=state(3,page_index=0);assert s['generation']==generation
        drag(240,2,240,465);state(3,drawer=True);capture('17-quick-over-reading')
        tap(412,300);state(3,drawer=True,muted=True)
        drag(240,470,240,12);s=state(3,drawer=False);assert s['generation']==generation and s['page_index']==index
        reads=s['speech_requests'];tap(100,414);state(3,muted=True,speech_requests=reads+1);capture('27-manual-read-while-muted')
        tap(390,347);state(3,page_index=1);capture('05-next-page')
        tap(76,347);state(3,page_index=0)
        for i in range(1,s['page_count']):tap(390,347);state(3,page_index=i)
        last=state(3);tap(390,347);s=state(3);assert s['generation']==last['generation'];capture('21-last-page')
        for i in range(s['page_count']-2,-1,-1):tap(76,347);state(3,page_index=i)
        s=state(3);answers=s['answer_requests'];reads=s['speech_requests'];tap(410,39);state(3,answer_requests=answers+1,muted=True);capture('06-explain')
        time.sleep(.3);assert state(3)['speech_requests']==reads,'Muted answer unexpectedly read automatically'
        tap(340,414);state(4);capture('06a-followup')
        tap(110,422);state(3)
        tap(340,414);state(4);key.write_text('1');state(4,recording=True)
        time.sleep(.6);key.write_text('0');s=state(8);capture('22-followup-review')
        answers=s['answer_requests'];time.sleep(.5);assert state(8)['answer_requests']==answers
        tap(340,420);state(3,answer_requests=answers+1);capture('06b-followup-answer')
        tap(48,38);state(9);tap(48,38);state(0)
        drag(240,2,240,465);state(0,drawer=True);tap(348,384);state(6,drawer=False);capture('07-settings')
        check_grades()
        tap(48,38);state(0)
        tap(365,290);state(4);capture('08-voice-ready')
        key.write_text('1');state(4,recording=True);capture('09-listening')
        frames=[]
        for frame in range(4):
            capture(f'25-recording-motion-{frame}');frames.append(hashlib.sha256((out/f'25-recording-motion-{frame}.png').read_bytes()).hexdigest());time.sleep(.13)
        assert len(set(frames))>1,'Recording waveform did not animate'
        before=state(4,recording=True)
        drag(240,2,240,465);state(4,drawer=True,recording=True)
        tap(412,300);state(4,recording=True,muted=not before['muted'])
        tap(412,300);state(4,recording=True,muted=before['muted'])
        drag(240,470,240,12);s=state(4,drawer=False,recording=True);assert s['generation']==before['generation']
        time.sleep(.6);key.write_text('0');s=state(8);capture('23-voice-review');answers=s['answer_requests']
        assert s['review_count']>=2
        tap(390,354);state(8,review_index=1);capture('24-review-next-page')
        tap(76,354);state(8,review_index=0)
        tap(110,420);state(4);assert state(4)['answer_requests']==answers
        capture('26-idle-wave-a');time.sleep(.2);capture('26-idle-wave-b')
        assert (out/'26-idle-wave-a.png').read_bytes()==(out/'26-idle-wave-b.png').read_bytes(),'Idle waveform still animates'
        # Both the waveform and the on-screen button own tap-to-record.
        tap(240,124);before=state(4,recording=True)
        key.write_text('1');time.sleep(.4);key.write_text('0');s=state(4,recording=True);assert s['record_requests']==before['record_requests']
        tap(330,424);state(8);assert state(8)['answer_requests']==answers
        tap(110,420);state(4)
        tap(330,424);state(4,recording=True);time.sleep(.6);tap(240,124);state(8)
        time.sleep(.5);assert state(8)['answer_requests']==answers
        tap(340,420);state(3,answer_requests=answers+1);capture('10-voice-answer')
        tap(48,38);state(0)
        tap(140,290);state(1);tap(240,399);state(2);tap(48,38);state(0)
        failure.write_text('1');tap(140,290);state(7);capture('11-camera-error')
        failure.write_text('0');tap(380,369);state(1);tap(48,38);state(0)
        tap(140,290);state(1);failure.write_text('2');tap(86,400);state(7,light=False);capture('32-light-error')
        failure.write_text('0');tap(380,369);state(1,light=False);tap(48,38);state(0)
        # One candidate must still wait for a click; empty/failure never solves.
        questions.write_text('1');tap(140,290);state(1);tap(240,399);state(2)
        answers=state(2)['answer_requests'];tap(330,405);state(9,question_count=1);capture('35-question-single')
        time.sleep(.5);assert state(9)['answer_requests']==answers
        tap(240,266);state(9);tap(390,420);state(9,question_page=0)
        drag(240,2,240,465);state(9,drawer=True);tap(348,384);state(6)
        tap(48,38);state(9,question_count=1)
        tap(240,189);state(3,answer_requests=answers+1);tap(48,38);state(9)
        tap(240,460);state(1);tap(240,399);state(2)
        questions.write_text('2');tap(330,405);state(7);capture('36-question-unclear')
        assert state(7)['answer_requests']==answers+1
        questions.write_text('0');tap(380,369);state(9,question_count=6);capture('37-question-retry')
        tap(48,38);state(0)
        (out/'result.json').write_text(json.dumps(dict(ok=True,screens=checks,
            evidence='Native C UI with simulated camera/audio/cloud only; actual pointer input and AI key fixture'),ensure_ascii=False,indent=2))
        assert 'learning_ui: error=' not in (out/'backend.log').read_text(errors='replace')
        print('PASS:',len(checks),'screens; retake, page navigation, explain, mute, AI press/release and back')
    finally:
        if rpc:
            try:rpc.call('quit')
            except Exception:pass
        for process in (backend,host):
            if process and process.poll() is None:
                process.terminate()
                try:process.wait(timeout=5)
                except subprocess.TimeoutExpired:process.kill();process.wait()
        hostlog.close();backendlog.close()
if __name__=='__main__':main()
