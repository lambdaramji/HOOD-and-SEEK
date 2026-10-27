"""ReSpeaker 현장 시험의 첫 시도·원문·실제 발화 끝을 보존해 집계한다. 자동 발행하지 않는다."""
import argparse
from collections import Counter
import html
import json
from pathlib import Path
import shlex
import tempfile

EXECUTABLE = {'FAN_SET', 'FAN_OFF', 'IND_ON', 'IND_OFF', 'IND_UP', 'IND_DOWN'}


def events(text):
    result = []
    for line in text.splitlines():
        if not line.startswith('event='):
            continue
        try:
            result.append(dict(part.split('=', 1) for part in shlex.split(line) if '=' in part))
        except ValueError:
            raise ValueError('관측 로그가 잘렸거나 손상됐습니다. 원문을 확인하세요.')
    return result


def first_attempts(session):
    """재녹음 성공으로 첫 실패를 덮어쓰지 않는다."""
    first = {}
    for path in sorted((session / 'attempts').glob('*/metadata.json'),
                       key=lambda p: json.loads(p.read_text())['capture_requested_at']):
        data = json.loads(path.read_text())
        first.setdefault(data['requested_condition']['source_id'], (path.parent, data))
    return first


def score(prompt, log, duration, annotation, capture_ok, publish_required=False):
    rows = events(log)
    outputs = [row for row in rows if row.get('event') == 'asr']
    candidates = [row for row in outputs if row.get('intent') in EXECUTABLE]
    origin = next((int(row['sample0_monotonic_ns']) for row in rows
                   if row.get('event') == 'capture_audio_origin'), None)
    reviewed = annotation.get('listening') == 'intact' and annotation.get('spoken_as_prompt') is True
    correct = len(candidates) == 1 and candidates[0]['intent'] == prompt['expected']
    if correct and prompt.get('value') is not None:
        correct = candidates[0].get('value') == str(prompt['value'])
    latency = None
    end = annotation.get('speech_end_s')
    if (correct and origin is not None and isinstance(end, (int, float)) and
            not isinstance(end, bool) and 0 <= end <= duration and
            'intent_monotonic_ns' in candidates[0]):
        latency = (int(candidates[0]['intent_monotonic_ns']) - origin) / 1e6 - end * 1000
    if latency is not None and latency < 0:
        latency = None
    publish_required = publish_required or any(row.get('disposition') == 'sent' for row in outputs)
    core = annotation.get('core_result', {})
    if not isinstance(core, dict):
        core = {}
    evidence = core.get('evidence')
    core_ok = (core.get('source_id') == prompt['source_id'] and
               core.get('last_intent', core.get('intent')) == prompt['expected'] and core.get('accepted') is True and
               isinstance(evidence, str) and bool(evidence.strip()))
    if prompt.get('value') is not None:
        core_ok = core_ok and type(core.get('value')) is int and core['value'] == prompt['value']
    elif core.get('value') is not None:
        core_ok = False
    sent = len(candidates) == 1 and candidates[0].get('disposition') == 'sent'
    timely = bool(capture_ok and reviewed and correct and latency is not None and latency <= 3000)
    kind = prompt['kind']
    return dict(source_id=prompt['source_id'], kind=kind, expected=prompt['expected'],
                value=prompt.get('value'), capture_ok=capture_ok, listening_reviewed=reviewed,
                duration_s=duration, execution_candidates=len(candidates), outputs=outputs,
                correct_candidate=bool(capture_ok and correct),
                actual_end_to_intent_ms=latency,
                timely_correct_candidate=timely,
                publication_required=bool(publish_required), sent=sent,
                core_acceptance_reviewed=bool(core_ok), core_result=core,
                timely_correct=bool(timely and (not publish_required or (sent and core_ok))),
                negative_ok=bool(kind == 'negative' and capture_ok and reviewed and not candidates),
                ambient_ok=bool(kind == 'ambient' and capture_ok and reviewed and not candidates and duration >= 150
                                and annotation.get('actual_condition_verified') is True),
                policy_pending=kind == 'policy_diagnostic' and prompt['expected'] == 'POLICY_PENDING')


def summarize(session, annotations, destination):
    import wave
    data = json.loads((session / 'session.json').read_text())
    first = first_attempts(session)
    result = []
    inventories = set()
    for prompt in data['tasks']:
        selected = first.get(prompt['source_id'])
        log, duration, ok = '', 0, False
        if selected:
            folder, metadata = selected
            if (folder / 'observer.log').exists():
                log = (folder / 'observer.log').read_text()
            if (folder / 'original-mono.wav').exists():
                with wave.open(str(folder / 'original-mono.wav')) as audio:
                    duration = audio.getnframes() / audio.getframerate()
            if (folder / 'observer.exit').exists():
                ok = (folder / 'observer.exit').read_text().strip() == '0' and duration > 0
            inventory = folder / 'environment.sha256'
            if inventory.exists():
                inventories.add(tuple(line.split()[0] for line in inventory.read_text().splitlines()))
            else:
                ok = False
        result.append(score(prompt, log, duration, annotations.get(prompt['source_id'], {}), ok,
                            publish_required=data.get('publish') is True))
    required = [row for row in result if row['kind'] == 'supported']
    negatives = [row for row in result if row['kind'] == 'negative']
    ambient = [row for row in result if row['kind'] == 'ambient']
    frequencies = Counter(row['expected'] + (':'+str(row['value']) if row['value'] is not None else '')
                          for row in required)
    expected = {'FAN_SET:1':5,'FAN_SET:2':5,'FAN_SET:3':5,'FAN_OFF':5,'IND_OFF':5}
    summary = dict(official_status='NOT_VERIFIED_PM_AND_LIVE_REVIEW_REQUIRED',
        speaker=data['speaker_id'], planned=len(result), attempted=len(first),
        supported_correct=sum(row['correct_candidate'] for row in required),
        supported_denominator=len(required), timely_correct=sum(row['timely_correct'] for row in required),
        nfr25_counts_complete=frequencies == expected and len(required) == 25,
        nfr25_numeric_pass=bool(frequencies == expected and len(required) == 25 and
                               sum(row['timely_correct'] for row in required) >= 20 and len(inventories) == 1),
        negatives_reviewed_ok=sum(row['negative_ok'] for row in negatives),
        negative_denominator=len(negatives), negative_execution_candidates=sum(row['execution_candidates'] for row in negatives),
        negative_numeric_pass=bool(len(negatives) >= 36 and all(row['negative_ok'] for row in negatives) and len(inventories) == 1),
        ambient_seconds=sum(row['duration_s'] for row in ambient),
        ambient_numeric_pass=bool(len(ambient) == 2 and all(row['ambient_ok'] for row in ambient)
                                  and sum(row['duration_s'] for row in ambient) >= 300 and len(inventories) == 1),
        distinct_binary_model_inventories=len(inventories),
        actual_publication=('CORE_RESULTS_REQUIRE_MANUAL_TRIAL_MATCHING'
                            if any(row['publication_required'] for row in result)
                            else 'NOT_TESTED_OBSERVATION_ONLY'),
        sent_supported=sum(row['sent'] for row in required),
        core_acceptance_reviewed=sum(row['core_acceptance_reviewed'] for row in required), trials=result)
    destination.mkdir(parents=True, exist_ok=False)
    (destination/'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k != 'trials'}, ensure_ascii=False, indent=2))


def review(session, destination):
    data = json.loads((session/'session.json').read_text())
    first = first_attempts(session)
    cards = []
    for prompt in data['tasks']:
        selected = first.get(prompt['source_id'])
        if not selected:
            continue
        folder, _ = selected
        wav = folder/'original-mono.wav'
        if not wav.exists():
            continue
        import os
        src = html.escape(os.path.relpath(wav, destination.parent), quote=True)
        ident = html.escape(prompt['source_id'], quote=True)
        cards.append(f'<section data-id="{ident}"><h2>{ident} · {html.escape(prompt["text"])}</h2>'
            f'<audio controls src="{src}"></audio><p><label>청취 <select class="listening">'
            '<option value="pending">미확인</option><option value="intact">잘림 없음</option>'
            '<option value="unclear">불명확/잘림</option></select></label> '
            '<label><input type="checkbox" class="spoken">표시된 문장을 한 번 말함(주변음은 명령 발화 없음)</label></p>'
            '<label>원음의 마지막 발음 끝(초) <input class="end" type="number" min="0" step="0.01"></label>'
            '<button class="mark">현재 재생 위치를 끝으로 표시</button> '
            '<label><input type="checkbox" class="condition">주변음의 실제 조용함/소음 조건 확인</label>'
            '<p><label>발행 시험만: 이 문항에 대조한 Core 결과 JSON '
            '<textarea class="core" rows="3" aria-label="Core 결과와 원문 근거"></textarea></label>'
            '(source_id, last_intent, value, accepted, evidence). 예전 마지막 결과를 재사용하지 않습니다.</p></section>')
    page = '<!doctype html><html lang="ko"><meta charset="utf-8"><title>PM 시험 원음 검수</title>'
    page += '<style>body{font:17px/1.6 system-ui;max-width:1000px;margin:30px auto;padding:20px}section{padding:20px;border:1px solid #aaa;margin:15px 0}audio{width:100%}button,input,select{font:inherit}</style>'
    page += '<h1>첫 시도 원음 검수</h1><p>마지막 단어·부정문의 마를 확인합니다. 발화 끝은 VAD 추정 대신 원음 재생 위치로 지정하세요. 정책 진단 문항은 필수25회와 분리합니다. 발행 시험은 원문 Core 결과를 문항과 대조하고 JSON 및 근거 위치를 입력하세요. 팬 실제 단수는 정답 비교에 쓰지 않습니다.</p>'
    page += ''.join(cards) + '<button id="save">검수 결과 다운로드</button><script>'
    page += "document.querySelectorAll('.mark').forEach(b=>b.onclick=()=>{const s=b.closest('section');s.querySelector('.end').value=s.querySelector('audio').currentTime.toFixed(2)});"
    page += "document.getElementById('save').onclick=()=>{try{const out={};document.querySelectorAll('section').forEach(s=>{const e=s.querySelector('.end').value;const c=s.querySelector('.core').value.trim();out[s.dataset.id]={listening:s.querySelector('.listening').value,spoken_as_prompt:s.querySelector('.spoken').checked,actual_condition_verified:s.querySelector('.condition').checked,speech_end_s:e===''?null:Number(e),core_result:c?JSON.parse(c):{}}});const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([JSON.stringify(out,null,2)],{type:'application/json'}));a.download='hoodseek-pm-listening.json';a.click()}catch(e){alert('Core 결과 JSON 형식을 확인하세요: '+e.message)}};</script></html>"
    with destination.open('x') as stream:
        stream.write(page)


def self_test():
    prompt = dict(source_id='R11',kind='supported',expected='FAN_SET',value=3)
    annotation = dict(listening='intact',spoken_as_prompt=True,speech_end_s=1)
    log = ('event=capture_audio_origin sample0_monotonic_ns=1000000000\n'
           'event=asr intent=FAN_SET value=3 intent_monotonic_ns=3000000000 text="하이엘지 후드 강풍"\n')
    assert score(prompt,log,3,annotation,True)['timely_correct']
    assert not score(prompt,log+log.splitlines()[-1],3,annotation,True)['correct_candidate']
    assert not score(prompt,log.replace('value=3','value=2'),3,annotation,True)['correct_candidate']
    assert not score(prompt,'',3,annotation,True)['correct_candidate']
    assert not score(prompt,log.replace('3000000000','6000000001'),7,annotation,True)['timely_correct']
    assert not score(prompt,log,3,annotation,False)['timely_correct']
    sent_log = log.replace('event=asr ', 'event=asr disposition=sent ')
    core_annotation = dict(annotation, core_result=dict(source_id='R11', intent='FAN_SET', value=3,
                                                       accepted=True, evidence='core-state.log:17'))
    assert not score(prompt,sent_log,3,annotation,True)['timely_correct']
    assert score(prompt,sent_log,3,core_annotation,True,True)['timely_correct']
    for patch in [{'accepted':False},{'source_id':'R12'},{'value':2},{'evidence':''}]:
        ann = dict(core_annotation, core_result=dict(core_annotation['core_result'], **patch))
        assert not score(prompt,sent_log,3,ann,True,True)['timely_correct']
    assert not score(prompt,log,3,core_annotation,True,True)['timely_correct']
    negative = dict(source_id='N01',kind='negative',expected='NO_EXEC')
    assert score(negative,'',3,annotation,True)['negative_ok']
    assert not score(negative,log,3,annotation,True)['negative_ok']
    assert not score(negative,'',3,{},True)['negative_ok']
    ambient = dict(source_id='A01',kind='ambient',expected='NO_EXEC')
    ann = dict(annotation,actual_condition_verified=True)
    assert score(ambient,'',150,ann,True)['ambient_ok']
    assert not score(ambient,'',149.9,ann,True)['ambient_ok']
    with tempfile.TemporaryDirectory() as tmp:
        root=Path(tmp)
        for n,date in [(2,'2026-10-24T12:01'),(1,'2026-10-24T12:00')]:
            p=root/'attempts'/str(n);p.mkdir(parents=True)
            (p/'metadata.json').write_text(json.dumps(dict(capture_requested_at=date,requested_condition={'source_id':'R11'})))
        assert first_attempts(root)['R11'][0].name == '1'
    print('field_evaluation_checks=19 failures=0')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['summarize','review','self-test'])
    parser.add_argument('--session',type=Path)
    parser.add_argument('--annotations',type=Path)
    parser.add_argument('--out',type=Path)
    args=parser.parse_args()
    if args.command=='self-test':
        return self_test()
    if args.session is None or args.out is None:
        parser.error('--session and --out required')
    if args.command=='review':
        return review(args.session,args.out)
    annotations=json.loads(args.annotations.read_text()) if args.annotations else {}
    summarize(args.session,annotations,args.out)


if __name__=='__main__':
    main()
