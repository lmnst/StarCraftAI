"""Fail before training continues if the recorded CLI context has wrong settings."""
import argparse
import json
from pathlib import Path


def complete_lines(path):
    # CLI writers can flush in the middle of a UTF-8 character or JSON record.
    # Only decode records whose terminating newline has reached disk.
    return [line.decode('utf-8-sig') for line in path.read_bytes().split(b'\n')[:-1]]


def check_session(trial, model, reasoning, min_turns=1, session_root=None):
    event_files = sorted(trial.glob('events-*.jsonl'), key=lambda p: int(p.stem.split('-')[-1]))
    session = None
    for path in event_files:
        for line in complete_lines(path):
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                continue  # The active writer may not have finished its last line.
            if event.get('type') == 'thread.started':
                session = event['thread_id']
                break
        if session:
            break
    if not session:
        return False
    root = session_root or Path.home()/'.codex/sessions'
    for path in root.rglob('*'+session+'.jsonl'):
        contexts = []
        for line in complete_lines(path):
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                continue
            if row.get('type') != 'turn_context':
                continue
            context = row['payload']
            contexts.append(context)
            actual = (context.get('model'), context.get('effort'), context.get('sandbox_policy', {}).get('type'))
            expected = (model, reasoning, 'workspace-write')
            if actual != expected:
                raise RuntimeError('Training context mismatch: expected %r, recorded %r' % (expected, actual))
            if context['sandbox_policy'].get('network_access') is not False:
                raise RuntimeError('Training shell network access must remain disabled')
            if Path(context['cwd']).resolve() != trial.resolve():
                raise RuntimeError('Training working directory does not match the trial')
            profile = context.get('permission_profile', {})
            fs = profile.get('file_system', {})
            writable = [entry['path']['path'] for entry in fs.get('entries', [])
                        if entry.get('access') == 'write' and entry.get('path', {}).get('type') == 'path']
            if (profile.get('type') != 'managed' or profile.get('network') != 'restricted'
                    or fs.get('type') != 'restricted'
                    or not any(Path(path).resolve() == trial.resolve() for path in writable)):
                raise RuntimeError('Training permission profile does not grant restricted workspace writes')
            if context.get('approvals_reviewer') != 'auto_review':
                raise RuntimeError('Training must use automatic approval review')
        return len(contexts) >= min_turns
    return False


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--trial', required=True, type=Path)
    parser.add_argument('--model', required=True)
    parser.add_argument('--reasoning', required=True)
    parser.add_argument('--min-turns', type=int, default=1)
    args = parser.parse_args()
    raise SystemExit(0 if check_session(args.trial, args.model, args.reasoning, args.min_turns) else 3)
