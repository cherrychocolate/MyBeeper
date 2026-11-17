"""Build only: uses fresh relocatable directories; never flashes hardware."""
import argparse,json,os,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--project',choices=['e6','hfp','journal','audio'],required=True);p.add_argument('--profile',default='default');p.add_argument('--print-only',action='store_true');a=p.parse_args()
base=root/'firmware'
layouts={'e6':('esp-idf-e6-v01',{'default':'sdkconfig.default.defaults','bringup':'sdkconfig.bringup.defaults'}),'hfp':('esp-hfp-build-v01',{'classic':'sdkconfig.classic.defaults','dual':'sdkconfig.dual.defaults'}),'journal':('esp-task-journal-v01',{'default':'sdkconfig.defaults'}),'audio':('esp-audio-pcm-v01',{'default':'sdkconfig.defaults','probe':'sdkconfig.probe.defaults'})}
folder,profiles=layouts[a.project]
if a.profile not in profiles:p.error('Choose profile: '+', '.join(profiles))
project=base/folder;defaults=project/profiles[a.profile];cwd=project/'project' if a.project=='hfp' else project
assert defaults.is_file() and (cwd/'CMakeLists.txt').is_file()
sdk=Path(os.environ.get('IDF_PATH','/tmp/id-card-esp-idf'));build=root/'.local-build'/a.project/a.profile
python=Path(os.environ.get('IDF_PYTHON_ENV_PATH','/tmp/id-card-idf-python'))/'bin/python'
cmd=[str(python),str(sdk/'tools/idf.py'),'--preview','-B',str(build),'-DIDF_TARGET=esp32s31','-DSDKCONFIG='+str(build/'sdkconfig'),'-DSDKCONFIG_DEFAULTS='+str(defaults),'build']
if a.print_only:print(json.dumps({'cwd':str(cwd),'command':cmd},indent=2));sys.exit(0)
if subprocess.check_output(['git','-C',str(sdk),'rev-parse','HEAD'],text=True).strip()!='fff9895c82d744c7237be8847347bdd1b07c6643':raise SystemExit('SDK commit mismatch; use tools/bootstrap_idf.sh')
mods=subprocess.check_output(['git','-C',str(sdk),'submodule','status','--recursive'],text=True)
if any(x and not x.startswith(' ') for x in mods.splitlines()):raise SystemExit('SDK submodule lock mismatch')
env=os.environ.copy();env.update(IDF_PATH=str(sdk),IDF_TOOLS_PATH='/tmp/id-card-idf-tools',IDF_PYTHON_ENV_PATH=str(python.parents[1]));env['PATH']='/tmp/id-card-build-utils/bin:'+env['PATH']
shell=subprocess.run(['bash','-c','source "$IDF_PATH/export.sh" >&2 && env -0'],env=env,stdout=subprocess.PIPE,check=True)
for entry in shell.stdout.split(b'\0'):
 if b'=' in entry:
  k,v=entry.split(b'=',1);env[k.decode()]=v.decode()
sys.exit(subprocess.run(cmd,cwd=cwd,env=env).returncode)
