"""Windows OpenBW one-shot build, practice and frozen evaluation tools."""
from pathlib import Path
from datetime import datetime,timezone
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,sys,time
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from local_paths import load_paths
PATHS=load_paths()
ROOT=PATHS['runtime_root']
CMAKE=PATHS['cmake']
MSBUILD=PATHS['msbuild']
def now():return datetime.now(timezone.utc).isoformat()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(p,x):p.write_text(json.dumps(x,indent=2,ensure_ascii=False),encoding='utf-8')
def config():return json.loads((ROOT/'environment.json').read_text(encoding='utf-8'))

def build(trial):
    manifest=trial/'manifest.json'
    if manifest.exists():
        m=json.loads(manifest.read_text(encoding='utf-8-sig'))
        deadline = m.get('deadline_utc')
        if deadline:
            # .NET emits seven fractional digits; this Python accepts at most six.
            import re
            deadline = re.sub(r'(\.\d{6})\d+(?=[+-]|Z|$)', r'\1', deadline)
        if deadline and datetime.now(timezone.utc)>=datetime.fromisoformat(deadline):
            raise RuntimeError('Trial deadline passed; only the operator may compile frozen/src')
    version=trial/'builds'/datetime.now(timezone.utc).strftime('%H%M%S-%f')
    version.mkdir(parents=True)
    shutil.copytree(trial/'src',version/'src')
    sources=sorted((version/'src').glob('*.cpp'))
    if not sources:raise RuntimeError('No src/*.cpp')
    api=PATHS['bwapi_include']
    lib=PATHS['bwapi_library']
    libs=ROOT/'sdk'
    bwem=sorted((libs/'bwem/BWEM/src').glob('*.cpp'))
    (version/'Candidate.def').write_text('EXPORTS\ngameInit\nnewAIModule\n')
    lines=['cmake_minimum_required(VERSION 3.1)','project(Candidate LANGUAGES CXX)','set(CMAKE_CXX_STANDARD 17)','add_library(Candidate SHARED Candidate.def']
    lines+=['"'+p.as_posix()+'"' for p in sources+bwem]
    lines+=[')','target_compile_definitions(Candidate PRIVATE NOMINMAX WIN32 NDEBUG _CRT_SECURE_NO_WARNINGS)',
            'target_compile_options(Candidate PRIVATE /bigobj)',
            'target_include_directories(Candidate PRIVATE "'+api.as_posix()+'" "'+(libs/'fap/FAP/include').as_posix()+'" "'+(libs/'bwem/BWEM/include').as_posix()+'")',
            'target_link_libraries(Candidate "'+lib.as_posix()+'")']
    (version/'CMakeLists.txt').write_text('\n'.join(lines),encoding='utf-8')
    env=dict(os.environ,MSBUILDDISABLENODEREUSE='1')
    directory=version/'cmake';directory.mkdir()
    commands=[[str(CMAKE),'-G','Visual Studio 15 2017','-DCMAKE_SYSTEM_VERSION=10.0.17763.0','-DCMAKE_TRY_COMPILE_CONFIGURATION=Release','..'],
              [str(MSBUILD),'Candidate.sln','/p:Configuration=Release','/p:Platform=Win32','/m:1','/nr:false']]
    started=now();code=0
    with (version/'compiler.log').open('wb') as log:
        for cmd in commands:
            code=subprocess.run(cmd,cwd=directory,env=env,stdout=log,stderr=subprocess.STDOUT).returncode
            if code:break
    result={'version':str(version),'started_utc':started,'completed_utc':now(),'exit_code':code,'sources':{p.name:sha(p) for p in (version/'src').iterdir() if p.is_file()}}
    if not code:
        binary=directory/'Release/Candidate.dll'
        shutil.copyfile(binary,version/'Candidate.dll');shutil.copyfile(binary,trial/'Candidate.dll')
        result['binary_sha256']=sha(binary)
        save(trial/'latest-build.json',result)
    save(version/'build.json',result)
    if code:raise RuntimeError('Build failed: '+str(version/'compiler.log')+'\n'+(version/'compiler.log').read_text(errors='replace')[-9000:])
    return result

def play_job(job_path):
    job=json.loads(job_path.read_text(encoding='utf-8'));cfg=config()
    opponent=job.get('operator_fixture') or next(o for o in cfg['opponents'] if o['name']==job['opponent'])
    if not job.get('operator_fixture'):
        assert sha(Path(opponent['entry']))==opponent['binary_sha256'],'Opponent binary changed after configuration'
        if opponent.get('payload'):assert sha(Path(opponent['payload']))==opponent['payload_sha256']
    spec=importlib.util.spec_from_file_location('backend',Path(__file__).with_name('match_backend.py'))
    a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a)
    a.OUT=Path(job['output']);a.BIN=Path(cfg['stock_bin']);a.TIMEOUT_SECONDS=job.get('operator_timeout_seconds',cfg.get('wall_timeout_seconds',2400))
    a.OUT.mkdir(parents=True,exist_ok=True)
    shutil.copytree(ROOT/'maps',a.OUT/'maps')
    original_env=a.base_env
    def base_env():
        e=original_env();e.update(OPENBW_TCP_LISTEN_PORT=str(job['port']),OPENBW_TCP_CONNECT_PORT=str(job['port']),OPENBW_MATCH_SEED=str(job['seed']))
        e['STARBENCH_FRAME_CAP']=str(job.get('frame_cap',86400))
        if job.get('operator_cap_on_score'):e['STARBENCH_CAP_ON_KILL_SCORE']=str(job['operator_cap_on_score'])
        return e
    a.base_env=base_env
    real_popen=subprocess.Popen;java=[];logs=[]
    def popen(args,*pos,**kw):
        side=Path(kw.get('cwd','.')).name
        if str(args[0]).endswith('BWAPILauncher.exe'):
            if side=='player1':
                kw['env']=dict(kw['env'],BWAPI_CONFIG_AI__AI=job['candidate'])
            elif side=='player2':
                args=[opponent['launcher']]+list(args[1:])
                if opponent['kind']=='java':kw['env']=dict(kw['env'],OPENBW_EXPECT_CLIENT='1')
        p=real_popen(args,*pos,**kw)
        if side=='player2' and opponent['kind']=='java':
            cwd=Path(kw['cwd']);log=(cwd/'java.log').open('wb');logs.append(log)
            jar=cwd/'bwapi-data/AI/PurpleWave.jar'
            command=[cfg['java'],'--add-opens=java.base/java.nio=ALL-UNNAMED','-Xms768m','-Xmx768m','-jar',str(jar)]
            java.append(real_popen(command,cwd=cwd,stdout=log,stderr=subprocess.STDOUT,creationflags=0x08000000))
        return p
    a.subprocess.Popen=popen
    # Use the trusted backend's existing lifecycle/replay checks; change only launch order.
    # Isolated worker processes keep per-match configuration and ports independent.
    if not job['candidate_host']:
        import inspect
        code=inspect.getsource(a.play)
        assert '        for side in [1,2]:\n            work' in code
        code=code.replace('        for side in [1,2]:\n            work','        for side in [2,1]:\n            work').replace('            if side==1:\n                time.sleep(0.6)','            if side==2:\n                time.sleep(0.6)')
        exec(compile(code,'trusted_backend_launch_order','exec'),a.__dict__)
    serial=job['seed']-820000
    try:
        r=a.play(dict(opponent,name=job['id'],kind='dll'),job['map'],serial)
    finally:
        for p in java:
            if p.poll() is None:p.kill();p.wait()
        for log in logs:log.close()
    r['id']=job['id'];r['opponent']=opponent['name'];r['candidate_sha256']=sha(Path(job['candidate']));r['candidate_host']=job['candidate_host']
    if not job['candidate_host']:
        r['exit_codes'].reverse();r['exit_codes_hex'].reverse()
        r['forced_stop_sides']=[3-s for s in r['forced_stop_sides']]
    r['actual_seeds']=[side[0].get('actual_seed') if side else None for side in r['starts']]
    r['requested_seeds']=[side[0].get('requested_seed') if side else None for side in r['starts']]
    if None in r['actual_seeds'] or r['actual_seeds'][0]!=r['actual_seeds'][1] or r['requested_seeds']!=[job['seed'],job['seed']]:r['issues'].append('actual_start_seed_mismatch')
    r['outcome']=None
    if not r['issues']:
        if any(r['caps']):
            if not cfg['scoring_ready'] and not job.get('operator_scoring_validation'):r['issues'].append('frame_cap_scoring_not_ready')
            else:
                scores=[(r['caps'][s] or r['ends'][s])[-1]['score'] for s in [0,1]]
                r['cap_scores']=scores;r['outcome']='win' if scores[0]>scores[1] else 'loss' if scores[0]<scores[1] else 'draw'
        else:r['outcome']='win' if r['ends'][0][-1]['winner'] else 'loss'
    if opponent['kind']=='java':
        log=Path(r['directory'])/'player2/java.log'
        text=log.read_text(encoding='utf-8',errors='replace')
        if 'Exception' in text or 'ERROR |' in text:r['issues'].append('java_bot_error');r['outcome']=None
    save(Path(r['directory'])/'result.json',r);save(a.OUT/'result.json',r)
    return r

def run_batch(trial,schedule,output,candidate,workers=2):
    cfg=config();output.mkdir(parents=True)
    frozen=output/'Candidate.dll';shutil.copyfile(candidate,frozen)
    save(output/'schedule.json',schedule)
    running=[];pending=list(schedule);results=[];serial=0
    try:
        while pending or running:
            while pending and len(running)<workers:
                item=pending.pop(0);job=dict(item,candidate=str(frozen),output=str(output/item['id']),port=(27300 if item['id'].startswith('e') else 26300)+2*serial)
                jobpath=output/(item['id']+'.job.json');save(jobpath,job)
                log=(output/(item['id']+'.log')).open('wb')
                p=subprocess.Popen([sys.executable,str(Path(__file__).resolve()),'--job',str(jobpath)],stdout=log,stderr=subprocess.STDOUT,creationflags=0x08000000)
                job['_worker_started']=time.monotonic()
                running.append((p,item,log,job));serial+=1
            for p,item,log,job in running[:]:
                if p.poll() is None:
                    timeout=item.get('operator_timeout_seconds')
                    if not timeout or time.monotonic()-job['_worker_started']<=timeout+30:continue
                    # Bound the operator check even if setup/launcher startup hangs
                    # before the backend reaches its own match timeout loop.
                    job['_worker_timeout']=True
                    subprocess.run(['taskkill.exe','/PID',str(p.pid),'/T','/F'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=10)
                    p.wait(timeout=10)
                log.close();running.remove((p,item,log,job))
                result=Path(job['output'])/'result.json'
                r=json.loads(result.read_text(encoding='utf-8')) if result.exists() else dict(item,outcome=None,issues=['worker_failed'],exit_code=p.returncode)
                if job.get('_worker_timeout'):r['issues'].append('operator_worker_timeout');r['outcome']=None
                results.append(r);save(output/'results.json',results)
            time.sleep(.2)
    finally:
        for p,_,log,_ in running:
            if p.poll() is None:
                subprocess.run(['taskkill.exe','/PID',str(p.pid),'/T','/F'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            p.wait();log.close()
    n=len(schedule);wins=sum(r.get('outcome')=='win' for r in results);losses=sum(r.get('outcome')=='loss' for r in results);draws=sum(r.get('outcome')=='draw' for r in results)
    unresolved=n-wins-losses-draws
    summary={'planned':n,'wins':wins,'losses':losses,'draws':draws,'unresolved':unresolved,'win_rate':wins/n if unresolved==0 else None,'wins_over_planned':wins/n,'output':str(output),'scope':'local versions; tscmoo author candidate; NOT official benchmark','per_opponent':{}}
    for name in dict.fromkeys(x['opponent'] for x in schedule):
        group=[r for r in results if r['opponent']==name]
        summary['per_opponent'][name]={k:sum(r.get('outcome')==k for r in group) for k in ['win','loss','draw']}
        summary['per_opponent'][name]['unresolved']=sum(r.get('outcome') is None for r in group)
    save(output/'summary.json',summary)
    feedback=json.dumps(summary,ensure_ascii=False,indent=2)
    (output/'feedback.txt').write_text(feedback,encoding='utf-8')
    return summary

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--trial',type=Path);p.add_argument('--action',choices=['Build','Practice','Evaluate','Preflight']);p.add_argument('--games',type=int,default=6);p.add_argument('--opponents',default='');p.add_argument('--job',type=Path)
    args=p.parse_args()
    if args.job:play_job(args.job);return
    trial=args.trial.resolve();cfg=config()
    if args.action=='Build':print(json.dumps(build(trial)));return
    if not cfg['ready']:raise RuntimeError('Roster/scoring acceptance is not complete; see readiness report')
    names=args.opponents.split(',') if args.opponents else [o['name'] for o in cfg['opponents']]
    assert names and all(n in [o['name'] for o in cfg['opponents']] for n in names)
    if args.action=='Evaluate':
        if len(names)!=10:raise RuntimeError('Evaluation requires the fixed ten-entry local roster')
        if not (trial/'frozen.json').exists():raise RuntimeError('Evaluate only operator-frozen code')
        frozen=json.loads((trial/'frozen.json').read_text(encoding='utf-8-sig'));assert sha(trial/'Candidate.dll')==frozen['binary_sha256']
        assert sha(ROOT/'environment.json')==frozen['environment_sha256'].lower()
        count=180;serial=0;seedbase=9100000
        schedule=[]
        for name in names:
            for m in cfg['maps']:
                for j in range(6):
                    schedule.append({'id':'e%03d'%serial,'opponent':name,'map':m,'seed':seedbase+serial,'candidate_host':j%2==0});serial+=1
        output=trial/'evaluation'
    else:
        if not 1<=args.games<=12:raise RuntimeError('Practice supports 1..12 games')
        built=build(trial);serial=len(list((trial/'practice').glob('*'))) if (trial/'practice').exists() else 0
        schedule=[{'id':'g%03d'%i,'opponent':names[(serial*args.games+i)%len(names)],'map':cfg['maps'][i%3],'seed':1200000+serial*100+i,'candidate_host':i%2==0} for i in range(args.games)]
        if args.action=='Preflight':
            for item in schedule:
                item.update(frame_cap=1200,operator_timeout_seconds=120)
        output=trial/'practice'/('batch-%03d'%serial)
    print(json.dumps(run_batch(trial,schedule,output,trial/'Candidate.dll',workers=cfg['workers']),ensure_ascii=False))
if __name__=='__main__':main()
