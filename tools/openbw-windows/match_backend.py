"""Match lifecycle and replay checks used by the Windows OpenBW practice tool."""
from pathlib import Path
import ctypes
from datetime import datetime, timezone
import hashlib
import json
import os
import re
import shutil
import subprocess
import time

import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from local_paths import load_paths
PATHS = load_paths()
OUT = PATHS['runtime_root'] / 'backend-matches'
PROBE = PATHS['probe_root']
BIN = PROBE / 'engine/bin/Release'
TIMEOUT_SECONDS = 600
ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)

def save(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False), encoding='utf-8')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def events(path):
    if not path.exists():
        return []
    return [json.loads(s) for s in path.read_text(encoding='utf-8').splitlines() if s.strip()]

def base_env():
    env = {k:v for k,v in os.environ.items() if not k.startswith(('BWAPI_CONFIG_', 'OPENBW_', 'STARBENCH_'))}
    env.update({'OPENBW_ENABLE_UI':'0', 'OPENBW_MPQ_PATH':str(PROBE/'player1'),
                'OPENBW_LAN_MODE':'TCP', 'OPENBW_TCP_LISTEN_HOSTNAME':'127.0.0.1',
                'OPENBW_TCP_CONNECT_HOSTNAME':'127.0.0.1', 'OPENBW_TCP_LISTEN_PORT':'26114',
                'OPENBW_TCP_CONNECT_PORT':'26114', 'BWAPI_CONFIG_AUTO_MENU__AUTO_MENU':'LAN',
                'BWAPI_CONFIG_AUTO_MENU__GAME_TYPE':'MELEE', 'BWAPI_CONFIG_AUTO_MENU__AUTO_RESTART':'OFF',
                'BWAPI_CONFIG_AUTO_MENU__RACE':'Protoss', 'BWAPI_CONFIG_STARCRAFT__SPEED_OVERRIDE':'0',
                'BWAPI_CONFIG_STARCRAFT__SOUND':'OFF', 'BWAPI_CONFIG_AI__TOURNAMENT':str(BIN/'ProbeReferee.dll'),
                'STARBENCH_FRAME_CAP':'86400'})
    return env

def check_replay(replay, folder):
    (folder/'bwapi-data').mkdir(parents=True)
    shutil.copyfile(PATHS['bwapi_ini'], folder/'bwapi-data/bwapi.ini')
    env = base_env()
    env.update({'BWAPI_CONFIG_AUTO_MENU__AUTO_MENU':'SINGLE_PLAYER',
                'BWAPI_CONFIG_AUTO_MENU__MAP':str(replay), 'BWAPI_CONFIG_AUTO_MENU__SAVE_REPLAY':'',
                'BWAPI_CONFIG_AI__TOURNAMENT':'', 'BWAPI_CONFIG_AI__AI':str(BIN/'ProbeReplayCheck.dll')})
    started = time.monotonic()
    with (folder/'playback.log').open('wb') as log:
        p = subprocess.Popen([str(BIN/'BWAPILauncher.exe')],cwd=folder,env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=0x08000000)
        try:
            p.wait(timeout=90)
        except subprocess.TimeoutExpired:
            p.kill(); p.wait()
    ev = [json.loads(s) for s in (folder/'playback.log').read_text(errors='replace').splitlines() if s.startswith('{"event":')]
    starts = [e for e in ev if e['event']=='replay_start']
    ends = [e for e in ev if e['event']=='replay_end']
    result = {'passed':bool(p.returncode==0 and starts and starts[-1]['is_replay'] and ends and ends[-1]['frames']>=starts[-1]['expected_frames']-1),
              'exit_code':p.returncode,'events':ev,'wall_seconds':round(time.monotonic()-started,3)}
    save(folder/'verification.json',result)
    return result

def play(opponent, map_item, serial):
    name = opponent['name']
    directory = OUT/'matches'/('%02d-%s-%s'%(serial,name,map_item['id']))
    directory.mkdir(parents=True)
    print(json.dumps({'event':'match_start','opponent':name,'map':map_item['id'],'directory':str(directory)}),flush=True)
    env = base_env()
    env.update({'BWAPI_CONFIG_AUTO_MENU__MAP':str(OUT/'maps'/map_item['name']),
                'BWAPI_CONFIG_STARCRAFT__SEED_OVERRIDE':str(820000+serial)})
    processes, logs = [], []
    record = {'opponent':name,'map':map_item['id'],'directory':str(directory),'seed':820000+serial,
              'binary_sha256':sha(Path(opponent['entry'])),'status':'starting','issues':[]}
    for side in [1,2]:
        work = directory/('player%d'%side)
        if side==2:
            shutil.copytree(Path(opponent['runtime'])/'bwapi-data',work/'bwapi-data')
        else:
            (work/'bwapi-data').mkdir(parents=True)
        for sub in ['read','write','AI']:
            (work/'bwapi-data'/sub).mkdir(exist_ok=True)
        shutil.copyfile(PATHS['bwapi_ini'],work/'bwapi-data/bwapi.ini')
    entry = directory/'player2'/Path(opponent['entry']).relative_to(Path(opponent['runtime']))
    assert sha(entry)==record['binary_sha256']
    started = time.monotonic()
    try:
        for side in [1,2]:
            work = directory/('player%d'%side)
            player_env = env.copy()
            player_env.update({'BWAPI_CONFIG_AI__AI':str(BIN/'ProbeBaseline.dll') if side==1 else str(entry),
                               'BWAPI_CONFIG_AUTO_MENU__CHARACTER_NAME':'compat%d'%side,
                               'BWAPI_CONFIG_AUTO_MENU__SAVE_REPLAY':'game.rep'})
            log = (directory/('player%d.log'%side)).open('wb')
            logs.append(log)
            p = subprocess.Popen([str(BIN/'BWAPILauncher.exe')],cwd=work,env=player_env,stdout=log,stderr=subprocess.STDOUT,creationflags=0x08000000)
            processes.append(p)
            if side==1:
                time.sleep(0.6)
        while any(p.poll() is None for p in processes):
            if any(p.poll() not in (None,0) for p in processes):
                record['issues'].append('player_process_failed')
                break
            if time.monotonic()-started>TIMEOUT_SECONDS:
                record['issues'].append('wall_timeout_%ds_unresolved'%TIMEOUT_SECONDS)
                break
            time.sleep(0.2)
    finally:
        forced=[]
        for side,p in enumerate(processes,1):
            if p.poll() is None:
                forced.append(side); p.kill()
            p.wait()
        for log in logs:
            log.close()
        record.update(wall_seconds=round(time.monotonic()-started,3),exit_codes=[p.returncode for p in processes],
                      exit_codes_hex=[hex(p.returncode & 0xffffffff) for p in processes],forced_stop_sides=forced)
    traces = [events(directory/('player%d'%s)/'referee.jsonl') for s in [1,2]]
    starts = [[e for e in t if e['event']=='start'] for t in traces]
    ends = [[e for e in t if e['event']=='end'] for t in traces]
    caps = [[e for e in t if e['event']=='cap'] for t in traces]
    samples = [e for e in traces[1] if e['event']=='sample']
    active = any(e['frame']>0 and (e['workers']>4 or e['army']>0 or e['minerals']!=50) for e in samples)
    rawlog = (directory/'player2.log').read_text(errors='replace')
    problem_lines = [s.strip() for s in rawlog.splitlines() if re.search(r'!Assert:|BWTA map analysis failed|assertion failed|Failed to load|No module loaded|Error:',s,re.I)]
    record.update(starts=starts,ends=ends,caps=caps,opponent_active=active,problem_log_lines=problem_lines[:15],
                  activity={'max_workers':max([e['workers'] for e in samples] or [0]),'max_army':max([e['army'] for e in samples] or [0]),
                            'max_frame':max([e['frame'] for e in samples] or [0])})
    if problem_lines:
        record['issues'].append('opponent_error_or_assertion')
    if not all(starts):
        record['issues'].append('missing_start')
    if not active:
        record['issues'].append('opponent_not_observed_acting')
    if not all(ends):
        record['issues'].append('missing_end')
    if record['exit_codes']!=[0,0]:
        record['issues'].append('nonzero_or_forced_exit')
    replays = [directory/('player%d'%s)/'game.rep' for s in [1,2]]
    if not all(p.exists() and p.stat().st_size>0 for p in replays):
        record['issues'].append('missing_replay')
    if all(ends) and not any(caps) and ends[0][-1]['winner']==ends[1][-1]['winner']:
        record['issues'].append('noncomplementary_end')
    record['status']='issue_recorded' if record['issues'] else ('ran_frame_cap_unscored' if any(caps) else 'passed_natural')
    if not record['issues']:
        record['replay_checks']=[check_replay(p,directory/('replay-check-%d'%s)) for s,p in enumerate(replays,1)]
        if not all(c['passed'] for c in record['replay_checks']):
            record['issues'].append('replay_verification_failed'); record['status']='issue_recorded'
    record['replay_artifacts']=[{'path':str(p),'bytes':p.stat().st_size,'sha256':sha(p)} for p in replays if p.exists()]
    # Record cache diagnostic logs without repairing paths or creating aliases.
    for p in (directory/'player2/bwapi-data').rglob('*'):
        if p.is_file() and p.name.lower() in ['bwta.log']:
            record.setdefault('cache_logs',{})[str(p.relative_to(directory))]=p.read_text(errors='replace')[-3000:]
    save(directory/'result.json',record)
    print(json.dumps({'event':'match_done','opponent':name,'map':map_item['id'],'status':record['status'],'seconds':record['wall_seconds'],'issues':record['issues']}),flush=True)
    return record
