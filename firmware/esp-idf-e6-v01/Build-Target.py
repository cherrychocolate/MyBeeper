"""Local compile/link only. Never flashes or opens a hardware device."""
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
SDK = Path('/tmp/id-card-esp-idf')
COMMIT = 'fff9895c82d744c7237be8847347bdd1b07c6643'

def main():
    commit = subprocess.check_output(['git', '-C', str(SDK), 'rev-parse', 'HEAD'], text=True).strip()
    if commit != COMMIT:
        raise RuntimeError('SDK commit does not match lock')
    modules = subprocess.check_output(['git', '-C', str(SDK), 'submodule', 'status', '--recursive'], text=True)
    (ROOT/'Submodule-Lock.txt').write_text(modules)
    if any(line and line[0] != ' ' for line in modules.splitlines()):
        raise RuntimeError('SDK submodules incomplete or differ from lock')
    env = os.environ.copy()
    env.update(IDF_PATH=str(SDK), IDF_TOOLS_PATH='/tmp/id-card-idf-tools',
               IDF_PYTHON_ENV_PATH='/tmp/id-card-idf-python')
    env['PATH']='/tmp/id-card-build-utils/bin:/opt/homebrew/opt/python@3.12/libexec/bin:'+env['PATH']
    with (ROOT/'SDK-Export.log').open('w') as log:
        exported = subprocess.run(['bash', '-c', 'source "$IDF_PATH/export.sh" >&2 && env -0'],
            env=env, stdout=subprocess.PIPE, stderr=log, check=True)
    for item in exported.stdout.split(b'\0'):
        if b'=' in item:
            k,v=item.split(b'=',1)
            env[k.decode()]=v.decode()
    results=[]
    for name,enabled in [('default',False),('bringup',True)]:
        defaults=ROOT/f'sdkconfig.{name}.defaults'
        defaults.write_text('CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y\n' + ('CONFIG_IDC_E6_RUN_ONCE=y\n' if enabled else '# CONFIG_IDC_E6_RUN_ONCE is not set\n'))
        build=ROOT/f'build-{name}'
        cmd=[env['IDF_PYTHON_ENV_PATH']+'/bin/python', str(SDK/'tools/idf.py'),
             '--preview','-B',str(build),'-DIDF_TARGET=esp32s31',
             '-DSDKCONFIG='+str(build/'sdkconfig'),'-DSDKCONFIG_DEFAULTS='+str(defaults),'build']
        with (ROOT/f'Target-Build-{name}.log').open('w') as log:
            result=subprocess.run(cmd,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT)
        results.append({'configuration':name,'physical_path_compiled':enabled,
                        'command':cmd,'exit_code':result.returncode})
        (ROOT/'Target-Build-Results.json').write_text(json.dumps({'sdk_commit':commit,'builds':results,
            'flashed':False,'hardware_tested':False},indent=2))
        if result.returncode:
            return result.returncode
    return 0

if __name__ == '__main__':
    sys.exit(main())
