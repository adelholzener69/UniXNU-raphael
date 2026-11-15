#!/usr/bin/env python3
# Add LC_LOAD_DYLIB /usr/local/lib/sweet/bl-hook.dylib to a staged backboardd (stock, or already staged for
# Chlorophyll by tools/raphael/chlorophyll/stage_backboardd.py) and re-sign it ad hoc, keeping identity and entitlements.
# usage: stage_backboardd_bl.py RENEGADE_DIR BACKBOARDD_IN BACKBOARDD_OUT
import os, shutil, subprocess, sys
ren, src, dst = sys.argv[1:4]
sys.path.insert(0, ren)
import root_tweaks18 as rt
import pacstrip
HOOK = '/usr/local/lib/sweet/bl-hook.dylib'
os.makedirs(os.path.dirname(dst), exist_ok=True)
shutil.copyfile(src, dst)
os.chmod(dst, 0o755)
ents, ident = rt.entitlements_of(dst), pacstrip.cs_identity(dst)
print('before: hook', rt.macho_has_dylib(dst, HOOK), 'ident', ident, 'ents', len(ents or {}))
if not rt.macho_has_dylib(dst, HOOK):
    subprocess.run([sys.executable, '-I', rt.ADDLC, dst, HOOK], check=True)
    pacstrip.sign(dst)
n_ents, n_ident = rt.entitlements_of(dst), pacstrip.cs_identity(dst)
assert n_ents == ents and n_ident[:2] == ident[:2], 'identity or entitlements changed'
assert rt.macho_has_dylib(dst, HOOK)
print('after: hook', rt.macho_has_dylib(dst, HOOK), 'ident', n_ident, 'ents', len(n_ents or {}))
