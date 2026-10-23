"""Pinned compiler adapter for CMake 4/Ninja drive-letter depfiles on Windows."""
from pathlib import Path
import os,subprocess,sys

def main():
    if sys.argv[1:]==['--wrapper']:
        cache=Path(os.environ['LOCALAPPDATA'])/'esp-mosaico/build-adapters/learning_assistant'
        cache.mkdir(parents=True,exist_ok=True)
        wrapper=cache/'gspc.cmd'
        wrapper.write_text('@echo off\n"'+sys.executable+'" "'+str(Path(__file__).resolve())+'" %*\n',encoding='utf-8')
        print(wrapper.as_posix());return 0
    explicit=os.environ.get('GSPC_REAL_EXECUTABLE')
    candidates=([Path(explicit)] if explicit else [])+[
        Path.home()/'.cache/esp-mosaico/gspc/0.5.0/gspc.exe',
        Path(os.environ['LOCALAPPDATA'])/'esp-mosaico/gspc/0.5.0/gspc.exe']
    compiler=next((p for p in candidates if p.is_file()),None)
    if compiler is None:raise SystemExit('Pinned GSPC 0.5.0 is required; resolve it with mosaico.py project sim first')
    version=subprocess.check_output([str(compiler),'--version'],text=True)
    if '0.5.0' not in version:raise SystemExit('GSPC 0.5.0 required')
    args=sys.argv[1:];result=subprocess.run([str(compiler),*args],check=False)
    if result.returncode:return result.returncode
    if '--depfile' in args:
        depfile=Path(args[args.index('--depfile')+1])
        if depfile.is_file():
            # Direct dependencies include scene/font/image inputs below. Do
            # not retain CMake-transformed /D:/ pseudo-paths in Ninja's DB.
            depfile.write_text(f'{depfile.stem}:\n',encoding='utf-8')
    return 0
if __name__=='__main__':raise SystemExit(main())
